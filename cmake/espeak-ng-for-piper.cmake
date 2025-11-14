function(download_espeak_ng_for_piper)
  include(FetchContent)

  # Use local espeak-ng fork with normalized text support
  # Instead of fetching from URL, use the local directory with our modifications
  set(espeak_ng_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../espeak-ng")
  set(espeak_ng_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/espeak-ng-build")

  set(BUILD_ESPEAK_NG_TESTS OFF CACHE BOOL "" FORCE)
  set(USE_ASYNC OFF CACHE BOOL "" FORCE)
  set(USE_MBROLA OFF CACHE BOOL "" FORCE)
  set(USE_LIBSONIC OFF CACHE BOOL "" FORCE)
  set(USE_LIBPCAUDIO OFF CACHE BOOL "" FORCE)
  set(USE_KLATT OFF CACHE BOOL "" FORCE)
  set(USE_SPEECHPLAYER OFF CACHE BOOL "" FORCE)
  set(EXTRA_cmn ON CACHE BOOL "" FORCE)
  set(EXTRA_ru ON CACHE BOOL "" FORCE)
  if (NOT SHERPA_ONNX_ENABLE_EPSEAK_NG_EXE)
    set(BUILD_ESPEAK_NG_EXE OFF CACHE BOOL "" FORCE)
  endif()

  message(STATUS "Using local espeak-ng from ${espeak_ng_SOURCE_DIR}")
  message(STATUS "espeak-ng binary dir is ${espeak_ng_BINARY_DIR}")

  if(BUILD_SHARED_LIBS)
    set(_build_shared_libs_bak ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
  endif()

  add_subdirectory(${espeak_ng_SOURCE_DIR} ${espeak_ng_BINARY_DIR})

  if(_build_shared_libs_bak)
    set_target_properties(espeak-ng
      PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
    )
    set(BUILD_SHARED_LIBS ON)
  endif()

  set(espeak_ng_SOURCE_DIR ${espeak_ng_SOURCE_DIR} PARENT_SCOPE)

  if(WIN32 AND MSVC)
    target_compile_options(ucd PUBLIC
      /wd4309
    )

    target_compile_options(espeak-ng PUBLIC
      /wd4005
      /wd4018
      /wd4067
      /wd4068
      /wd4090
      /wd4101
      /wd4244
      /wd4267
      /wd4996
    )

    if(TARGET espeak-ng-bin)
      target_compile_options(espeak-ng-bin PRIVATE
        /wd4244
        /wd4024
        /wd4047
        /wd4067
        /wd4267
        /wd4996
      )
    endif()
  endif()

  if(UNIX AND NOT APPLE)
    target_compile_options(espeak-ng PRIVATE
      -Wno-unused-result
      -Wno-format-overflow
      -Wno-format-truncation
      -Wno-uninitialized
      -Wno-format
    )

    if(TARGET espeak-ng-bin)
      target_compile_options(espeak-ng-bin PRIVATE
        -Wno-unused-result
      )
    endif()
  endif()

  target_include_directories(espeak-ng
    INTERFACE
      ${espeak_ng_SOURCE_DIR}/src/include
      ${espeak_ng_SOURCE_DIR}/src/ucd-tools/src/include
  )

  if(NOT BUILD_SHARED_LIBS)
    install(TARGETS
      espeak-ng
      ucd
    DESTINATION lib)
  endif()
endfunction()

download_espeak_ng_for_piper()
