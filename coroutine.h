#pragma once

#include <memory>
#include <functional>
#include <stdint.h>

#include <future.h>

#ifdef SPLIT_STACK

///////////////////////////
// forward declaration for splitstack-functions defined in libgcc
// https://github.com/gcc-mirror/gcc/blob/master/libgcc/generic-morestack.c
// https://github.com/gcc-mirror/gcc/blob/master/libgcc/config/i386/morestack.S

enum splitstack_context_offsets {
  MORESTACK_SEGMENTS = 0,
  CURRENT_SEGMENT = 1,
  CURRENT_STACK = 2,
  STACK_GUARD = 3,
  INITIAL_SP = 4,
  INITIAL_SP_LEN = 5,
  BLOCK_SIGNALS = 6,

  NUMBER_OFFSETS = 10
};

extern "C" {
  void *__splitstack_makecontext(std::size_t, void* [NUMBER_OFFSETS], std::size_t*);
  void __splitstack_setcontext(void* [NUMBER_OFFSETS]);
  void __splitstack_getcontext(void* [NUMBER_OFFSETS]);
  void __splitstack_releasecontext(void* [NUMBER_OFFSETS]);
  void __splitstack_resetcontext(void* [NUMBER_OFFSETS]);
  void __splitstack_block_signals_context(void* [NUMBER_OFFSETS], int* new_value, int* old_value);
}

#endif

namespace cops {
class coro_t;
class event_loop_t;
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
#ifdef SPLIT_STACK
    sp_ = static_cast<char*>(__splitstack_makecontext(size_, ss_ctx_, &size_));
    static int off = 0;
    __splitstack_block_signals_context(ss_ctx_, &off, 0);
#else
    sp_ = new char[size_];
#endif
  }
  ~stack_t() {
#ifdef SPLIT_STACK
    __splitstack_releasecontext(ss_ctx_);
#else
    delete[] sp_;
#endif
  }

  void* sp() {
    return sp_ + size_ - 1;
  }

private:
  char* sp_;
  size_t size_;
#ifdef SPLIT_STACK
public:
  using split_stack_context = void* [NUMBER_OFFSETS];
  split_stack_context ss_ctx_{0};
  split_stack_context next_{0};
#endif
};

#ifdef SPLIT_STACK
inline void switch_split_stack_context(stack_t::split_stack_context from,
                                       stack_t::split_stack_context to) {
  __splitstack_getcontext(from);
  __splitstack_setcontext(to);
}
#endif

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
  void detach(std::unique_ptr<coro_t>& self, event_loop_t* loop);

public:
  context ctx_;
  coro_t* next_;
  stack_t stack_;
  Fn fn_;
  future_t<int> fut_;
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
