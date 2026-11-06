/* i386 C-only nonlocal jump ABI. No SEH or C++ destructor unwinding.
 * Used by parser validation scopes, never as an SEH replacement. */
    .text
    .globl _rp_setjmp
    .def _rp_setjmp; .scl 2; .type 32; .endef
_rp_setjmp:
    movl 4(%esp), %edx
    movl %ebp, 0(%edx)
    movl %ebx, 4(%edx)
    movl %edi, 8(%edx)
    movl %esi, 12(%edx)
    movl %esp, 16(%edx)
    movl 0(%esp), %eax
    movl %eax, 20(%edx)
    xorl %eax, %eax
    ret
    .globl _rp_longjmp
    .def _rp_longjmp; .scl 2; .type 32; .endef
_rp_longjmp:
    movl 4(%esp), %edx
    movl 8(%esp), %eax
    testl %eax, %eax
    jne 1f
    incl %eax
1:
    movl 0(%edx), %ebp
    movl 4(%edx), %ebx
    movl 8(%edx), %edi
    movl 12(%edx), %esi
    movl 16(%edx), %esp
    addl $4, %esp
    jmp *20(%edx)
