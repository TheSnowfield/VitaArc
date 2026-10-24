# Production module membership shared by the target and host tests.
set(VITAARC_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../src")

set(VITAARC_JNI_RUNTIME_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/heap.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/class.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/string.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/array.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/field.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/method.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/services.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/native.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/reference.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/runtime/exception.c"
)

set(VITAARC_FILE_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/libc/file.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/file/metadata.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/file/rename.c"
)

set(VITAARC_AUDIO_BACKEND_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/audio/backend.c"
    "${VITAARC_SOURCE_ROOT}/bridges/audio/common.c"
    "${VITAARC_SOURCE_ROOT}/bridges/audio/feed.c"
    "${VITAARC_SOURCE_ROOT}/bridges/audio/mixer.c"
    "${VITAARC_SOURCE_ROOT}/bridges/audio/bgm.c"
    "${VITAARC_SOURCE_ROOT}/bridges/audio/sfx.c"
)

set(VITAARC_APPLICATION_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridge_symbols.c"
    "${VITAARC_SOURCE_ROOT}/init.c"
    "${VITAARC_SOURCE_ROOT}/kubridge.c"
    "${VITAARC_SOURCE_ROOT}/patcher.c"
)

set(VITAARC_ABI_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/abi/kuser.c"
    "${VITAARC_SOURCE_ROOT}/bridges/abi/softfp.c"
)

set(VITAARC_ANDROID_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/android/assetmgr.c"
    "${VITAARC_SOURCE_ROOT}/bridges/android/log.c"
)

set(VITAARC_AUDIO_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/audio/mp3.c"
    "${VITAARC_SOURCE_ROOT}/bridges/audio/provider.c"
    "${VITAARC_SOURCE_ROOT}/bridges/audio/stream.c"
    "${VITAARC_SOURCE_ROOT}/bridges/audio/wav.c"
)

set(VITAARC_COCOS2DX_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/cocos2dx/cocos2dx.c"
    "${VITAARC_SOURCE_ROOT}/bridges/cocos2dx/storage.c"
    "${VITAARC_SOURCE_ROOT}/bridges/cocos2dx/textures.c"
)

set(VITAARC_COMPAT_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/compat/compat.c"
)

set(VITAARC_JNI_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/jni/bitmap.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/cocos.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/descriptor.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/jni.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/native_call.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/preferences.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/references.c"
    "${VITAARC_SOURCE_ROOT}/bridges/jni/thread.c"
)

set(VITAARC_LIBC_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/libc/bionic/__memcpy_chk.cpp"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/bionic/__memmove_chk.cpp"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/bionic/__memset_chk.cpp"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/bionic/__strchr_chk.cpp"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/bionic/__strlen_chk.cpp"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/bionic/__vsnprintf_chk.cpp"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/bionic/__vsprintf_chk.cpp"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/cxx.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/directory.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/guest_errno.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/libc.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/memory.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/once.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/poll.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/posix.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/pthread.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/sync.c"
    "${VITAARC_SOURCE_ROOT}/bridges/libc/thread_attributes.c"
)

set(VITAARC_OPENGL_SOURCES
    "${VITAARC_SOURCE_ROOT}/bridges/opengl/buffers.c"
    "${VITAARC_SOURCE_ROOT}/bridges/opengl/gl.c"
    "${VITAARC_SOURCE_ROOT}/bridges/opengl/pixel_store.c"
    "${VITAARC_SOURCE_ROOT}/bridges/opengl/proc.c"
    "${VITAARC_SOURCE_ROOT}/bridges/opengl/shader_binary.c"
    "${VITAARC_SOURCE_ROOT}/bridges/opengl/shader_cache.c"
    "${VITAARC_SOURCE_ROOT}/bridges/opengl/opengl.c"
)

set(VITAARC_LOADER_SOURCES
    "${VITAARC_SOURCE_ROOT}/loader/image.c"
    "${VITAARC_SOURCE_ROOT}/loader/relocation.c"
    "${VITAARC_SOURCE_ROOT}/loader/validation.c"
    "${VITAARC_SOURCE_ROOT}/loader.c"
)

set(VITAARC_LOGCAT_SOURCES
    "${VITAARC_SOURCE_ROOT}/logcat/logcat.c"
    "${VITAARC_SOURCE_ROOT}/logcat/screen.c"
    "${VITAARC_SOURCE_ROOT}/logcat/text.c"
)

set(VITAARC_PLATFORM_SOURCES
    "${VITAARC_SOURCE_ROOT}/platform/input.c"
    "${VITAARC_SOURCE_ROOT}/platform/loop.c"
    "${VITAARC_SOURCE_ROOT}/platform/memory.c"
    "${VITAARC_SOURCE_ROOT}/platform/perf.c"
    "${VITAARC_SOURCE_ROOT}/platform/stdio.c"
    "${VITAARC_SOURCE_ROOT}/platform/timing.c"
)

set(VITAARC_UTILS_SOURCES
    "${VITAARC_SOURCE_ROOT}/utils/debug.c"
    "${VITAARC_SOURCE_ROOT}/utils/fs.c"
    "${VITAARC_SOURCE_ROOT}/utils/string.c"
)

set(VITAARC_SOURCES
    ${VITAARC_JNI_RUNTIME_SOURCES}
    ${VITAARC_FILE_SOURCES}
    ${VITAARC_AUDIO_BACKEND_SOURCES}
    ${VITAARC_APPLICATION_SOURCES}
    ${VITAARC_ABI_SOURCES}
    ${VITAARC_ANDROID_SOURCES}
    ${VITAARC_AUDIO_SOURCES}
    ${VITAARC_COCOS2DX_SOURCES}
    ${VITAARC_COMPAT_SOURCES}
    ${VITAARC_JNI_SOURCES}
    ${VITAARC_LIBC_SOURCES}
    ${VITAARC_OPENGL_SOURCES}
    ${VITAARC_LOADER_SOURCES}
    ${VITAARC_LOGCAT_SOURCES}
    ${VITAARC_PLATFORM_SOURCES}
    ${VITAARC_UTILS_SOURCES}
)
