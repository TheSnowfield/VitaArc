#include <string.h>
#include <kubridge.h>
#include <loader.h>
#include <logcat/logcat.h>
#include "kuser.h"

/* Linux's kuser ABI preserves r1/r2 as well as the AAPCS callee-saved
 * registers. Carry indicates success, independently of the nonzero failure
 * result. Plain C atomics do not provide that register/flags contract. */
__attribute__((naked)) int bridge_arm_kuser_cmpxchg(
    uint32_t old_value, uint32_t new_value, volatile uint32_t* destination)
{
    __asm__ volatile("dmb ish\n"
                     "1: ldrex r3, [r2]\n"
                     "cmp r3, r0\n"
                     "bne 2f\n"
                     "strex ip, r1, [r2]\n"
                     "cmp ip, #0\n"
                     "bne 1b\n"
                     "movs r0, #0\n"
                     "cmp r0, #0\n" // Carry set on success.
                     "dmb ish\n"
                     "bx lr\n"
                     "2: clrex\n"
                     "movs r0, #1\n"
                     "cmp r0, #2\n" // Carry clear on failure.
                     "dmb ish\n"
                     "bx lr\n");
}

__attribute__((naked)) void bridge_arm_kuser_memory_barrier(void)
{
    __asm__ volatile("dmb ish\n"
                     "bx lr\n");
}

static uint64_t __fingerprint(const void* address, uint32_t size)
{
    const uint8_t* bytes = address;
    uint64_t hash = UINT64_C(14695981039346656037);
    for(uint32_t index = 0; index < size; ++index) {
        hash = (hash ^ bytes[index]) * UINT64_C(1099511628211);
    }

    return hash;
}

static bool __executable_range(const dynalib_t* library, uintptr_t address, uint32_t size)
{
    for(uint32_t index = 0; index < library->header->e_phnum; ++index) {
        const Elf32_Phdr* segment = &library->phdr[index];
        uintptr_t start = library->base + segment->p_vaddr;
        if(segment->p_type == PT_LOAD && (segment->p_flags & PF_X) && address >= start &&
            address - start <= segment->p_filesz && size <= segment->p_filesz - (address - start)) {
            return true;
        }
    }

    return false;
}

static bool __is_helper(uint32_t value)
{
    return value == UINT32_C(0xffff0fc0) || value == UINT32_C(0xffff0fa0) || value == UINT32_C(0xffff0f60) ||
        value == UINT32_C(0xffff0fe0) || value == UINT32_C(0xffff0ffc);
}

bool bridge_patch_arm_kuser(dynalib_t* library)
{
    uint32_t count = 0;
    for(uint32_t index = 0; index < library->header->e_phnum; ++index) {
        const Elf32_Phdr* segment = &library->phdr[index];
        if(segment->p_type != PT_LOAD || !(segment->p_flags & PF_X)) {
            continue;
        }

        uintptr_t start = library->base + segment->p_vaddr;
        for(uint32_t offset = (uint32_t)(-start & 3); offset + 4 <= segment->p_filesz; offset += 4) {
            uint32_t value = *(const uint32_t*)(start + offset);
            count += __is_helper(value);
        }
    }

    if(!count) {
        return true;
    }

    /* The audited libgcc groups contain code and literal pools, with no
     * relocations. Verify the entire group before replacing its 48 helper
     * literals; a matching integer elsewhere is not sufficient patch evidence. */
    uintptr_t anchor = (uintptr_t)loader_get_proc_address(library, "__gnu_uldivmod_helper");
    const uint32_t anchor_size = 60;
    uint32_t group_size = 0;
    uint64_t anchor_hash = anchor && !(anchor & 3) && __executable_range(library, anchor, anchor_size)
        ? __fingerprint((void*)anchor, anchor_size)
        : 0;
    if(anchor_hash == UINT64_C(0x3aec7fb812332b7b))
        group_size = 0xef0; /* 1.6.1c */
    if(anchor_hash == UINT64_C(0x3de61c19ff6cb59d))
        group_size = 0xf68; /* 1.9.0c */
    uintptr_t group = anchor + anchor_size;
    if(count != 48 || !group_size || !__executable_range(library, anchor, anchor_size + group_size) ||
        __fingerprint((void*)group, group_size) !=
            (group_size == 0xef0 ? UINT64_C(0x05bd792528c1b20f) : UINT64_C(0x767f46830cd20206))) {
        log_e(TAG, "Unsupported ARM Linux kuser helper layout in %s", library->name);
        return false;
    }

    uint32_t compares = 0, barriers = 0;
    for(uint32_t offset = 0; offset < group_size; offset += 4) {
        uint32_t value;
        memcpy(&value, (void*)(group + offset), sizeof(value));
        compares += value == UINT32_C(0xffff0fc0);
        barriers += value == UINT32_C(0xffff0fa0);
    }

    if(compares != 43 || barriers != 5) {
        return false;
    }

    for(uint32_t offset = 0; offset < group_size; offset += 4) {
        uint32_t value;
        memcpy(&value, (void*)(group + offset), sizeof(value));
        switch(value) {
            case UINT32_C(0xffff0fc0):
                value = (uintptr_t)bridge_arm_kuser_cmpxchg;
                break;

            case UINT32_C(0xffff0fa0):
                value = (uintptr_t)bridge_arm_kuser_memory_barrier;
                break;

            default: continue;
        }

        if(kuKernelCpuUnrestrictedMemcpy((void*)(group + offset), &value, sizeof(value)) < 0) {
            log_e(TAG, "Could not patch ARM kuser helper in %s", library->name);
            return false;
        }
    }

    log_i(TAG, "Patched %u ARM compare-exchange and %u memory-barrier helper literals", compares, barriers);
    return true;
}
