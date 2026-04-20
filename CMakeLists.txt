cmake_minimum_required(VERSION 3.20)
project(STMFemtoBoltViewerVer1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8)
endif()

set(ORBBEC_SDK_DIR "${CMAKE_SOURCE_DIR}/../legacy_20260420_pre_rebuild/_vendor/OrbbecSDK" CACHE PATH "Path to OrbbecSDK root")
if(NOT EXISTS "${ORBBEC_SDK_DIR}/include/libobsensor/ObSensor.hpp")
  message(FATAL_ERROR "Orbbec SDK headers not found. ORBBEC_SDK_DIR=${ORBBEC_SDK_DIR}")
endif()

find_package(glfw3 CONFIG QUIET)

set(ORBBEC_SAMPLE_VIEWER_GLFW_DIR "${CMAKE_SOURCE_DIR}/../_tmp_orbbec_sample_viewer/extern/glfw" CACHE PATH "Path to GLFW source from Orbbec Sample Viewer")
if(NOT TARGET glfw AND NOT TARGET glfw3::glfw)
  if(EXISTS "${ORBBEC_SAMPLE_VIEWER_GLFW_DIR}/CMakeLists.txt")
    add_subdirectory("${ORBBEC_SAMPLE_VIEWER_GLFW_DIR}" "${CMAKE_BINARY_DIR}/_deps/glfw" EXCLUDE_FROM_ALL)
  endif()
endif()

if(TARGET glfw)
  set(GLFW_TARGET glfw)
elseif(TARGET glfw3::glfw)
  set(GLFW_TARGET glfw3::glfw)
else()
  message(FATAL_ERROR "GLFW target not found. Install glfw3 or set ORBBEC_SAMPLE_VIEWER_GLFW_DIR to a valid glfw source directory.")
endif()

set(ORBBEC_LIB_DIR "${ORBBEC_SDK_DIR}/lib/win_x64")
set(ORBBEC_LIB "${ORBBEC_LIB_DIR}/OrbbecSDK.lib")
if(NOT EXISTS "${ORBBEC_LIB}")
  message(FATAL_ERROR "OrbbecSDK.lib not found at ${ORBBEC_LIB}")
endif()

add_executable(stm_femto_bolt_viewer_ver1
  src/main.cpp
)

target_include_directories(stm_femto_bolt_viewer_ver1 PRIVATE
  ${ORBBEC_SDK_DIR}/include
)

target_link_libraries(stm_femto_bolt_viewer_ver1 PRIVATE
  ${ORBBEC_LIB}
  ${GLFW_TARGET}
  opengl32
)

set(ORBBEC_RUNTIME_DLLS
  "${ORBBEC_LIB_DIR}/OrbbecSDK.dll"
  "${ORBBEC_LIB_DIR}/ob_usb.dll"
  "${ORBBEC_LIB_DIR}/depthengine_2_0.dll"
  "${ORBBEC_LIB_DIR}/live555.dll"
)

foreach(DLL_PATH IN LISTS ORBBEC_RUNTIME_DLLS)
  if(EXISTS "${DLL_PATH}")
    add_custom_command(TARGET stm_femto_bolt_viewer_ver1 POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${DLL_PATH}"
      "$<TARGET_FILE_DIR:stm_femto_bolt_viewer_ver1>"
    )
  endif()
endforeach()
