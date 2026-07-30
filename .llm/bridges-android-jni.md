# Android and JNI bridges

## Android log

注册：

- `__android_log_print`
- `__android_log_write`

两个函数都：

1. 接收 Android priority、tag、format 和 varargs。
2. 将 priority 强制转换为本地 `LOGLEVEL`。
3. 调用 `logBase`。
4. 返回 0。

本地 `LOGLEVEL` 只有 0..4 五个值，而 Android log priority 的数值体系不是同一个枚举。未经范围转换的 priority 可越界访问 `logLvString`。

## Android AssetManager

注册表包含：

- `AAssetDir_close`，重复注册两次
- `AAssetDir_getNextFileName`
- `AAssetManager_fromJava`
- `AAssetManager_open`
- `AAssetManager_openDir`
- `AAsset_close`
- `AAsset_getLength`
- `AAsset_openFileDescriptor`
- `AAsset_read`

当前行为：

- `AAssetDir_close`：空。
- `AAssetDir_getNextFileName`：返回 `NULL`。
- `AAssetManager_fromJava`：函数没有返回语句。
- `AAssetManager_open`：返回 `NULL`。
- `AAssetManager_openDir`：返回 `NULL`。
- `AAsset_close`：空。
- `AAsset_getLength`：声明为 `void` 且空实现；Android NDK API 正常需要长度返回值。
- `AAsset_openFileDescriptor`：返回 0，不写 `outStart/outLength`。
- `AAsset_read`：返回 0，不读取。

README 要求复制 APK `assets` 到 `ux0:vitaarc/assets`，但当前 AssetManager bridge 没有把 Android asset path 映射到这个目录。

## JNI ABI 头

`src/bridges/jni/impl/android/jni.h` 是 Android/AOSP 风格的 JNI ABI 声明，包含：

- Java primitive types。
- jobject/jclass/jstring/jarray 等引用类型。
- `JNINativeMethod`。
- 完整 `JNINativeInterface` 函数表布局。
- `JNIInvokeInterface`。
- C++ `_JNIEnv` 和 `_JavaVM` convenience methods。
- JNI 版本和错误码。
- `JNI_OnLoad`/`JNI_OnUnload` 函数指针类型。

该头的函数表字段顺序用于与 Android ARM 原生库的间接 JNI 调用 ABI 对齐。

## 本地 `JNINativeInterface`

当前只填充以下字段：

- `reserved0 = &jniNativeEnv`
- `GetVersion`
- `FromReflectedField`
- `FromReflectedMethod`
- `FindClass`
- `ExceptionClear`
- `NewGlobalRef`
- `DeleteLocalRef`
- `GetJavaVM`
- `GetMethodID`
- `GetStaticMethodID`
- `GetStringChars`
- `GetStringLength`
- `CallObjectMethodV`
- `CallStaticObjectMethodV`
- `CallStaticIntMethodV`
- `CallStaticBooleanMethodV`
- `CallStaticVoidMethodV`
- `CallBooleanMethodV`
- `CallIntMethodV`
- `CallFloatMethodV`
- `CallVoidMethodV`
- `NewString`
- `NewStringUTF`
- `GetStringUTFChars`
- `ReleaseStringChars`
- `ReleaseStringUTFChars`
- `RegisterNatives`

未初始化字段默认为 `NULL`。目标库调用未填字段时会跳到空地址。

## 本地 `JNIInvokeInterface`

填充：

- `reserved0 = &jniInvokeEnv`
- `GetEnv`
- `DestroyJavaVM`
- `AttachCurrentThread`
- `DetachCurrentThread`
- `AttachCurrentThreadAsDaemon`

另外定义：

```c
static JavaVM jniJavaVM = &jniInvokeEnv;
```

`GetEnv` 将输出环境设置为 `&jniNativeEnv`，记录日志，返回 0。

`GetJavaVM` 将输出 VM 设置为 `&jniJavaVM`，记录日志，返回 0。

注意实际 JNI C ABI 的 `JNIEnv`/`JavaVM` 是函数表指针的指针语义；当前代码在多个调用处直接传结构地址或伪 VM 指针，依赖目标二进制的具体解引用方式。

## JavaVM 方法行为

- `DestroyJavaVM`：记录日志，返回 0。
- `AttachCurrentThread`：记录日志，返回 0，但不写 `JNIEnv **`。
- `DetachCurrentThread`：记录日志，返回 0。
- `AttachCurrentThreadAsDaemon`：记录日志，返回 0，但不写 `JNIEnv **`。

## JNI method ID 模型

不是创建真实 Java class/method 对象，而是定义内部枚举：

