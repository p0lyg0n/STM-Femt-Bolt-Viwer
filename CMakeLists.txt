cmake_minimum_required(VERSION 3.20)
project(STMFemtoBoltViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(MSVC)
  add_compile_options(/utf-8)
endif()

include(ResolveOrbbecSDK)
find_package(OpenGL REQUIRED)
stm_resolve_orbbec_sdk()

if(TARGET OpenGL::GL)
  set(STM_OPENGL_TARGET OpenGL::GL)
elseif(TARGET OpenGL::OpenGL)
  set(STM_OPENGL_TARGET OpenGL::OpenGL)
else()
  message(FATAL_ERROR "No cross-platform OpenGL target exported by find_package(OpenGL).")
endif()

include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/_imgui_cache" CACHE PATH "" FORCE)
# Keep the downloaded Dear ImGui source checkout, but drop generated subbuild state
# so the cache can be reused across different absolute checkout paths.
file(REMOVE_RECURSE
  "${FETCHCONTENT_BASE_DIR}/imgui-subbuild"
  "${FETCHCONTENT_BASE_DIR}/imgui-build"
)
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
target_link_libraries(imgui_lib PUBLIC ${STM_OPENGL_TARGET})

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

add_executable(stm_femto_bolt_viewer
  src/main.cpp
  src/usb_topology.cpp
  src/frame_processing.cpp
  src/camera_session.cpp
  src/gl_utils.cpp
  src/render.cpp
  src/input.cpp
  src/i18n.cpp
  src/app_settings.cpp
  src/log_util.cpp
)

target_include_directories(stm_femto_bolt_viewer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(stm_femto_bolt_viewer PRIVATE
  ${STM_ORBBEC_TARGET}
  ${GLFW_TARGET}
  ${STM_OPENGL_TARGET}
  imgui_lib
)

foreach(RUNTIME_FILE IN LISTS STM_ORBBEC_RUNTIME_FILES)
  if(EXISTS "${RUNTIME_FILE}")
    add_custom_command(TARGET stm_femto_bolt_viewer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${RUNTIME_FILE}"
      "$<TARGET_FILE_DIR:stm_femto_bolt_viewer>"
    )
  endif()
endforeach()

foreach(RUNTIME_DIR IN LISTS STM_ORBBEC_RUNTIME_DIRS)
  if(EXISTS "${RUNTIME_DIR}")
    add_custom_command(TARGET stm_femto_bolt_viewer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${RUNTIME_DIR}"
      "$<TARGET_FILE_DIR:stm_femto_bolt_viewer>/extensions"
    )
  endif()
endforeach()
