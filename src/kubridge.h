#ifndef __KUBRIDGE_H
#define __KUBRIDGE_H

#include <stddef.h>

int kuKernelCpuUnrestrictedMemset(void *destination, int value,
                                 size_t length);

#endif /* __KUBRIDGE_H */
