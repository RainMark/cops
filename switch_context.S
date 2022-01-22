.file "switch_context.S"
.text
.globl switch_context
.type switch_context,@function
.align 16
switch_context:
    leaq  -56(%rsp), %rsp
    movq  %rdi, (%rsp)    /* save suspended coro_t* */

    movq  %r12, 8(%rsp)
    movq  %r13, 16(%rsp)
    movq  %r14, 24(%rsp)
    movq  %r15, 32(%rsp)
    movq  %rbx, 40(%rsp)
    movq  %rbp, 48(%rsp)

    movq  %rsp, %rax     /* return suspended context */

    movq  %rsi, %rsp     /* restore context */
    movq  56(%rsp), %r8  /* return point */

    movq  8(%rsp), %r12
    movq  16(%rsp), %r13
    movq  24(%rsp), %r14
    movq  32(%rsp), %r15
    movq  40(%rsp), %rbx
    movq  48(%rsp), %rbp

    leaq  64(%rsp), %rsp

    movq  %rax, %rsi    /* for main(coro_t*, context) 2rd argument */

    jmp  *%r8
.size switch_context,.-switch_context
.section .note.GNU-stack,"",%progbits

.text
.globl make_context
.type make_context,@function
.align 16
make_context:
    andq  $-16, %rdi      /* 16 bytes align for movdqa */
    leaq  -56(%rdi), %rdi
    movq  %rsi, 56(%rdi)  /* set entry */
    movq  %rdi, %rax      /* return updated context */
    retq
.size make_context,.-make_context
.section .note.GNU-stack,"",%progbits
