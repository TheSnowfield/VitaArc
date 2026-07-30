# Current working tree and development state

## Git state

Repository:

```text
/home/thesnowfield/Desktop/projects/VitaArc
```

Current branch:

```text
main
```

Observed refs:

- `HEAD -> main`
- `origin/main` points to current HEAD

Recent commits:

1. `320b6bd loader: refactor dynalib`
2. `f5eb846 chore: refactor`
3. `6af88d3 add read all file`
4. `fb7ea4f update bridges`
5. `40c4b0d clean project`

## Worktree status before this `.llm` alignment

`git status --short` produced no output before editing `.llm` in this task, so the tracked working tree was clean at that point.

## Current source layout change versus older `.llm`

Compared with the previous knowledge base:

- `src/common/*` moved into `src/bridges/*` or was otherwise removed from the current tree.
- `src/solibrary/*` was replaced by `src/bridges/loader.c` and `src/bridges/loader.h`.
- `src/utils/patcher.*` moved to `src/bridges/patcher.*`.
- `src/utils/kubridge.*` moved to `src/bridges/kubridge.*`.
- `src/kubridge/*` is no longer present in the current source tree.
- `third-party/kubridge` is now the submodule path in `.gitmodules`.
- Bridge subdirectory `CMakeLists.txt` files are no longer present.
- Top-level CMake now gathers all `src/*.c` and `src/*.cpp` recursively.

## `src/main.c` current behavior

- Includes `bridges/audio/audio.h`.
- Starts logging with `logBegin(PATH_TO_LOGFILE)`.
- Configures vitaGL runtime shader compiler.
- Sets VDM/vertex/fragment/USSE buffer sizes.
- Does not call `vglInitExtended`; the old initialization/clear/swap block is commented.
- Loads only `LIBRARY_LIBCOCOS2DCPP`.
- Applies EABI, Android, libc, OpenGL, Cocos2d-x and audio bridge patches.
- Calls `solibInitLibrary`.
- Calls `bridgeCallJNIMain`.
- Leaves Cocos platform/device/native init calls commented.
- Enters `for(;;);` at `ExitProgram`, making `logEnd()` unreachable in normal control flow.

## Build state

Current root directory does not contain `build/`, `CMakeCache.txt`, `VitaArc.vpk`, `VitaArc.self`, or other local build outputs.

Current `build.sh`:

```sh
cmake -S . -B build
cmake --build build --parallel 8
```

No build was run during this `.llm` alignment.

## `.llm` alignment edits

This task updates `.llm` documentation to match the current codebase. These are documentation-only changes.
