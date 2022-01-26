#include <iostream>
#include "coroutine.h"
#include "event_loop.h"

int main() {
    auto loop = std::make_unique<cops::event_loop_t>();
    loop->call_soon([&loop]() {
        std::cout << "task1" << std::endl;
        loop->call_soon([&loop]() {
            std::cout << "task2" << std::endl;
        });
    });

    loop->create_task([&loop]() {
        int server = loop->create_server("127.0.0.1", 9999);
        std::cout << "create server " << server << std::endl;
        while (1) {
            int conn = loop->sock_accept(server);
            std::cout << "server " << server << " new conn " << conn << std::endl;
            loop->create_task([&loop, conn]() {
                char buf[128];
                ssize_t n = loop->sock_recv(conn, buf, 128, 0);
                std::cout << buf << std::endl;
                n = loop->sock_send(conn, buf, n, 0);
                close(conn);
            });
        }
    });

    loop->create_task([&loop]() {
        int server = loop->create_server("127.0.0.1", 9998);
        std::cout << "create server " << server << std::endl;
        while (1) {
            int conn = loop->sock_accept(server);
            std::cout << "server " << server << " new conn " << conn << std::endl;
            loop->create_task([&loop, conn]() {
                char buf[128];
                ssize_t n = loop->sock_recv(conn, buf, 128, 0);
                std::cout << buf << std::endl;
                n = loop->sock_send(conn, buf, n, 0);
                close(conn);
            });
        }
    });
    loop->run_forever();
    return 0;
}
