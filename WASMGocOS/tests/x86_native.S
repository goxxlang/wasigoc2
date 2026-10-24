# Runs one code snippet on the host CPU from a given register state and
# saves the state after it: the reference the x86 engine is checked against.
#
#   void x86_native_run(uint64_t* ctx /* rcx: gpr[16], rflags */, void* code);
#
# rsp is not loaded from ctx (the snippet runs on this stack); every other
# general register and RFLAGS is. The snippet ends in `ret`.

        .intel_syntax noprefix
        .text
        .globl  x86_native_run
x86_native_run:
        push    rbx
        push    rbp
        push    rsi
        push    rdi
        push    r12
        push    r13
        push    r14
        push    r15
        sub     rsp, 8                  # keep 16-byte alignment after the pushes
        mov     [rsp], rcx              # ctx
        mov     r11, rdx                # code
        push    qword ptr [rcx + 128]
        popfq
        mov     rax, [rcx + 0]
        mov     rdx, [rcx + 16]
        mov     rbx, [rcx + 24]
        mov     rbp, [rcx + 40]
        mov     rsi, [rcx + 48]
        mov     rdi, [rcx + 56]
        mov     r8,  [rcx + 64]
        mov     r9,  [rcx + 72]
        mov     r10, [rcx + 80]
        mov     r12, [rcx + 96]
        mov     r13, [rcx + 104]
        mov     r14, [rcx + 112]
        mov     r15, [rcx + 120]
        mov     rcx, [rcx + 8]
        call    r11
        pushfq
        xchg    rcx, [rsp + 8]          # rcx = ctx, the slot keeps the snippet's rcx
        mov     [rcx + 0], rax
        mov     [rcx + 16], rdx
        mov     [rcx + 24], rbx
        mov     [rcx + 40], rbp
        mov     [rcx + 48], rsi
        mov     [rcx + 56], rdi
        mov     [rcx + 64], r8
        mov     [rcx + 72], r9
        mov     [rcx + 80], r10
        mov     [rcx + 88], r11
        mov     [rcx + 96], r12
        mov     [rcx + 104], r13
        mov     [rcx + 112], r14
        mov     [rcx + 120], r15
        pop     rax                     # rflags
        mov     [rcx + 128], rax
        pop     rax                     # the snippet's rcx
        mov     [rcx + 8], rax
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     rdi
        pop     rsi
        pop     rbp
        pop     rbx
        ret