- `UNKNOWN = 0`
- `GET_COCOS2DX_PACKAGE_NAME`
- `GET_COCOS2DX_WRITABLE_PATH`
- `GET_CURRENT_LANGUAGE`
- `GET_INTEGER_FOR_KEY`
- `GET_STRING_FOR_KEY`
- `GET_BOOL_FOR_KEY`
- `SET_STRING_FOR_KEY`
- `GENERATE_GUID`
- `LOAD_CLASS_METHOD = 0x23333333`

按方法名映射：

- `getCocos2dxPackageName`
- `getCocos2dxWritablePath`
- `getCurrentLanguage`
- `getIntegerForKey`
- `getStringForKey`
- `getBoolForKey`
- `setStringForKey`
- `generateGuid`

签名字符串没有参与 method lookup。

## JNI 函数行为

### 版本、类和引用

- `GetVersion`：记录日志，返回 0，不返回标准 JNI 版本。
- `FindClass`：记录日志，返回 0。
- `ExceptionClear`：仅记录。
- `NewGlobalRef`：返回固定值 `0xDEADC0DE`。
- `DeleteLocalRef`：仅记录。
- `FromReflectedField`：记录日志，返回 `NULL`。
- `FromReflectedMethod`：记录日志，返回 `NULL`。

### method lookup

`GetMethodID`：记录日志，返回 `NULL`。

`GetStaticMethodID`：

1. 记录 env、class、method name、signature。
2. 只按 method name 遍历内部表。
3. 返回枚举值强制转换的 `jmethodID`。
4. 未命中返回 `NULL`。

### method invocation

`CallObjectMethodV`：

- 若 object 为 0 且 method ID 为 0，返回 `LOAD_CLASS_METHOD`。
- 其他返回 `NULL`。

`CallStaticObjectMethodV`：

- `GET_COCOS2DX_PACKAGE_NAME` → `getCocos2dxPackageName`
- `GET_CURRENT_LANGUAGE` → `getCurrentLanguage`
- `GENERATE_GUID` → `generateGuid`
- 其他返回 `NULL`

`CallStaticVoidMethodV`：

- `SET_STRING_FOR_KEY` 分支直接 `return`，没有读取 varargs，也没有调用 `setStringForKey`。
- 其他分支也只返回。

以下函数记录日志并返回零值：

- `CallStaticIntMethodV` → 0
- `CallStaticBooleanMethodV` → 0
- `CallBooleanMethodV` → 0
- `CallIntMethodV` → 0
- `CallFloatMethodV` → `0.0f`

`CallVoidMethodV` 仅记录。

### string

宏：

```c
#define JSTRING(x) (jstring)u##x
```

它把 C UTF-16 字面量地址直接伪装为 `jstring`。

`NewStringUTF`：

- 直接把输入 UTF-8 `char *` 地址当 `jstring` 返回。

`GetStringUTFChars`：

- 直接把 `jstring` 当 `char *` 返回。

`GetStringChars`：

- 直接把 `jstring` 当 `jchar *` 返回。

`GetStringLength`：

- 将 `jstring` 当 UTF-16 `jchar *`，调用 `utilUcharLen`。

`NewString`：

- 打印输入的 UTF-16 原始字节。
- 直接返回输入地址。

`ReleaseStringChars` 和 `ReleaseStringUTFChars`：

- 只记录，不释放。

同一个伪 `jstring` 可能来自 UTF-8 地址或 UTF-16 地址，调用者必须恰好使用匹配的 accessor，否则编码解释不一致。

### native registration

`RegisterNatives` 只记录日志，不保存 method 表，返回 0。

## Cocos/平台方法

本地方法：

- `loadClassMethod()`：仅记录。
- `getStringForKey(key, default)`：返回 UTF-16 `"NULL"`。
- `getCocos2dxPackageName()`：返回 UTF-16 `"moe.low.arc"`。
- `getCurrentLanguage()`：返回 UTF-16 `"en"`。
- `getCocos2dxWritablePath()`：返回 UTF-16 `"NULL"`。
- `setStringForKey(key, value)`：仅记录，不持久化。
- `generateGuid()`：返回 UTF-16 `"61616161"`。

`getIntegerForKey` 和 `getBoolForKey` 有 method ID，但没有对应本地实现/调用分支。

## 游戏导出调用

通过动态符号表查找：

- `JNI_OnLoad`
- `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit`
- `Java_low_moe_AppActivity_initJVMPlatformUtils`
- `Java_low_moe_AppActivity_setAppVersion`
- `Java_low_moe_AppActivity_setDeviceId`

函数指针没有做空检查。

参数原型是按已逆向出的目标库调用方式手工定义，不包含标准 JNI 导出常见的 `JNIEnv *`/`jobject` 全部参数。

当前 `main()` 只调用 `bridgeCallJNIMain`；Cocos init/device/native init 调用被注释。
