#pragma once

#include <deque>
#include <memory>
#include <string>

#include <assert.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "coroutine.h"

namespace cops {

class task_t {
public:
    virtual ~task_t() = default;
    virtual void run() = 0;
};

template <class T>
class lambda_task_t : public task_t {
public:
    explicit lambda_task_t(T&& task) : task_(std::forward<T>(task)) {}
    void run() override {
        task_();
    }
private:
    T task_;
};

template <class T>
inline std::unique_ptr<task_t> make_lambda_task(T&& lambda) {
    return std::unique_ptr<task_t>(new lambda_task_t(std::forward<T>(lambda)));
}

inline void oops(const std::string& s) {
    perror(s.data());
    exit(1);
}

class event_loop_t {
public:
    struct epoll_data_t {
        int fd;
        std::unique_ptr<task_t> task;
    };
    static constexpr int kMaxEpollEvents = 128;

public:
    event_loop_t() = default;
    ~event_loop_t() {
        if (!closed_) {
            stop();
        }
    }

    template <class Fn>
    void call_soon(Fn&& fn) {
        task_queue_.push_back(make_lambda_task(std::forward<Fn>(fn)).release());
    }

    template <class Fn>
    void create_task(Fn&& fn) {
        auto coro = make_coro(std::forward<Fn>(fn));
        call_soon([coro = coro.get()]() { coro->switch_in(); });
        coro->detach(coro, this);
        assert(coro.get() == nullptr);
    }

    int create_server(const std::string& host, int16_t port) {
        int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (sockfd == 0) {
            oops("socket() " + std::to_string(errno));
        }
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            oops("setsockopt() " + std::to_string(errno));
        }
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        inet_pton(AF_INET, host.data(), &(sa.sin_addr));
        sa.sin_port = htons(port);
        if (bind(sockfd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
            oops("bind() " + std::to_string(errno));
        }
        int backlog = 4096;
        if (listen(sockfd, backlog) < 0) {
            oops("listen() " + std::to_string(errno));
        }
        return sockfd;
    }

    int sock_accept(int sockfd) {
        struct sockaddr_in sa;
        socklen_t len = sizeof(sa);
        int fd = accept4(sockfd, (struct sockaddr*)&sa, (socklen_t*)&len, SOCK_NONBLOCK);
        // TODO: return std::tuple<fd, sa>
        if (fd >= 0) {
            return fd;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            auto curr = current;
            file_desc_add(sockfd, EPOLLIN, [curr]() { curr->switch_in(); });
            curr->switch_out();
        }
        return accept4(sockfd, (struct sockaddr*)&sa, (socklen_t*)&len, SOCK_NONBLOCK);
    }

    ssize_t sock_recv(int sockfd, void* buf, size_t len, int flags) {
        ssize_t n = recv(sockfd, buf, len, flags);
        if (n >= 0) {
            return n;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            auto curr = current;
            file_desc_add(sockfd, EPOLLIN, [curr]() { curr->switch_in(); });
            curr->switch_out();
        }
        return recv(sockfd, buf, len, flags);
    }

    ssize_t sock_send(int sockfd, void* buf, size_t len, int flags) {
        ssize_t n = send(sockfd, buf, len, flags);
        if (n >= 0) {
            return n;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            auto curr = current;
            file_desc_add(sockfd, EPOLLOUT, [curr]() { curr->switch_in(); });
            curr->switch_out();
        }
        return send(sockfd, buf, len, flags);
    }

public:
    void run_forever() {
        epollfd_ = epoll_create(kMaxEpollEvents);
        if (epollfd_ < 0) {
            oops("epoll_create() " + std::to_string(errno));
        }
        eventfd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (eventfd_ < 0) {
            oops("eventfd() " + std::to_string(errno));
        }
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = nullptr;
        if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, eventfd_, &ev) < 0) {
            oops("epoll_ctl() " + std::to_string(errno));
        }

        closed_ = false;
        while (!closed_) {
            int timeout = task_queue_.empty() ? -1 : 0;
            struct epoll_event events[kMaxEpollEvents];
            int n = epoll_wait(epollfd_, events, kMaxEpollEvents, timeout);
            if (n < 0) {
                oops("epoll_wait() " + std::to_string(errno));
            }
            for (int i = 0; i < n; ++i) {
                auto data = static_cast<epoll_data_t*>(events[i].data.ptr);
                // response wakeup
                if (!data) {
                    static char _unused[8];
                    read(eventfd_, _unused, 8);
                    continue;
                }
                // invoke io callback
                file_desc_del(data->fd);
                data->task->run();
                delete data;
            }
            // schedule normal task
            if (!task_queue_.empty()) {
                std::unique_ptr<task_t> task(task_queue_.front());
                task_queue_.pop_front();
                task->run();
            }
        }
        file_desc_del(eventfd_);
        close(eventfd_);
        close(epollfd_);
        eventfd_ = -1;
        epollfd_ = -1;
    }
    void stop() {
        closed_ = true;
    }

private:
    void wakeup() {
        if (eventfd_ > 0) {
            static uint64_t one = 1;
            write(eventfd_, &one, sizeof(one));
        }
    }

    template <class Callback>
    void file_desc_add(int fd, int flags, Callback callback) {
        struct epoll_event ev;
        ev.events = flags;
        auto task = make_lambda_task(std::forward<Callback>(callback));
        ev.data.ptr = new epoll_data_t{fd, std::move(task)};
        if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            oops("epoll_ctl() " + std::to_string(errno));
        }
    }

    void file_desc_del(int fd) {
        if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            oops("epoll_ctl() " + std::to_string(errno));
        }
    }

private:
    bool closed_ = true;
    int epollfd_ = -1;
    int eventfd_ = -1;
    std::deque<task_t*> task_queue_;
};

} // cops
