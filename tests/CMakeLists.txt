cmake_minimum_required(VERSION 3.16)
project(VitaArcHostTests C)

enable_testing()
find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Freetype REQUIRED)
find_path(FFI_INCLUDE_DIR ffi.h PATH_SUFFIXES x86_64-linux-gnu aarch64-linux-gnu)
find_library(FFI_LIBRARY ffi)
find_path(VORBIS_INCLUDE_DIR vorbis/vorbisfile.h)
find_library(VORBISFILE_LIBRARY vorbisfile)
find_library(VORBISENC_LIBRARY vorbisenc)
find_library(VORBIS_LIBRARY vorbis)
find_library(OGG_LIBRARY ogg)
if(NOT VORBIS_INCLUDE_DIR OR NOT VORBISFILE_LIBRARY OR NOT VORBISENC_LIBRARY OR NOT VORBIS_LIBRARY OR NOT OGG_LIBRARY)
  message(FATAL_ERROR "Host audio tests require libvorbis-dev and libogg-dev")
endif()
if(NOT FFI_INCLUDE_DIR OR NOT FFI_LIBRARY)
  message(FATAL_ERROR "Host native dispatch tests require libffi-dev")
endif()
option(VITAARC_SANITIZERS "Enable address and undefined behavior sanitizers" ON)
option(VITAARC_THREAD_SANITIZER "Enable ThreadSanitizer in a separate build" OFF)
if(VITAARC_SANITIZERS AND VITAARC_THREAD_SANITIZER)
  message(FATAL_ERROR "ThreadSanitizer requires -DVITAARC_SANITIZERS=OFF")
endif()

