#include <context.h>
#include <iostream>

namespace cops{

static coro_t def([](){});
coro_t* current = &def;

void main(coro_t* coro, context from) {
  coro->next_->ctx_ = from;
  coro->fn_();
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

}
