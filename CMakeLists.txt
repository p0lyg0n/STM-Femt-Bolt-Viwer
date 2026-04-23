cmake_minimum_required(VERSION 3.20)
project(STMFemtoBoltViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CTest)

option(STM_BUILD_APP "Build the GUI viewer application" ON)
option(STM_ENABLE_COVERAGE "Enable LLVM coverage instrumentation for unit tests" OFF)

if(MSVC)
  add_compile_options(/utf-8)
endif()

add_library(stm_core STATIC
  src/i18n.cpp
  src/settings_codec.cpp
  src/pixel_conversion.cpp
  src/usb_topology_format.cpp
)
target_include_directories(stm_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(BUILD_TESTING)
  find_package(Catch2 3 CONFIG QUIET)
  if(NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.5.4
      GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  endif()
  include(Catch)

  add_executable(stm_unit_tests
    tests/unit_tests.cpp
  )
  target_link_libraries(stm_unit_tests PRIVATE stm_core Catch2::Catch2WithMain)
  catch_discover_tests(stm_unit_tests)
endif()

if(STM_ENABLE_COVERAGE)
  if(NOT BUILD_TESTING)
    message(FATAL_ERROR "STM_ENABLE_COVERAGE requires BUILD_TESTING=ON.")
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "STM_ENABLE_COVERAGE currently requires a Clang-family compiler.")
  endif()

  target_compile_options(stm_core PRIVATE -O0 -g -fprofile-instr-generate -fcoverage-mapping)
  target_compile_options(stm_unit_tests PRIVATE -O0 -g -fprofile-instr-generate -fcoverage-mapping)
  target_link_options(stm_unit_tests PRIVATE -fprofile-instr-generate -fcoverage-mapping)

  if(APPLE)
    set(STM_LLVM_PROFDATA_COMMAND xcrun llvm-profdata)
    set(STM_LLVM_COV_COMMAND xcrun llvm-cov)
  else()
    find_program(STM_LLVM_PROFDATA_EXECUTABLE NAMES llvm-profdata REQUIRED)
    find_program(STM_LLVM_COV_EXECUTABLE NAMES llvm-cov REQUIRED)
    set(STM_LLVM_PROFDATA_COMMAND ${STM_LLVM_PROFDATA_EXECUTABLE})
    set(STM_LLVM_COV_COMMAND ${STM_LLVM_COV_EXECUTABLE})
  endif()

  set(STM_COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")
  set(STM_COVERAGE_PROFILE "${STM_COVERAGE_DIR}/stm_unit_tests.profraw")
  set(STM_COVERAGE_PROFDATA "${STM_COVERAGE_DIR}/stm_unit_tests.profdata")
  set(STM_COVERAGE_IGNORE_REGEX ".*/_deps/.*|.*/tests/.*|.*/third_party/.*")

  add_custom_target(coverage
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${STM_COVERAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${STM_COVERAGE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${STM_COVERAGE_PROFILE}" "$<TARGET_FILE:stm_unit_tests>"
    COMMAND ${STM_LLVM_PROFDATA_COMMAND} merge -sparse "${STM_COVERAGE_PROFILE}" -o "${STM_COVERAGE_PROFDATA}"
    COMMAND ${STM_LLVM_COV_COMMAND} report "$<TARGET_FILE:stm_unit_tests>" "-instr-profile=${STM_COVERAGE_PROFDATA}" -ignore-filename-regex "${STM_COVERAGE_IGNORE_REGEX}"
    COMMAND ${STM_LLVM_COV_COMMAND} show "$<TARGET_FILE:stm_unit_tests>" "-instr-profile=${STM_COVERAGE_PROFDATA}" -ignore-filename-regex "${STM_COVERAGE_IGNORE_REGEX}" "-format=html" "-output-dir=${STM_COVERAGE_DIR}/html"
    DEPENDS stm_unit_tests
    USES_TERMINAL
    VERBATIM
  )
endif()

if(STM_BUILD_APP)
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
    src/app_settings.cpp
    src/log_util.cpp
  )

  target_include_directories(stm_femto_bolt_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(stm_femto_bolt_viewer PRIVATE
    stm_core
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
endif()
