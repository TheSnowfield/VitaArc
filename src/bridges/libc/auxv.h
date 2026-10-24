#ifndef VITAARC_LIBC_AUXV_H
#define VITAARC_LIBC_AUXV_H

unsigned long bridge_getauxval(unsigned long type);
char* bridge_getenv(const char* name);

#endif
