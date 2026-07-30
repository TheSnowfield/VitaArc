# ELF shared-object loader

## Files and public API

The loader state type is `dynalib_t`, declared independently in `src/dynalib.h`.
The implementation and public declarations are in `src/loader.c` and
`src/loader.h`.

Public functions:

- `loader_load_library(const char *library_path)`
- `loader_init_library(dynalib_t *library)`
- `loader_free_library(dynalib_t *library)`
- `loader_get_proc_address(dynalib_t *library, const char *symbol_name)`
- `loader_get_library_image_base(dynalib_t *library)`
- `loader_find_library(const char *library_name)`
- `loader_clone_handle(dynalib_t *library)`
- `loader_find_empty_slot(void)`
- `loader_load_sections(dynalib_t *library)`
- `loader_debug_print_elf_table(dynalib_t *library)`
- `loader_symbol_ref(dynalib_t *library)`

The old `HSOLIB`, `SOINTERNAL`, `LPSOINTERNAL` and `solib_*` interfaces no
longer exist.

## `dynalib_t`

The structure contains:

- `ref_count`
- `slot_index`
- `library_name[64]`
- `library_path[128]`
- original ELF file image pointer `library_image_base`
- original image memblock ID `image_mem_block`
- ELF header, program-header and section-header pointers
- dynamic symbol section metadata
- `.init_array` section pointer
- section-name table
- dynamic symbol and string tables
- dynamic symbol count
- executable segment base `text_base`

The loader has 32 process-global slots:

```c
#define MAX_LIBRARY 32
static uint32_t __library_loaded;
static dynalib_t *__library_slots[MAX_LIBRARY];
```

Library identity is the basename, not the complete path.

## Load flow

`loader_load_library`:

1. Uses `util_file_exists` and `util_get_file_size`.
2. Extracts the basename with `util_get_file_name`.
3. Reuses a library with the same basename, resolves its symbols again, and
   increments its reference count.
4. Allocates a zeroed `dynalib_t`.
5. Allocates a page-aligned user RW memblock for the complete source ELF.
6. Opens and reads the ELF into that memblock.
7. Reserves a loader slot and copies the name and path.
8. Prints ELF diagnostics.
9. Calls `loader_load_sections`; failure now rolls back the slot, original
   image memblock, and state allocation.
10. Calls `loader_symbol_ref` to bind undefined imports against the generated
    local bridge table.
11. Returns the same state pointer through `loader_clone_handle`.

Remaining unchecked operations include the `sceIoOpen`, `sceIoRead`,
`sceKernelGetMemBlockBase` results and actual read length. The two `strcpy`
calls have no length guard.

## Fixed runtime mapping

Every loaded ELF currently uses:

```c
uintptr_t linear_address_base = 0x98000000;
```

Only one distinct shared object can therefore be mapped safely. Although the
slot model accepts 32 libraries, a second distinct ELF will attempt to occupy
the same virtual range.

## `PT_LOAD` mapping

For every `PT_LOAD` program header:

1. Compute the gap from the end of the previous mapped block.
2. Align `p_memsz + gap` to `p_align`.
3. Request a fixed-address memblock using
   `SceKernelAllocMemBlockKernelOpt.field_C`.
4. Use `kuKernelAllocMemBlock` with user RX for `PF_X`, otherwise user RW.
5. Add `0x98000000` directly to the program header's `p_vaddr`.
6. Save the executable segment address as `text_base`.
7. Zero the allocation with `kuKernelCpuUnrestrictedMemset`.
8. Copy `p_filesz` bytes from the source image with
   `kuKernelCpuUnrestrictedMemcpy`.

The allocated segment memblock IDs are local variables and are not retained in
`dynalib_t`; `loader_free_library` cannot release those mapped segments.

## Sections and relocations

The source ELF's section table is scanned for:

- `.dynsym`
- `.dynstr`
- `.init_array`
- `.rel.dyn`
- `.rel.plt`

Missing `.dynsym` or `.dynstr` makes `loader_load_sections` fail. Missing
`.init_array` is accepted during loading but later dereferenced by
`loader_init_library`.

Supported ARM relocation types:

- `R_ARM_ABS32`
- `R_ARM_RELATIVE`
- `R_ARM_GLOB_DAT`
- `R_ARM_JUMP_SLOT`

For a defined symbol, the runtime symbol address is
`0x98000000 + st_value`. Undefined symbols initially resolve to zero for the
ABS/GLOB/JUMP cases and are handled by the later local-symbol pass. Unknown
relocation types are logged and loading continues.

`OFFRST(x)` is currently `(x)`. The commented alternative clears the Thumb bit,
but is not active.

Relocation targets are computed as `0x98000000 + r_offset`. The relocation
tables themselves are addressed as `library_image_base + section.sh_addr`,
not `sh_offset`; this depends on the target ELF layout.

## Automatic local-symbol binding

`loader.c` directly includes `generated/bridge_symbols.h`. The generated array
is lexically sorted.

`loader_symbol_ref` derives its length with `sizeof` and passes it to static
`__resolve_symbols`. That routine:

1. Scans `.rel.dyn` and `.rel.plt`.
2. Rejects invalid symbol indices.
3. Processes only symbols whose `st_shndx == SHN_UNDEF`.
4. Uses static `__find_bridge_symbol` for binary search.
5. Writes `patch_func_t.bridge_proc` directly to the relocation target.

It runs at the end of a new load and again when a loaded library is reused.
There is no separate `bridge_symbol_count`, generated `.c` file,
`patch_bridge_symbols`, or public `patch_symbols` routine.

See `symbol-resolution.md` for the definition/generation pipeline.

## Initialization and symbol lookup

`loader_init_library` calculates the init table as:

```c
internal->text_base + internal->elf_init_array_section->sh_addr
```

It walks `sh_size / 4` entries and calls non-null functions. There is no null
check for the `.init_array` section.

`loader_get_proc_address` performs a linear search over `.dynsym` and returns
`0x98000000 + st_value` on a name match. It does not reject undefined symbols
or inspect binding/type.

## Lifetime

`loader_clone_handle` increments `ref_count` and returns the same pointer.

`loader_free_library` decrements the reference count, removes the global slot,
releases only the original ELF-image memblock, and frees `dynalib_t`. It does
not release RX/RW segment memblocks. The current `main` never calls it because
execution finishes in an infinite loop.

## Fixed-offset patcher

`src/patcher.c` provides:

- `patch_address`
- `patch_uint32`
- `patch_uint16`
- `patch_thumb`
- `patch_arm`
- `hook_stub_proc`

`hook_stub_proc` writes an ARM absolute jump:

```text
E51FF004    LDR PC, [PC, #-4]
callback    absolute address
```

These routines still use the fixed runtime base and kuBridge unrestricted copy.
They are separate from automatic undefined-symbol resolution and are used for
version-specific code offsets.
