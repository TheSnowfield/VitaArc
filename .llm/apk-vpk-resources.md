# APK, VPK and runtime resource paths

## VPK 文件格式验证

VPK 是 ZIP 容器。Vita 应用安装所需的关键目录/文件包括：

```text
eboot.bin
sce_sys/param.sfo
sce_sys/icon0.png
sce_sys/livearea/contents/template.xml
sce_sys/livearea/contents/background.png
sce_sys/livearea/contents/startup.png
```

资源可以作为普通 ZIP entry 放在 VPK 任意子目录。安装后 VPK 根目录以只读设备 `app0:` 暴露。

实测 `vita-pack-vpk -a source=destination` 可以把普通资源加入 VPK；解包后资源内容与源文件逐字节相同。Deflate 和 Store entry 都可以存在于最终 VPK。

## APK 作为 ZIP

APK 本身也是 ZIP。项目不解码 AndroidManifest、resources.arsc、DEX 或 assets，而是把 APK 的所有非目录 entry 全量复制到 VPK：

```text
APK: AndroidManifest.xml
VPK: package/AndroidManifest.xml

APK: assets/Default/Button_Disable.png
VPK: package/assets/Default/Button_Disable.png

APK: lib/armeabi-v7a/libcocos2dcpp.so
VPK: package/lib/armeabi-v7a/libcocos2dcpp.so
```

这保留完整 APK 目录结构，便于未来 bridge 访问 `res/`、`META-INF/`、DEX 或其他路径，而不只处理 `assets` 和 `.so`。

## `app0:` 映射

Vita 的设备路径允许冒号后直接连接相对路径。VitaSDK 自带示例使用：

```c
fopen("app0:test.json", "rb");
```

所以当前采用：

```text
VPK entry                          runtime path
package/                           app0:package/
package/assets/                    app0:package/assets/
package/lib/armeabi-v7a/           app0:package/lib/armeabi-v7a/
package/lib/armeabi-v7a/libcocos2dcpp.so
                                    app0:package/lib/armeabi-v7a/libcocos2dcpp.so
```

`app0:/package/...` 在部分库中也能见到，但项目统一使用既有 Vita 设备路径风格 `app0:package/...`。

## `src/config.h`

当前宏级联：

```c
#define PATH_TO_APP "app0:"
#define PATH_TO_PACKAGE PATH_TO_APP "package/"
#define PATH_TO_LIBRARY PATH_TO_PACKAGE "lib/armeabi-v7a/"
#define PATH_TO_ASSETS PATH_TO_PACKAGE "assets"
#define PATH_TO_OBB PATH_TO_PACKAGE "obb/"

#define LIBRARY_LIBCOCOS2DCPP PATH_TO_LIBRARY "libcocos2dcpp.so"

#define PATH_TO_USER "ux0:vitaarc/"
#define PATH_TO_LOGFILE PATH_TO_USER "boot.log"
#define PATH_TO_DATA PATH_TO_USER "persistent/data/"
```

编译后的 VitaArc ELF 已检查到：

```text
app0:package/assets
app0:package/lib/armeabi-v7a/libcocos2dcpp.so
```

旧的下列字符串不再作为资源路径：

```text
ux0:vitaarc/assets
ux0:vitaarc/library/armeabi-v7a/libcocos2dcpp.so
```

## 只读与可写边界

`app0:` 是应用安装内容，不能用于运行时写入。

只读：

```text
APK assets
APK native libraries
APK manifest/resources/classes
随 VPK 安装的 OBB 目录（若未来导入）
```

可写：

```text
ux0:vitaarc/boot.log
ux0:vitaarc/persistent/data/
ux0:vitaarc/memdump.bin
ux0:vitaarc/shader/1.glsl
```

不能把存档、日志、shader dump 或 memory dump 改到 `app0:`。

## AssetManager 根目录

`src/bridges/android/assetmgr.c` 的全局 manager：

```c
static a_asset_manager_t __asset_manager = {
    .root_path = PATH_TO_ASSETS,
};
```

所以 NDK 调用 `AAssetManager_open(manager, "audio/x")` 最终打开：

```text
app0:package/assets/audio/x
```

AssetManager 拒绝：

- 空 manager/root。
- 以 `/` 或 `\` 开头的路径。
- 包含 `:` 或反斜杠的路径。
- 任一等于 `..` 的路径组件。

路径 buffer 为 1024 字节，超长拼接失败。

## libc 数据路径重定向

`bridge_fopen` 只处理 Android 私有数据前缀：

```text
/data/data/<suffix>
    ->
ux0:vitaarc/persistent/data/<suffix>
```

该重定向不用于 assets；assets 应通过 AssetManager 或显式 `app0:package/assets/...` 访问。

目前只有 `fopen` 使用该规则，`open/stat/access/opendir/remove/rename` 没有统一重定向。
