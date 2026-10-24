#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/rng.h>

#include "auxv.h"

static pthread_once_t __once = PTHREAD_ONCE_INIT;
static unsigned char __random[16];
static int __random_error;

char* bridge_getenv(const char* name)
{
    /* OpenSSL's SIGILL-based probe cannot run on the Vita kernel. Its own
     * capability override selects only the Cortex-A9 NEON implementation. */
    static char neon[] = "1";
    if(name && strcmp(name, "OPENSSL_armcap") == 0) {
        return neon;
    }

    return getenv(name);
}

static void __seed(void)
{
    __random_error = sceKernelGetRandomNumber(__random, sizeof(__random)) < 0;
}

unsigned long bridge_getauxval(unsigned long type)
{
    switch(type) {
        case 6: return 4096;
        case 11:
        case 12:
        case 13:
        case 14: return 0;

        /* Cortex-A9: VFPv3-D32 and NEON, without ARMv8 crypto or integer divide. */
        case 16: return 0x8b0d6;
        case 23:
        case 26: return 0;

        case 15: return (uintptr_t)"v7l";
        case 25:
            pthread_once(&__once, __seed);
            if(__random_error) {
                errno = EIO;
                return 0;
            }

            return (uintptr_t)__random;
    }

    errno = ENOENT;
    return 0;
}
