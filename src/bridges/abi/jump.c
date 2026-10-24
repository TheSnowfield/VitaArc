#include <libc/signals.h>

/* Android ARM32 provides 256 bytes for jmp_buf. Save base-PCS core and VFP
 * registers directly; the SDK's setjmp only preserves core registers.
 * Vita has no process signal mask or asynchronous POSIX signal delivery. */
__attribute__((naked, returns_twice)) int bridge_sigsetjmp(void* state, int mask)
{
    __asm__ volatile(
        "mov ip, sp\n"
        "stmia r0!, {r4-r11, ip, lr}\n"
        "vstmia r0!, {d8-d15}\n"
        "vmrs r2, fpscr\n"
        "str r2, [r0]\n"
        "movs r0, #0\n"
        "bx lr\n");
}

__attribute__((naked, noreturn)) void bridge_siglongjmp(void* state, int value)
{
    __asm__ volatile(
        "ldmia r0!, {r4-r11, ip, lr}\n"
        "vldmia r0!, {d8-d15}\n"
        "ldr r2, [r0]\n"
        "vmsr fpscr, r2\n"
        "mov sp, ip\n"
        "movs r0, r1\n"
        "it eq\n"
        "moveq r0, #1\n"
        "bx lr\n");
}
