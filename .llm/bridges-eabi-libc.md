# EABI, libc, Bionic and pthread bridges

## EABI bridge

`bridgePatchEABI` 注册以下导入符号：

- `__aeabi_d2uiz`
- `__aeabi_dmul`
- `__aeabi_dsub`
- `__aeabi_f2ulz`
- `__aeabi_l2d`
- `__aeabi_l2f`
- `__aeabi_memclr`
- `__aeabi_memclr4`
- `__aeabi_memclr8`
- `__aeabi_memcpy`
- `__aeabi_memcpy4`
- `__aeabi_memcpy8`
- `__aeabi_memmove`
- `__aeabi_memmove4`
- `__aeabi_memset`
- `__aeabi_memset4`
- `__aeabi_memset8`
- `__aeabi_ui2d`
- `__aeabi_ul2d`

实现来自 Vita 工具链可用的 EABI helper 声明。

## libc bridge 表

`bridgePatchLibC` 用一个大型静态表映射 Android/Bionic 导入。

### C++/Bionic runtime

- `__cxa_atexit`
- `__cxa_finalize`
- `__errno`
- `__gnu_Unwind_Find_exidx`
- `__stack_chk_fail`
- `__stack_chk_guard`
- `__assert2`

`__stack_chk_fail`、`__check_failed`、`__assert2` 记录 fatal 日志并 `exit(-1)`。

`__gnu_Unwind_Find_exidx` 记录 unsupported/called，返回 0。

### Bionic fortify

- `__memcpy_chk`
- `__memset_chk`
- `__strchr_chk`
- `__strlen_chk`
- `__vsnprintf_chk`
- `__vsprintf_chk`

相关实现位于 `src/bridges/libc/impl/bionic/*.cpp`。`__fortify_chk_fail` 记录 fatal 并退出。

### 内存和字符串

直接或包装：

- `calloc`
- `free`
- `malloc`
- `memalign`
- `memchr`
- `memcmp`
- `memcpy`
- `memmove`
- `memset`
- `realloc`
- `bsearch`
- `qsort`
- `strcat`
- `strchr`
- `strcmp`
- `strcoll`
- `strcpy`
- `strcspn`
- `strdup`
- `strerror`
- `strerror_r`
- `strlen` → `bridgeStrlen`
- `strncmp`
- `strncpy`
- `strrchr`
- `strspn`
- `strstr`
- `strtok`
- `strcasecmp`
- `strtod`
- `strtol`
- `strtoll`
- `strtoul`
- `strtoull`
- `strxfrm`
- `sscanf`
- `vsscanf`
- `atoi`
- `atol`

`bridgeStrlen` 记录字符串内容后调用 Vita libc `strlen`。

`memmem` 是占位实现，记录 unsupported，返回 `NULL`。

`strtoimax` 和 `strtoumax` 是占位实现，返回 0。

### stdio

- `fclose`
- `ferror`
- `fflush`
- `fopen` → `bridgeFopen`
- `fprintf`
- `fputc`
- `fread`
- `fseek`
- `fseeko`
- `ftell`
- `ftello`
- `fwrite`
- `getc`
- `perror`
- `printf` → `logPrintf`
- `puts`
- `snprintf`
- `sprintf`
- `ungetc`
- `vasprintf`
- `vfprintf`
- `vsnprintf`

`bridgeFopen` 对以 `/data/data/` 开头的路径执行重定向：

```text
/data/data/<suffix>
    ↓
ux0:vitaarc/persistent/data/<suffix after 11 chars>
```

重定向使用 256 字节栈缓冲区和 `sprintf`，没有边界检查。

存在 `bridgeFread` 包装函数，但 bridge 表中的 `fread` 当前直接指向系统 `fread`，没有使用 `bridgeFread`。

### 文件描述符和文件系统

- `access`
- `close` → `bridgeClose`
- `closedir`
- `fchmod`
- `fchown`
- `fcntl`
- `fstat`
- `fsync`
- `ftruncate64`
- `lseek`
- `lseek64`
- `lstat`
- `mkdir`
- `open` → `bridgeOpen`
- `__open_2` → 系统 `open`，不是 `bridgeOpen`
- `opendir`
- `read` → `bridgeRead`
- `readlink`
- `remove`
- `rename`
- `rmdir`
- `stat`
- `unlink`
- `utimes`
- `write`

占位行为：

- `mkdir`：返回 0，不创建目录。
- `lseek64`：返回 0，不移动文件位置。
- `lstat`：返回 0，不填充 `stat`。
- `fsync`：返回 0。
- `ftruncate64`：返回 0。
- `getcwd`：返回 `NULL`。
- `rmdir`：返回 0。
- `readlink`：返回 0。
- `utimes`：返回 0。
- `fchmod`：返回 0。
- `fchown`：返回 0。

### `/dev/urandom`

`bridgeOpen("/dev/urandom", ...)` 返回伪 fd：

```c
DEV_URANDOM = 1
```

`bridgeRead(DEV_URANDOM, buffer, size)` 调用 `sceKernelGetRandomNumber` 并返回请求长度。

