#!/usr/bin/env bash
# Listen2 fork: sherpa-onnx.xcframework without espeak-ng and piper-phonemize
# (-DSHERPA_ONNX_ENABLE_ESPEAK=OFF). Piper VITS voices then take ready-made
# phonemes only (text starting with "[[l2-phonemes]]").
#
# Slices, the same as the shipping framework: iOS device (arm64), iOS simulator
# (arm64 + x86_64) and Mac Catalyst (arm64).
#
# onnxruntime is not downloaded: point ORT_XCFRAMEWORK at the one the app
# already links (default: ~/projects/Listen2/Frameworks/onnxruntime.xcframework).
set -e

dir=build-ios-no-espeak
ORT_XCFRAMEWORK=${ORT_XCFRAMEWORK:-$HOME/projects/Listen2/Frameworks/onnxruntime.xcframework}
if [ ! -f "$ORT_XCFRAMEWORK/ios-arm64/libonnxruntime.a" ]; then
  echo "onnxruntime.xcframework not found at $ORT_XCFRAMEWORK"
  exit 1
fi

mkdir -p $dir
cd $dir

export SHERPA_ONNXRUNTIME_INCLUDE_DIR=$ORT_XCFRAMEWORK/Headers

common_flags=(
  -S ..
  -DCMAKE_TOOLCHAIN_FILE=./toolchains/ios.toolchain.cmake
  -DENABLE_BITCODE=0
  -DENABLE_ARC=1
  -DENABLE_VISIBILITY=0
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX=./install
  -DBUILD_SHARED_LIBS=OFF
  -DSHERPA_ONNX_ENABLE_ESPEAK=OFF
  -DSHERPA_ONNX_ENABLE_PYTHON=OFF
  -DSHERPA_ONNX_ENABLE_TESTS=OFF
  -DSHERPA_ONNX_ENABLE_CHECK=OFF
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF
  -DSHERPA_ONNX_ENABLE_JNI=OFF
  -DSHERPA_ONNX_ENABLE_C_API=ON
  -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF
  -DDEPLOYMENT_TARGET=13.0
)

echo "Building for simulator (arm64)"
export SHERPA_ONNXRUNTIME_LIB_DIR=$ORT_XCFRAMEWORK/ios-arm64_x86_64-simulator
cmake "${common_flags[@]}" -DPLATFORM=SIMULATORARM64 -B build/simulator_arm64
cmake --build build/simulator_arm64 -j ${JOBS:-4}

echo "Building for simulator (x86_64)"
cmake "${common_flags[@]}" -DPLATFORM=SIMULATOR64 -B build/simulator_x86_64
cmake --build build/simulator_x86_64 -j ${JOBS:-4}

echo "Building for Mac Catalyst (arm64-macabi)"
# As build-catalyst.sh: the ios toolchain does not emit macabi objects with
# Makefiles, so force the Catalyst target triple instead.
macosx_sdk=$(xcrun --sdk macosx --show-sdk-path)
macabi_target=arm64-apple-ios14.0-macabi
export SHERPA_ONNXRUNTIME_LIB_DIR=$ORT_XCFRAMEWORK/ios-arm64
cmake -S .. \
  -DSHERPA_ONNX_ENABLE_BINARY=OFF \
  -DCMAKE_OSX_SYSROOT=$macosx_sdk \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_C_FLAGS="--target=$macabi_target" \
  -DCMAKE_CXX_FLAGS="--target=$macabi_target" \
  -DCMAKE_ASM_FLAGS="--target=$macabi_target" \
  -DCMAKE_INSTALL_PREFIX=./install-catalyst \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DSHERPA_ONNX_ENABLE_ESPEAK=OFF \
  -DSHERPA_ONNX_ENABLE_PYTHON=OFF \
  -DSHERPA_ONNX_ENABLE_TESTS=OFF \
  -DSHERPA_ONNX_ENABLE_CHECK=OFF \
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF \
  -DSHERPA_ONNX_ENABLE_JNI=OFF \
  -DSHERPA_ONNX_ENABLE_C_API=ON \
  -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF \
  -B build/catalyst_arm64
cmake --build build/catalyst_arm64 -j ${JOBS:-4}

echo "Building for iOS device (arm64)"
export SHERPA_ONNXRUNTIME_LIB_DIR=$ORT_XCFRAMEWORK/ios-arm64
cmake "${common_flags[@]}" -DPLATFORM=OS64 -B build/os64
cmake --build build/os64 -j ${JOBS:-4}
cmake --build build/os64 --target install

echo "Generate xcframework"
libs="libkaldi-native-fbank-core.a libkissfft-float.a libsherpa-onnx-c-api.a libsherpa-onnx-core.a
      libsherpa-onnx-fstfar.a libsherpa-onnx-fst.a libsherpa-onnx-kaldifst-core.a libkaldi-decoder-core.a
      libssentencepiece_core.a"
for slice in simulator_arm64 simulator_x86_64 catalyst_arm64 os64; do
  args=()
  for f in $libs; do args+=("build/$slice/lib/$f"); done
  libtool -static -o build/$slice/libsherpa-onnx.a "${args[@]}"
  if nm build/$slice/libsherpa-onnx.a 2>/dev/null | grep -q "espeak_\(Initialize\|TextToPhonemes\|ng_\)"; then
    echo "ERROR: espeak symbols found in $slice"
    exit 1
  fi
done

mkdir -p build/simulator
lipo -create build/simulator_arm64/libsherpa-onnx.a build/simulator_x86_64/libsherpa-onnx.a \
     -output build/simulator/libsherpa-onnx.a

rm -rf sherpa-onnx.xcframework
xcodebuild -create-xcframework \
      -library "build/os64/libsherpa-onnx.a" -headers install/include \
      -library "build/simulator/libsherpa-onnx.a" -headers install/include \
      -library "build/catalyst_arm64/libsherpa-onnx.a" -headers install/include \
      -output sherpa-onnx.xcframework
# The app's sherpa-onnx.xcframework also carries the ONNX Runtime headers
# (onnxruntime_c_api.h, coreml_provider_factory.h, ...); copy them into both
# slices from the framework the app ships.
ORT_HEADERS_FROM=${ORT_HEADERS_FROM:-$HOME/projects/Listen2/Frameworks/sherpa-onnx.xcframework/ios-arm64/Headers}
for slice in ios-arm64 ios-arm64_x86_64-simulator ios-arm64-maccatalyst; do
  cp "$ORT_HEADERS_FROM"/onnxruntime_*.h "$ORT_HEADERS_FROM"/*_provider_factory.h \
     sherpa-onnx.xcframework/$slice/Headers/
done
echo "Done: $PWD/sherpa-onnx.xcframework (no espeak-ng)"
