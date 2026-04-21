cmake_minimum_required(VERSION 3.20)
project(STMFemtoBoltViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8)
endif()

include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/_imgui_cache" CACHE PATH "" FORCE)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.91.6
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
)
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  "${CMAKE_SOURCE_DIR}/third_party/glfw/include"
)
target_link_libraries(imgui_lib PUBLIC opengl32)

set(ORBBEC_SDK_DIR "C:/Program Files/OrbbecSDK 2.7.6" CACHE PATH "Path to OrbbecSDK root")
if(NOT EXISTS "${ORBBEC_SDK_DIR}/include/libobsensor/ObSensor.hpp")
  message(FATAL_ERROR "Orbbec SDK headers not found. ORBBEC_SDK_DIR=${ORBBEC_SDK_DIR}")
endif()

find_package(glfw3 CONFIG QUIET)

set(ORBBEC_SAMPLE_VIEWER_GLFW_DIR "${CMAKE_SOURCE_DIR}/third_party/glfw" CACHE PATH "Path to GLFW source")
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

set(ORBBEC_LIB "")
set(ORBBEC_LIB_CANDIDATES
  "${ORBBEC_SDK_DIR}/lib/win_x64/OrbbecSDK.lib"
  "${ORBBEC_SDK_DIR}/lib/OrbbecSDK.lib"
)
foreach(CANDIDATE IN LISTS ORBBEC_LIB_CANDIDATES)
  if(EXISTS "${CANDIDATE}")
    set(ORBBEC_LIB "${CANDIDATE}")
    break()
  endif()
endforeach()
if(ORBBEC_LIB STREQUAL "")
  message(FATAL_ERROR "OrbbecSDK.lib not found. Checked: ${ORBBEC_LIB_CANDIDATES}")
endif()

set(ORBBEC_RUNTIME_DIR "")
set(ORBBEC_RUNTIME_CANDIDATES
  "${ORBBEC_SDK_DIR}/bin"
  "${ORBBEC_SDK_DIR}/lib/win_x64"
)
foreach(CANDIDATE IN LISTS ORBBEC_RUNTIME_CANDIDATES)
  if(EXISTS "${CANDIDATE}/OrbbecSDK.dll")
    set(ORBBEC_RUNTIME_DIR "${CANDIDATE}")
    break()
  endif()
endforeach()

add_executable(stm_femto_bolt_viewer
  src/main.cpp
  src/usb_topology.cpp
  src/frame_processing.cpp
  src/camera_session.cpp
  src/gl_utils.cpp
  src/render.cpp
  src/input.cpp
  src/i18n.cpp
)

target_include_directories(stm_femto_bolt_viewer PRIVATE
  ${ORBBEC_SDK_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(stm_femto_bolt_viewer PRIVATE
  ${ORBBEC_LIB}
  ${GLFW_TARGET}
  opengl32
  imgui_lib
)

set(ORBBEC_RUNTIME_DLLS "")
if(NOT ORBBEC_RUNTIME_DIR STREQUAL "")
  set(ORBBEC_RUNTIME_DLLS
    "${ORBBEC_RUNTIME_DIR}/OrbbecSDK.dll"
    "${ORBBEC_RUNTIME_DIR}/ob_usb.dll"
    "${ORBBEC_RUNTIME_DIR}/depthengine_2_0.dll"
    "${ORBBEC_RUNTIME_DIR}/live555.dll"
  )
endif()

foreach(DLL_PATH IN LISTS ORBBEC_RUNTIME_DLLS)
  if(EXISTS "${DLL_PATH}")
    add_custom_command(TARGET stm_femto_bolt_viewer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${DLL_PATH}"
      "$<TARGET_FILE_DIR:stm_femto_bolt_viewer>"
    )
  endif()
endforeach()

if(NOT ORBBEC_RUNTIME_DIR STREQUAL "")
  if(EXISTS "${ORBBEC_RUNTIME_DIR}/extensions")
    add_custom_command(TARGET stm_femto_bolt_viewer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ORBBEC_RUNTIME_DIR}/extensions"
      "$<TARGET_FILE_DIR:stm_femto_bolt_viewer>/extensions"
    )
  endif()
endif()