`bridgeClose(DEV_URANDOM)` 返回 0。

伪 fd 1 可能与正常 POSIX stdout fd 值冲突。

### 动态加载和虚拟内存

- `dladdr`
- `dlclose`
- `dlerror`
- `dlopen`
- `dlsym`
- `__mmap2` → `bridgeMmap2`
- `mremap`
- `munmap`

当前均为占位：

- `dlopen`/`dlsym`/`mmap2`/`mremap` 返回 `NULL`。
- `dlerror` 返回 `NULL`。
- `dladdr`/`dlclose`/`munmap` 返回 0。

这些函数没有接入本项目的 `solibLoadLibrary`/`solibGetProcAddress`。

### 时间、进程和系统

- `clock_gettime` → 自定义
- `getpid`
- `gettimeofday`
- `gmtime`
- `localtime`
- `nanosleep` → 占位
- `sched_yield`
- `syscall` → 占位
- `sysconf` → 占位
- `time`
- `strftime`
- `geteuid` → 占位
- `getenv`
- `exit`
- `abort`
- `raise` 被注释，没有 bridge

自定义 `clock_gettime`：

1. 调用 `sceRtcGetCurrentTick`。
2. `tv_nsec = 0`。
3. `tv_sec = sRtcTick.tick`。

`SceRtcTick.tick` 是 Vita RTC tick 数，不直接等于 POSIX 秒；当前实现没有换算。

`nanosleep` 返回 0，不等待。

### 网络

直接映射：

- `accept`
- `bind`
- `freeaddrinfo`
- `getaddrinfo`
- `inet_ntop`
- `inet_pton`
- `listen`
- `recvfrom`
- `select`
- `sendto`
- `setsockopt`
- `socket`

`gai_strerror` 为占位，返回 `NULL`。

`ioctl` 为占位，返回 0。

网络函数依赖 Vita libc/stub 的 ABI 是否与 Android 调用者预期一致。

### 数学

直接映射：

- `acos`
- `acosf`
- `asinf`
- `atan2f`
- `ceil`
- `ceilf`
- `cos`
- `cosf`
- `exp2f`
- `floor`
- `floorf`
- `fmodf`
- `ldexp`
- `log10f`
- `modf`
- `pow`
- `powf`
- `roundf`
- `scalbn`
- `sincos`
- `sincosf`
- `sinf`
- `tanf`

### 字符分类、宽字符和 locale 相关

- `btowc`
- `isalnum`
- `isalpha`
- `isblank`
- `islower`
- `isspace`
- `isupper`
- `isxdigit`
- `iswalpha`
- `iswcntrl`
- `iswdigit`
- `iswlower`
- `iswprint`
- `iswpunct`
- `iswspace`
- `iswupper`
- `iswxdigit`
- `mbrlen`
- `tolower`
- `toupper`
- `towlower`
- `towupper`
- `wcscoll`
- `wcsxfrm`
- `wctob`

### 随机

- `lrand48`
- `srand48`

### 非局部跳转

- `setjmp`
- `longjmp`

这些直接跨 Android 调用代码与 Vita libc，依赖 ABI/寄存器保存约定兼容。

## pthread bridge

Android mutex 被解释为：

```c
typedef struct {
    pthread_mutex_t value;
} android_pthread_mutex_t;
```

宏 `_MUTEX(x)` 取其 `value`。

### 线程

- `_pthread_create` → `pthread_create`
- `_pthread_detach` → `pthread_detach`
- `_pthread_equal` → `pthread_equal`
- `_pthread_join` → `pthread_join`
- `_pthread_self` → `pthread_self`

### mutex

- `_pthread_mutex_init`
- `_pthread_mutex_destroy`
- `_pthread_mutex_lock`
- `_pthread_mutex_timedlock`
- `_pthread_mutex_trylock`
- `_pthread_mutex_unlock`

`_pthread_mutex_init` 若 `_MUTEX(mutex)` 非零直接返回 0；否则初始化。

`_pthread_mutex_lock` 在每次 lock 前调用 `_pthread_mutex_init`，用于尝试惰性初始化。

`_pthread_mutex_destroy` 销毁后把 `_MUTEX(mutex)` 写成 `NULL`。

bridge 表没有注册 `pthread_mutex_timedlock`，虽然本地实现存在。

### mutex attribute

- `_pthread_mutexattr_init`
- `_pthread_mutexattr_destroy`
- `_pthread_mutexattr_settype`

### TLS

- `_pthread_key_create`
- `_pthread_key_delete`
- `_pthread_setspecific`
- `_pthread_getspecific`

### once

- `_pthread_once`

### condition variable

- `_pthread_cond_destroy`
- `_pthread_cond_wait`
- `_pthread_cond_timedwait`
- `_pthread_cond_signal`
- `_pthread_cond_broadcast`

condition variable本身直接作为 Vita `pthread_cond_t` 使用；传入的 mutex 则改为内部 `_MUTEX(mutex)`。

没有 `pthread_cond_init` bridge。代码依赖零初始化 condition variable 可被底层实现接受，或目标库不导入该函数。