set(VITAARC_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${VITAARC_ROOT}/cmake/Sources.cmake")

add_executable(test_log_text test_log_text.c "${VITAARC_ROOT}/src/logcat/text.c")
target_include_directories(test_log_text PRIVATE "${VITAARC_ROOT}/src")
target_compile_options(test_log_text PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME startup_log_text COMMAND test_log_text)

add_library(jni_bridge STATIC
  ${VITAARC_JNI_RUNTIME_SOURCES}
  "${VITAARC_ROOT}/src/bridges/jni/thread.c"
  "${VITAARC_ROOT}/src/bridges/jni/references.c"
  "${VITAARC_ROOT}/src/bridges/jni/descriptor.c"
  "${VITAARC_ROOT}/src/bridges/jni/native_call.c"
  "${VITAARC_ROOT}/src/bridges/jni/cocos.c"
  "${VITAARC_ROOT}/src/bridges/jni/bitmap.c"
  "${VITAARC_ROOT}/src/platform/timing.c"
  "${VITAARC_ROOT}/src/bridges/jni/preferences.c"
  ${VITAARC_FILE_SOURCES}
  "${VITAARC_ROOT}/src/bridges/libc/posix.c"
  "${VITAARC_ROOT}/src/bridges/jni/jni.c"
  "${VITAARC_ROOT}/src/bridges/android/assetmgr.c"
  mocks/platform.c)
target_include_directories(jni_bridge PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/mocks/include"
  "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_link_libraries(jni_bridge PUBLIC Threads::Threads m ${CMAKE_DL_LIBS} Freetype::Freetype)
target_include_directories(jni_bridge PRIVATE "${FFI_INCLUDE_DIR}")
target_link_libraries(jni_bridge PUBLIC "${FFI_LIBRARY}")
target_compile_options(jni_bridge PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)

add_library(mock_game SHARED mocks/game.c)
target_include_directories(mock_game PRIVATE "${VITAARC_ROOT}/src/bridges")
add_executable(test_jni test_jni.c test_dispatch.c test_loop.c test_references.c "${VITAARC_ROOT}/src/platform/loop.c")
target_link_libraries(test_jni PRIVATE jni_bridge)
target_compile_options(test_jni PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_compile_options(mock_game PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -UNDEBUG)
set_target_properties(test_jni PROPERTIES ENABLE_EXPORTS ON)

find_file(VITAARC_TEST_FONT DejaVuSans.ttf PATHS /usr/share/fonts/truetype/dejavu)
if(NOT VITAARC_TEST_FONT)
  message(FATAL_ERROR "Text bitmap tests require fonts-dejavu-core or -DVITAARC_TEST_FONT=<font.ttf>")
endif()
add_executable(test_bitmap test_bitmap.c)
target_link_libraries(test_bitmap PRIVATE jni_bridge)
target_compile_options(test_bitmap PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME jni_text_bitmap COMMAND test_bitmap "${VITAARC_TEST_FONT}")

add_executable(test_posix test_posix.c "${VITAARC_ROOT}/src/bridges/libc/posix.c")
target_include_directories(test_posix PRIVATE "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_compile_options(test_posix PRIVATE -Wall -Wextra -Werror -UNDEBUG)

add_executable(test_vita_posix test_vita_posix.c "${VITAARC_ROOT}/src/bridges/libc/posix.c")
target_include_directories(test_vita_posix PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/mocks/include" "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_compile_definitions(test_vita_posix PRIVATE __vita__)
target_compile_options(test_vita_posix PRIVATE -Wall -Wextra -Werror -UNDEBUG)

add_executable(test_threads test_threads.c)
target_link_libraries(test_threads PRIVATE jni_bridge)
target_compile_options(test_threads PRIVATE -Wall -Wextra -Werror -UNDEBUG)

add_executable(test_audio test_audio.c vorbis_fixture.c mocks/audio.c mocks/platform.c
  ${VITAARC_AUDIO_BACKEND_SOURCES} "${VITAARC_ROOT}/src/bridges/audio/mp3.c"
  "${VITAARC_ROOT}/src/bridges/audio/mp3_frame.c" "${VITAARC_ROOT}/src/bridges/audio/mp3_index.c"
  "${VITAARC_ROOT}/src/bridges/audio/mp3_marks.c"
  "${VITAARC_ROOT}/src/bridges/audio/stream.c"
  "${VITAARC_ROOT}/src/bridges/audio/provider.c" "${VITAARC_ROOT}/src/bridges/audio/wav.c")
target_include_directories(test_audio PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/mocks/include" "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges" "${VORBIS_INCLUDE_DIR}")
target_link_libraries(test_audio PRIVATE Threads::Threads m ${CMAKE_DL_LIBS}
  "${VORBISFILE_LIBRARY}" "${VORBISENC_LIBRARY}" "${VORBIS_LIBRARY}" "${OGG_LIBRARY}")
target_link_options(test_audio PRIVATE -Wl,--wrap=ov_read,--wrap=ov_pcm_seek)
target_compile_options(test_audio PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -UNDEBUG)

set(GL_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/gl-fixture/generated")
add_custom_command(
  OUTPUT "${GL_FIXTURE_DIR}/bridge_symbols.h" "${GL_FIXTURE_DIR}/gl_proc_fixture.c"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/generate_gl_fixture.py" "${GL_FIXTURE_DIR}"
  DEPENDS generate_gl_fixture.py "${VITAARC_ROOT}/src/bridges/symbol.def" "${VITAARC_ROOT}/tools/generate_symbols.py"
  VERBATIM)
add_executable(test_gl test_gl.c "${GL_FIXTURE_DIR}/gl_proc_fixture.c" "${GL_FIXTURE_DIR}/bridge_symbols.h"
  "${VITAARC_ROOT}/src/bridge_symbols.c"
  "${VITAARC_ROOT}/src/bridges/opengl/buffers.c"
  "${VITAARC_ROOT}/src/bridges/opengl/pixel_store.c"
  "${VITAARC_ROOT}/src/bridges/opengl/proc.c")
target_include_directories(test_gl PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/gl-fixture" "${CMAKE_CURRENT_LIST_DIR}/mocks/include"
  "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_compile_options(test_gl PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_gl PRIVATE -Wl,--wrap=calloc,--wrap=malloc)

add_executable(test_shader_binary test_shader_binary.c
  "${VITAARC_ROOT}/src/bridges/opengl/shader_binary.c")
target_include_directories(test_shader_binary PRIVATE "${VITAARC_ROOT}/src/bridges")
target_compile_options(test_shader_binary PRIVATE -Wall -Wextra -Werror -UNDEBUG)

set(NATIVE_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/native-fixture.c")
add_custom_command(OUTPUT "${NATIVE_FIXTURE}" "${CMAKE_CURRENT_BINARY_DIR}/native-fixture.h"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/generate_native_fixture.py" "${NATIVE_FIXTURE}"
  DEPENDS generate_native_fixture.py VERBATIM)
add_executable(test_native test_native.c "${NATIVE_FIXTURE}")
target_link_libraries(test_native PRIVATE jni_bridge)
target_compile_options(test_native PRIVATE -Wall -Wextra -Werror -UNDEBUG)

add_executable(test_file test_file.c ${VITAARC_FILE_SOURCES} "${VITAARC_ROOT}/src/bridges/libc/posix.c")
target_include_directories(test_file PRIVATE "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_compile_options(test_file PRIVATE -Wall -Wextra -Werror -UNDEBUG)

add_executable(test_vita_file test_vita_file.c ${VITAARC_FILE_SOURCES} "${VITAARC_ROOT}/src/bridges/libc/posix.c")
target_include_directories(test_vita_file PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/mocks/include" "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_compile_definitions(test_vita_file PRIVATE __vita__)
target_link_libraries(test_vita_file PRIVATE Threads::Threads)
target_compile_options(test_vita_file PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_vita_file PRIVATE -Wl,--wrap=close,--wrap=read,--wrap=write,--wrap=fcntl,--wrap=fsync)

add_executable(test_cxx test_cxx.c "${VITAARC_ROOT}/src/bridges/libc/cxx.c")
target_include_directories(test_cxx PRIVATE "${VITAARC_ROOT}/src/bridges")
target_link_libraries(test_cxx PRIVATE Threads::Threads)
target_compile_options(test_cxx PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_cxx PRIVATE -Wl,--wrap=malloc)

add_executable(test_memory test_memory.c "${VITAARC_ROOT}/src/bridges/libc/memory.c")
target_include_directories(test_memory PRIVATE "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_link_libraries(test_memory PRIVATE Threads::Threads)
target_compile_options(test_memory PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_memory PRIVATE -Wl,--wrap=calloc,--wrap=malloc,--wrap=dup,--wrap=pread,--wrap=mmap,--wrap=munmap)

add_executable(test_directory test_directory.c
  "${VITAARC_ROOT}/src/bridges/libc/directory.c"
  ${VITAARC_FILE_SOURCES}
  "${VITAARC_ROOT}/src/bridges/libc/posix.c")
target_include_directories(test_directory PRIVATE "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_link_libraries(test_directory PRIVATE Threads::Threads)
target_compile_options(test_directory PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_options(test_directory PRIVATE -Wl,--wrap=calloc)

add_executable(test_guest_errno test_guest_errno.c
  "${VITAARC_ROOT}/src/bridges/libc/guest_errno.c"
  ${VITAARC_FILE_SOURCES}
  "${VITAARC_ROOT}/src/bridges/libc/posix.c"
  "${VITAARC_ROOT}/src/bridges/libc/memory.c"
  "${VITAARC_ROOT}/src/bridges/libc/directory.c")
target_include_directories(test_guest_errno PRIVATE "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_link_libraries(test_guest_errno PRIVATE Threads::Threads)
target_compile_options(test_guest_errno PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
target_link_options(test_guest_errno PRIVATE -Wl,--gc-sections)

add_executable(test_poll test_poll.c "${VITAARC_ROOT}/src/bridges/libc/poll.c")
target_include_directories(test_poll PRIVATE "${VITAARC_ROOT}/src/bridges")
target_link_libraries(test_poll PRIVATE Threads::Threads)
target_compile_options(test_poll PRIVATE -Wall -Wextra -Werror -UNDEBUG)

add_executable(test_once test_once.c
  "${VITAARC_ROOT}/src/bridges/libc/once.c"
  "${VITAARC_ROOT}/src/bridges/libc/guest_errno.c")
target_include_directories(test_once PRIVATE "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_link_libraries(test_once PRIVATE Threads::Threads)
target_compile_options(test_once PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
target_link_options(test_once PRIVATE -Wl,--gc-sections,--wrap=calloc,--wrap=free,--wrap=pthread_cond_wait)

add_executable(test_sync test_sync.c
  "${VITAARC_ROOT}/src/bridges/libc/sync.c"
  "${VITAARC_ROOT}/src/bridges/libc/guest_errno.c")
target_include_directories(test_sync PRIVATE "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_link_libraries(test_sync PRIVATE Threads::Threads)
target_compile_options(test_sync PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
target_link_options(test_sync PRIVATE -Wl,--gc-sections,--wrap=calloc,--wrap=free,--wrap=pthread_mutex_destroy)

add_executable(test_thread_attributes test_thread_attributes.c
  "${VITAARC_ROOT}/src/bridges/libc/thread_attributes.c"
  "${VITAARC_ROOT}/src/bridges/libc/pthread.c"
  "${VITAARC_ROOT}/src/bridges/libc/guest_errno.c")
target_include_directories(test_thread_attributes PRIVATE "${VITAARC_ROOT}/src" "${VITAARC_ROOT}/src/bridges")
target_link_libraries(test_thread_attributes PRIVATE Threads::Threads)
target_compile_options(test_thread_attributes PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
target_link_options(test_thread_attributes PRIVATE -Wl,--gc-sections,--wrap=pthread_attr_init,--wrap=pthread_attr_destroy,--wrap=pthread_attr_setstacksize,--wrap=pthread_create)

if(VITAARC_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  foreach(target jni_bridge mock_game test_jni test_bitmap test_posix test_vita_posix test_threads test_audio test_gl test_shader_binary test_native test_file test_vita_file test_cxx test_memory test_directory test_guest_errno test_poll test_once test_sync test_thread_attributes test_log_text)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endforeach()
endif()

if(VITAARC_THREAD_SANITIZER AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  foreach(target jni_bridge mock_game test_jni test_bitmap test_posix test_vita_posix test_threads test_audio test_gl test_shader_binary test_native test_file test_vita_file test_cxx test_memory test_directory test_guest_errno test_poll test_once test_sync test_thread_attributes test_log_text)
    target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=thread)
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "EXECUTABLE")
      target_compile_options(${target} PRIVATE -fno-pie)
      target_link_options(${target} PRIVATE -no-pie)
    endif()
  endforeach()
endif()

add_test(NAME jni_cocos_closed_loop
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/run_tests.py"
    "$<TARGET_FILE:test_jni>" "$<TARGET_FILE:mock_game>" "${VITAARC_TEST_FONT}")
set_tests_properties(jni_cocos_closed_loop PROPERTIES TIMEOUT 60)

add_test(NAME libc_posix
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/run_isolated.py" "$<TARGET_FILE:test_posix>")
set_tests_properties(libc_posix PROPERTIES TIMEOUT 30)

add_test(NAME libc_vita_boundary COMMAND test_vita_posix)
set_tests_properties(libc_vita_boundary PROPERTIES TIMEOUT 10)

add_test(NAME jni_threads COMMAND test_threads)
add_test(NAME jni_vm_shutdown COMMAND test_threads shutdown)
add_test(NAME jni_vm_daemon_shutdown COMMAND test_threads daemon)
set_tests_properties(jni_threads jni_vm_shutdown jni_vm_daemon_shutdown PROPERTIES TIMEOUT 20)

add_test(NAME audio_position_and_seek
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/run_isolated.py" "$<TARGET_FILE:test_audio>")
set_tests_properties(audio_position_and_seek PROPERTIES TIMEOUT 30)

add_test(NAME opengl_objects_and_procs COMMAND test_gl)
add_test(NAME opengl_shader_binary COMMAND test_shader_binary)
set_tests_properties(opengl_objects_and_procs PROPERTIES TIMEOUT 10)

add_test(NAME jni_registered_native_dispatch COMMAND test_native)
set_tests_properties(jni_registered_native_dispatch PROPERTIES TIMEOUT 20)

add_test(NAME libc_file_abi
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/run_isolated.py" "$<TARGET_FILE:test_file>")
set_tests_properties(libc_file_abi PROPERTIES TIMEOUT 30)
add_test(NAME libc_vita_file_boundary COMMAND test_vita_file)
set_tests_properties(libc_vita_file_boundary PROPERTIES TIMEOUT 10)
add_test(NAME libc_cxx_finalizers COMMAND test_cxx)
set_tests_properties(libc_cxx_finalizers PROPERTIES TIMEOUT 20)
add_test(NAME libc_memory_mappings
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/run_isolated.py" "$<TARGET_FILE:test_memory>")
set_tests_properties(libc_memory_mappings PROPERTIES TIMEOUT 30)
add_test(NAME libc_directory_abi
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/run_isolated.py" "$<TARGET_FILE:test_directory>")
set_tests_properties(libc_directory_abi PROPERTIES TIMEOUT 30)
add_test(NAME libc_guest_errno COMMAND test_guest_errno)
set_tests_properties(libc_guest_errno PROPERTIES TIMEOUT 20)
add_test(NAME libc_poll COMMAND test_poll)
set_tests_properties(libc_poll PROPERTIES TIMEOUT 20)
add_test(NAME libc_once_abi COMMAND test_once)
set_tests_properties(libc_once_abi PROPERTIES TIMEOUT 30)
add_test(NAME libc_sync_abi COMMAND test_sync)
set_tests_properties(libc_sync_abi PROPERTIES TIMEOUT 30)
add_test(NAME libc_thread_attributes COMMAND test_thread_attributes)
set_tests_properties(libc_thread_attributes PROPERTIES TIMEOUT 30)
if(NOT VITAARC_THREAD_SANITIZER)
  # This libtsan leaves mutex interception disabled when nanosleep is canceled.
  # Keep that blocking-path test under ASan/UBSan; libc_once_abi still exercises
  # actual pthread_cancel at pthread_testcancel under TSan. See tests/README.md.
  add_test(NAME libc_once_blocking_cancel COMMAND test_once sleep-cancel)
  set_tests_properties(libc_once_blocking_cancel PROPERTIES TIMEOUT 30)
endif()
