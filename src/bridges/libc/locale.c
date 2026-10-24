#include <errno.h>

#include "locale.h"

locale_t bridge_newlocale(int mask, const char* name, locale_t base)
{
    static const int categories[] = {
        LC_CTYPE, LC_NUMERIC, LC_TIME, LC_COLLATE, LC_MONETARY, LC_MESSAGES,
    };

    if(mask & ~63) {
        errno = EINVAL;
        return (locale_t)0;
    }

    int native = 0;
    for(unsigned index = 0; index < sizeof(categories) / sizeof(categories[0]); ++index) {
        if(mask & (1 << index)) {
            native |= 1 << categories[index];
        }
    }

    return newlocale(native, name, base);
}
