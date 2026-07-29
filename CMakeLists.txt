## This file is a quick tutorial on writing CMakeLists for targeting the Vita
cmake_minimum_required(VERSION 3.12)

## Keep all generated files out of the source tree.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are disabled. Configure with: cmake -S . -B build")
endif()

## This includes the Vita toolchain, must go before project definition
# It is a convenience so you do not have to type
# -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake for cmake. It is
# highly recommended that you include this block for all projects.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(VITA_SDK_ROOT "$ENV{VITASDK}")
  elseif(EXISTS "/usr/local/vitasdk/share/vita.toolchain.cmake")
    set(VITA_SDK_ROOT "/usr/local/vitasdk")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
  set(CMAKE_TOOLCHAIN_FILE
    "${VITA_SDK_ROOT}/share/vita.toolchain.cmake"
    CACHE PATH "toolchain file")
endif()

## Define project parameters here
# Name of the project
project(VitaArc)
# This line adds Vita helper macros, must go after project definition in order
# to build Vita specific artifacts (self/vpk).
include("${VITASDK}/share/vita.cmake" REQUIRED)

## Configuration options for this app
# Display name (under bubble in LiveArea)
set(VITA_APP_NAME "VitaArc")
# Unique ID must be exactly 9 characters. Recommended: XXXXYYYYY where X =
# unique string of developer and Y = a unique number for this app
set(VITA_TITLEID  "VARC61616")
# Optional version string to show in LiveArea's more info screen
set(VITA_VERSION  "01.00")

## Flags and includes for building
# Note that we make sure not to overwrite previous flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
# Optional. You can specify more param.sfo flags this way.
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1")

# Add any additional include paths here
include_directories(
  "src"
  "src/bridges"
  "third-party"
)

# Add any additional library paths here
# ${CMAKE_CURRENT_BINARY_DIR} lets you use any library currently being built
link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

## Collect every user-mode C/C++ source under src. CONFIGURE_DEPENDS causes
## CMake to regenerate the list when a source file is added or removed.
file(GLOB_RECURSE VITAARC_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

## Build and link
add_executable(${PROJECT_NAME} ${VITAARC_SOURCES})

# Library to link to (drop the -l prefix). This will mostly be stubs.
target_link_libraries(${PROJECT_NAME}
  -Wl,--whole-archive pthread -Wl,--no-whole-archive
  m
  stdc++
  vitaGL
  vitashark
  mathneon
  SceKernelDmacMgr_stub
  SceShaccCgExt

  taihen_stub
  kubridge_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceAudio_stub
  SceAudioIn_stub
  SceCtrl_stub
  SceCommonDialog_stub
  SceDisplay_stub
  SceFios2_stub
  SceGxm_stub
  SceShaccCg_stub
  SceSysmodule_stub
  ScePower_stub
  SceTouch_stub
  SceVshBridge_stub
  SceNetCtl_stub
  SceRtc_stub
)

## Create Vita files
vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
# vita_create_vpk concatenates resource paths into a command-line string before
# splitting it. That breaks source paths containing spaces, so keep every
# resource mapping as an individual CMake argument instead.
set(VITA_SELF "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.self")
set(VITA_SFO "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.vpk_param.sfo")
set(VITA_VPK_OUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.vpk.out")
set(VITA_VPK "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.vpk")

add_custom_command(
  OUTPUT "${VITA_SFO}"
  COMMAND "${VITA_MKSFOEX}"
    -d PARENTAL_LEVEL=1
    -d ATTRIBUTE2=12
    -s "APP_VER=${VITA_VERSION}"
    -s "TITLE_ID=${VITA_TITLEID}"
    "${VITA_APP_NAME}"
    "${VITA_SFO}"
  DEPENDS "${VITA_SELF}"
  COMMENT "Generating param.sfo for ${PROJECT_NAME}.vpk"
  VERBATIM
)

add_custom_command(
  OUTPUT "${VITA_VPK_OUT}"
  COMMAND "${VITA_PACK_VPK}"
    -a "${CMAKE_CURRENT_SOURCE_DIR}/livearea/icon0.png=sce_sys/icon0.png"
    -a "${CMAKE_CURRENT_SOURCE_DIR}/livearea/background.png=sce_sys/livearea/contents/background.png"
    -a "${CMAKE_CURRENT_SOURCE_DIR}/livearea/startup.png=sce_sys/livearea/contents/startup.png"
    -a "${CMAKE_CURRENT_SOURCE_DIR}/livearea/template.xml=sce_sys/livearea/contents/template.xml"
    -s "${VITA_SFO}"
    -b "${VITA_SELF}"
    "${VITA_VPK_OUT}"
  DEPENDS
    "${VITA_SFO}"
    "${VITA_SELF}"
    "${CMAKE_CURRENT_SOURCE_DIR}/livearea/icon0.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/livearea/background.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/livearea/startup.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/livearea/template.xml"
  COMMENT "Building vpk ${PROJECT_NAME}.vpk"
  VERBATIM
)

add_custom_target(${PROJECT_NAME}.vpk-vpk ALL
  DEPENDS "${VITA_VPK_OUT}"
  COMMAND "${CMAKE_COMMAND}" -E copy "${VITA_VPK_OUT}" "${VITA_VPK}"
  BYPRODUCTS "${VITA_VPK}"
)
add_dependencies(${PROJECT_NAME}.vpk-vpk ${PROJECT_NAME}.self-self)
