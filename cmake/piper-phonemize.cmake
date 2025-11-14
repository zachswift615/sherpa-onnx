function(download_piper_phonemize)
  include(FetchContent)

  # Use local piper-phonemize with normalized text support
  # Instead of fetching from git, use the local directory with our modifications
  set(piper_phonemize_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../piper-phonemize")
  set(piper_phonemize_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/piper-phonemize-build")

  message(STATUS "Using local piper-phonemize from ${piper_phonemize_SOURCE_DIR}")
  message(STATUS "piper-phonemize binary dir is ${piper_phonemize_BINARY_DIR}")

  if(BUILD_SHARED_LIBS)
    set(_build_shared_libs_bak ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
  endif()

  # Tell piper-phonemize to use sherpa-onnx's onnxruntime instead of downloading its own
  if(IOS OR BUILD_PIPER_PHONEME_CPP_EXE)
    # For iOS builds and when building piper_phonemize_exe, we need to use sherpa-onnx's onnxruntime
    # Create a minimal directory structure that piper-phonemize expects
    if(DEFINED ENV{SHERPA_ONNXRUNTIME_INCLUDE_DIR} AND DEFINED ENV{SHERPA_ONNXRUNTIME_LIB_DIR})
      # iOS build uses environment variables
      set(ONNXRUNTIME_DIR ${CMAKE_BINARY_DIR}/onnxruntime-ios-shim)
      file(MAKE_DIRECTORY ${ONNXRUNTIME_DIR}/include)
      file(MAKE_DIRECTORY ${ONNXRUNTIME_DIR}/lib)
      # Create symlinks to the actual header files and library file in lib directory
      # Note: We symlink individual files rather than the directory itself
      file(GLOB onnx_headers "$ENV{SHERPA_ONNXRUNTIME_INCLUDE_DIR}/*.h")
      foreach(header ${onnx_headers})
        get_filename_component(header_name ${header} NAME)
        execute_process(COMMAND ln -sf ${header} ${ONNXRUNTIME_DIR}/include/${header_name})
      endforeach()
      # Symlink the library file directly into lib/
      execute_process(COMMAND ln -sf $ENV{SHERPA_ONNXRUNTIME_LIB_DIR}/libonnxruntime.a ${ONNXRUNTIME_DIR}/lib/libonnxruntime.a)
      message(STATUS "Using iOS onnxruntime from: include=$ENV{SHERPA_ONNXRUNTIME_INCLUDE_DIR}, lib=$ENV{SHERPA_ONNXRUNTIME_LIB_DIR}")
    elseif(DEFINED onnxruntime_SOURCE_DIR)
      set(ONNXRUNTIME_DIR ${onnxruntime_SOURCE_DIR})
    endif()
  endif()

  add_subdirectory(${piper_phonemize_SOURCE_DIR} ${piper_phonemize_BINARY_DIR} EXCLUDE_FROM_ALL)

  if(_build_shared_libs_bak)
    set_target_properties(piper_phonemize
      PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
    )
    set(BUILD_SHARED_LIBS ON)
  endif()

  if(WIN32 AND MSVC)
    target_compile_options(piper_phonemize PUBLIC
      /wd4309
    )
  endif()

  target_include_directories(piper_phonemize
    INTERFACE
      ${piper_phonemize_SOURCE_DIR}/src/include
  )

  if(NOT BUILD_SHARED_LIBS)
    install(TARGETS
      piper_phonemize
    DESTINATION lib)
  endif()
endfunction()

download_piper_phonemize()
