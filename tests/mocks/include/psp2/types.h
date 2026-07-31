#ifndef __TESTS_MOCKS_PSP2_TYPES_H
#define __TESTS_MOCKS_PSP2_TYPES_H
#include <stdint.h>
typedef int32_t SceUID;
typedef uint32_t SceSize;
typedef uint64_t SceUInt64;
typedef struct SceDateTime {
    uint16_t year, month, day, hour, minute, second;
    uint32_t microsecond;
} SceDateTime;

#endif
