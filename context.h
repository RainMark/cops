#pragma once

#include <memory>
#include <functional>
#include <stdint.h>

namespace cops {
class coro_t;
}

using context = void*;

extern "C"
context switch_context(cops::coro_t* to, context ctx);
extern "C"
context make_context(context sp, void* entry);

namespace cops {

class stack_t {
public:
  stack_t(size_t size = 64 * 1024) : size_(size) {
    sp_ = new char[size_];
  }
  ~stack_t() {
    delete[] sp_;
  }

  void* sp() {
    return sp_ + size_ - 1;
  }

private:
  char* sp_;
  size_t size_;
};

class coro_t {
public:
  using Fn = std::function<void(void)>;

  explicit coro_t(Fn&& fn)
    : fn_(std::forward<Fn>(fn)) {
  }
  ~coro_t() {
  }

  void switch_out();
  void switch_in();

public:
  context ctx_;
  coro_t* next_;
  stack_t stack_;
  Fn fn_;
};

extern coro_t* current;
void main(coro_t* coro, context ctx);

template <class Fn>
std::unique_ptr<coro_t> make_coro(Fn&& fn) {
  auto coro = std::make_unique<coro_t>(std::forward<Fn>(fn));
  coro->ctx_ = make_context(coro->stack_.sp(), (void*)main);
  return coro;
}

} // cops
