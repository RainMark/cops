#include "coroutine.h"
#include "event_loop.h"

namespace cops{

static coro_t def([](){});
coro_t* current = &def;

void main(coro_t* coro, context from) {
  coro->next_->ctx_ = from;
  coro->fn_();
  coro->fut_.set_value(0);
  coro->switch_out();
}

void coro_t::switch_out() {
  current = next_;
  context from = switch_context(next_, next_->ctx_);
  next_ = static_cast<coro_t*>(from);
  next_->ctx_ = from;
}

void coro_t::switch_in() {
  next_ = current;
  current = this;
  ctx_ = switch_context(this, ctx_);
}

void coro_t::detach(std::unique_ptr<coro_t>& self, event_loop_t* loop) {
  // make sure destruction after coro execute over
  fut_.set_callback([loop, self = self.release()]() {
    loop->call_soon([self]() { delete self; });
  });
}

}
