#ifndef VITAARC_LIBC_LOCALE_H
#define VITAARC_LIBC_LOCALE_H

#include <locale.h>

locale_t bridge_newlocale(int mask, const char* name, locale_t base);

#endif
