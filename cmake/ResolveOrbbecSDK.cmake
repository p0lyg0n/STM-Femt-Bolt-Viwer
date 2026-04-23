function(_stm_append_existing_path list_var path_value)
  if("${path_value}" STREQUAL "")
    set(${list_var} "${${list_var}}" PARENT_SCOPE)
    return()
  endif()

  file(TO_CMAKE_PATH "${path_value}" _normalized_path)
  if(EXISTS "${_normalized_path}")
    set(_paths "${${list_var}}")
    list(APPEND _paths "${_normalized_path}")
    list(REMOVE_DUPLICATES _paths)
    set(${list_var} "${_paths}" PARENT_SCOPE)
    return()
  endif()

  set(${list_var} "${${list_var}}" PARENT_SCOPE)
endfunction()

function(stm_resolve_orbbec_sdk)
  if(NOT DEFINED ORBBEC_SDK_DIR)
    if(DEFINED ENV{ORBBEC_SDK_DIR} AND NOT "$ENV{ORBBEC_SDK_DIR}" STREQUAL "")
      set(_default_orbbec_sdk_dir "$ENV{ORBBEC_SDK_DIR}")
    elseif(WIN32)
      set(_default_orbbec_sdk_dir "C:/Program Files/OrbbecSDK 2.7.6")
    else()
      set(_default_orbbec_sdk_dir "")
    endif()
    set(ORBBEC_SDK_DIR "${_default_orbbec_sdk_dir}" CACHE PATH "Path to OrbbecSDK root")
  endif()

  set(_orbbec_sdk_hints "")
  _stm_append_existing_path(_orbbec_sdk_hints "${ORBBEC_SDK_DIR}")
  _stm_append_existing_path(_orbbec_sdk_hints "${ORBBEC_SDK_DIR}/lib")

  if(_orbbec_sdk_hints)
    find_package(OrbbecSDK CONFIG QUIET PATHS ${_orbbec_sdk_hints})
  endif()
  if(NOT OrbbecSDK_FOUND)
    find_package(OrbbecSDK CONFIG QUIET)
  endif()
  if(NOT OrbbecSDK_FOUND)
    message(FATAL_ERROR
      "Orbbec SDK package not found. "
      "Set ORBBEC_SDK_DIR to the SDK root that contains include/ and lib/OrbbecSDKConfig.cmake."
    )
  endif()

  if(TARGET OrbbecSDK::OrbbecSDK)
    set(_orbbec_target "OrbbecSDK::OrbbecSDK")
  elseif(TARGET ob::OrbbecSDK)
    set(_orbbec_target "ob::OrbbecSDK")
  else()
    message(FATAL_ERROR "Orbbec SDK package was found, but it did not export a known CMake target.")
  endif()

  set(_runtime_files "")
  set(_runtime_dirs "")
  if(NOT "${ORBBEC_SDK_DIR}" STREQUAL "")
    if(WIN32)
      file(GLOB _runtime_files LIST_DIRECTORIES false
        "${ORBBEC_SDK_DIR}/bin/*.dll"
        "${ORBBEC_SDK_DIR}/bin/*.xml"
        "${ORBBEC_SDK_DIR}/bin/*.md"
      )
      if(EXISTS "${ORBBEC_SDK_DIR}/bin/extensions")
        list(APPEND _runtime_dirs "${ORBBEC_SDK_DIR}/bin/extensions")
      endif()
    elseif(APPLE)
      file(GLOB _runtime_files LIST_DIRECTORIES false
        "${ORBBEC_SDK_DIR}/lib/*.dylib"
        "${ORBBEC_SDK_DIR}/lib/*.xml"
        "${ORBBEC_SDK_DIR}/lib/*.md"
      )
      if(EXISTS "${ORBBEC_SDK_DIR}/lib/extensions")
        list(APPEND _runtime_dirs "${ORBBEC_SDK_DIR}/lib/extensions")
      endif()
    else()
      file(GLOB _runtime_files LIST_DIRECTORIES false
        "${ORBBEC_SDK_DIR}/lib/*.so*"
        "${ORBBEC_SDK_DIR}/lib/*.xml"
        "${ORBBEC_SDK_DIR}/lib/*.md"
      )
      if(EXISTS "${ORBBEC_SDK_DIR}/lib/extensions")
        list(APPEND _runtime_dirs "${ORBBEC_SDK_DIR}/lib/extensions")
      endif()
    endif()
  endif()

  list(REMOVE_DUPLICATES _runtime_files)
  list(REMOVE_DUPLICATES _runtime_dirs)
  set(STM_ORBBEC_TARGET "${_orbbec_target}" PARENT_SCOPE)
  set(STM_ORBBEC_RUNTIME_FILES "${_runtime_files}" PARENT_SCOPE)
  set(STM_ORBBEC_RUNTIME_DIRS "${_runtime_dirs}" PARENT_SCOPE)
endfunction()
