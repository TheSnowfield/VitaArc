# Android and JNI bridges

## Android log

统一 `symbol.def` 注册：

```text
__android_log_print
__android_log_write
```

实现把 Android priority、tag 和文本传给 VitaArc logger。Android priority 数值和本地五级 `LOGLEVEL` 不完全同构，需继续注意越界/等级映射。

## Android AssetManager 类型

```c
struct a_asset_manager {
  const char *root_path;
};

struct a_asset_dir {
  char **file_names;
  size_t file_count;
  size_t next_index;
};

struct a_asset {
  FILE *stream;
  char *path;
  off_t length;
  void *buffer;
};
```

全局 manager：

```text
root_path = app0:package/assets
```

## AssetManager exported API

`symbol.def` 当前包含：

```text
AAssetDir_close
AAssetDir_getNextFileName
AAssetDir_rewind
AAssetManager_fromJava
AAssetManager_open
AAssetManager_openDir
AAsset_close
AAsset_getBuffer
AAsset_getLength
AAsset_getRemainingLength
AAsset_isAllocated
AAsset_openFileDescriptor
AAsset_read
AAsset_seek
```

不存在旧表中的重复 `AAssetDir_close`。

## Asset path validation

relative path 必须：

- 非 NULL。
- 不以 `/` 或 `\` 开头。
- 不包含 `:`。
- 不包含反斜杠。
- 任一 `/` 分隔组件不能等于 `..`。

允许空字符串用于打开 asset 根目录。内部 path capacity 为 1024。

实际路径：

```text
<manager.root_path>/<relative_path>
```

## `AAssetManager_fromJava`

忽略 fake `env` 和 `asset_manager` jobject，返回全局 `__asset_manager`。

## `AAssetManager_open`

1. 校验文件名和路径。
2. `fopen(path, "rb")`。
3. `fseek` 到末尾。
4. `ftell` 保存长度。
5. 回到开头。
6. 分配 `a_asset_t`。
7. 复制完整路径。
8. 保存 stream/length。

失败时关闭已打开 stream 并释放已分配对象。

mode 当前被忽略，不区分 BUFFER/RANDOM/STREAMING/UNKNOWN。

## `AAssetManager_openDir`

1. 构造目录路径。
2. `opendir`。
3. 跳过 `.`、`..`。
4. 对每个 entry 构造完整路径并 `stat`。
5. 只保留普通文件；子目录不出现在结果中。
6. 名字数组容量从 16 开始倍增。
7. 每个名字单独复制。
8. 最后按 `strcmp` 排序。

`AAssetDir_getNextFileName` 依次返回内部 name；结束返回 NULL。

`AAssetDir_rewind` 把 index 置 0。

`AAssetDir_close` 释放所有 name、数组和 dir 对象。

## Asset data API

`AAsset_read`：

- 无效 asset/stream/buffer 返回 -1。
- count 为 0 返回 0。
- 大于 `INT_MAX` 的 request 截到 `INT_MAX`。
- `fread`。
- 0 bytes 且 `ferror` 返回 -1。

`AAsset_seek`：

- 使用 `fseek`/`ftell`。
- 失败返回 `(off_t)-1`。

`AAsset_getLength` 返回保存长度。

`AAsset_getRemainingLength`：

```text
length - current ftell position
```

position 错误或已到末尾返回 0。

`AAsset_getBuffer`：

1. 已缓存则返回原 buffer。
2. 分配 length 字节；空文件分配 1 字节。
3. 保存当前 stream position。
4. 回到开头并读取完整文件。
5. 恢复 position。
6. 保存 buffer。

`AAsset_isAllocated` 只在 `getBuffer` 已建立缓存时返回 1。

`AAsset_openFileDescriptor`：

- 对 asset path 再次 `open(O_RDONLY)`。
- `out_start=0`。
- `out_length=asset.length`。
- 返回独立 fd。

`AAsset_close` 关闭 stream，释放 buffer/path/object。

## JNI ABI header

`src/bridges/jni/aosp/jni.h` 保留 Android/AOSP 函数表布局：

- primitive/reference types。
- `JNINativeMethod`。
- `JNINativeInterface`。
- `JNIInvokeInterface`。
- C++ convenience wrappers。
- JNI version/error/reference type constants。

目标 Android ARM library 按该布局间接调用 JNI。

## fake runtime object model

`runtime.c` 定义 kind：

```text
FAKE_JNI_OBJECT
FAKE_JNI_CLASS
FAKE_JNI_STRING
FAKE_JNI_METHOD
FAKE_JNI_FIELD
FAKE_JNI_ARRAY
FAKE_JNI_DIRECT_BUFFER
```

每个对象：

```text
magic = 0x4A4E4931
kind
next
clazz
name
signature
utf8
utf16
length
element_size
data
native_proc
global_refs
is_static
```

所有对象挂在 `__fake_objects` 单链表。当前没有 runtime shutdown 或对象回收；分配对象在进程生命周期内保留。

`__runtime_cast` 只接受 magic 正确的 fake object。

## strings

runtime 实现 UTF-8 解码和 UTF-8/UTF-16 双向转换：

- 支持 1/2/3/4-byte UTF-8 sequence。
- 非法 sequence 写 U+FFFD。
- 非 BMP codepoint 转 surrogate pair。
- UTF-16 surrogate pair 转 4-byte UTF-8。

fake string 同时保存：

```text
utf8 NUL-terminated copy
utf16 NUL-terminated copy
UTF-16 code unit length
```

`NewString` 和 `NewStringUTF` 创建独立 fake object，不再把调用者裸指针强制当作 `jstring`。

`GetStringChars`/`GetStringUTFChars` 返回内部稳定地址并把 `is_copy` 设为 false。

Release string 函数不释放，因为数据属于 fake object。

`GetStringLength` 是 UTF-16 code unit 数；`GetStringUTFLength` 是 UTF-8 byte 数。

## classes, methods and fields

`FindClass` 按 class name 复用已有 fake class，否则分配新 class。

method/field identity 包含：

```text
class
name
signature
kind
is_static
```

`GetMethodID`、`GetStaticMethodID`、`GetFieldID`、`GetStaticFieldID` 返回相应 fake member，不再使用固定整数枚举 method ID。

普通 field accessor 当前由宏生成占位值；setter 不保存实例/静态 field state。

## object calls

`NewObject*`/`AllocObject` 创建 fake object 并关联 class。

实例 object method 特例：

```text
getClassLoader -> fake java/lang/ClassLoader object
loadClass      -> FindClass(jstring name)
```

其他实例 primitive/object/void calls 大多返回零/NULL或不执行。

静态 Cocos method dispatch 通过 fake method name：

```text
getCocos2dxPackageName
getCocos2dxWritablePath
getCurrentLanguage
generateGuid
getStringForKey
getIntegerForKey
getBoolForKey
setStringForKey
```

`getStringForKey` 返回 default value。

`getIntegerForKey` 返回 default integer。

`getBoolForKey` 返回 default boolean。

`setStringForKey` 会调用本地函数，但本地函数仍只记录日志，不持久化。

## arrays

fake array 保存：

```text
length
element_size
calloc data
```

支持：

- object arrays。
- boolean/byte/char/short/int/long/float/double arrays。
- Get/Set object element。
- Get primitive elements。
- Get/Set primitive regions。
- GetArrayLength。
- PrimitiveArrayCritical。

越界 region/element 操作直接返回，不设置 Java exception。

Release primitive elements 不执行 copy-back/free，因为 Elements 返回内部 data。

## native registration

`RegisterNatives`：

1. 校验 class、methods、count。
2. 按每个 name/signature 建立或复用 instance fake method。
3. 把 `fnPtr` 保存到 `native_proc`。
4. 记录映射日志。

`UnregisterNatives` 把属于该 class 的 fake method `native_proc` 清空。

当前普通 `Call*Method` dispatch 没有统一调用 `native_proc`，所以保存注册表不代表所有 registered native method 已能通过 JNI call API执行。

## exceptions and references

全局 `__pending_exception` 支持：

- `Throw`
- `ThrowNew`
- `ExceptionOccurred`
- `ExceptionDescribe`
- `ExceptionClear`
- `ExceptionCheck`

没有 exception class hierarchy 或 stack。

global ref 只增加/减少对象内 `global_refs`。local/weak refs 不维护 frame 生命周期。

`GetObjectRefType` 根据 magic/global_refs 返回 invalid/global/local。

## direct buffers and monitor

`NewDirectByteBuffer` 保存外部 address/capacity。

`GetDirectBufferAddress`/`Capacity` 返回保存值。

`MonitorEnter`/`MonitorExit` 固定成功，不做锁。

## JavaVM

`jvm.c` 已合并进 `runtime.c`。

全局：

```text
__fake_native_interface
__fake_invoke_interface
__fake_env
__fake_java_vm
```

公开：

```c
JNIEnv *jni_runtime_get_env(void);
JavaVM *jni_runtime_get_java_vm(void);
```

`GetEnv`：

- 校验输出指针。
- 接受 JNI 1.1/1.2/1.4/1.6。
- 写入 fake env。

`AttachCurrentThread` 和 daemon 版本现在会写 `JNIEnv **`，不再是假成功但不返回 env。

`DestroyJavaVM`/`DetachCurrentThread` 固定成功。

## Cocos platform values

```text
package name   moe.low.arc
language       en
writable path  ux0:vitaarc/persistent/data/
GUID           61616161
app version    v6.1.6
device ID      61616161
```

`getStringForKey` 返回传入 default。

`setStringForKey` 仍不落盘。

## `JNI_OnLoad`

`bridge_call_jni_main`：

- 查找并空检 `JNI_OnLoad`。
- dump 固定 SO 范围。
- 传 `jni_runtime_get_java_vm()`。
- 记录返回版本。
- 非 1.1/1.2/1.4/1.6 时警告。

Cocos exported JNI wrapper 也会在调用前检查 symbol 是否存在。
