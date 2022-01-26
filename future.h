#pragma once

#include <atomic>
#include <memory>
#include <functional>

namespace cops {

template <class T>
class future_t {
public:
    enum class status_t {
        kInit = 0,
        kHasValue,
        kHasCallback,
        kDone,
    };

    using Fn = std::function<void(void)>;

public:
    future_t() = default;
    ~future_t() = default;

    void set_value(T&& value) {
        auto s = status_.load(std::memory_order_acquire);
        if (s == status_t::kHasValue || s == status_t::kDone) {
            return;
        }
        value_ = std::move(value);
        if (s == status_t::kHasCallback) {
            invoke();
            return;
        }
        s = status_.exchange(status_t::kHasValue, std::memory_order_acq_rel);
        // double check, maybe exchange after set_callback
        if (s == status_t::kHasCallback) {
            invoke();
        }
    }

    T&& value() {
        return std::move(value_);
    }

    bool has_value() {
        return status_t::kHasValue == status_.load(std::memory_order_acquire);
    }

    void set_callback(Fn&& fn) {
        auto s = status_.load(std::memory_order_acquire);
        if (s == status_t::kHasCallback || s == status_t::kDone) {
            return;
        }
        callback_ = std::forward<Fn>(fn);
        if (s == status_t::kHasValue) {
            invoke();
            return;
        }
        s = status_.exchange(status_t::kHasCallback, std::memory_order_acq_rel);
        // double check, maybe exchange after set_value
        if (s == status_t::kHasValue) {
            invoke();
        }
    }

private:
    void set_done() {
        status_.store(status_t::kDone, std::memory_order_release);
    }
    void invoke() {
        set_done();
        callback_();
    }

private:
    T value_;
    std::atomic<status_t> status_ = status_t::kInit;
    Fn callback_;
};

} // cops
