# Piper-Phonemize Patches for Position Tracking

## Overview

The modifications to piper-phonemize are currently applied to the downloaded source in:
`build-test/_deps/piper_phonemize-src/src/`

These changes need to be:
1. Contributed upstream to piper-phonemize repository
2. Or maintained as a fork referenced in `cmake/piper-phonemize.cmake`

## Modified Files

### `phonemize.hpp`
- Added `PhonemePosition` struct
- Added `phonemize_eSpeak_with_positions()` function declaration

### `phonemize.cpp`
- Implemented `phonemize_eSpeak_with_positions()` using espeak_Synth callbacks
- Added callback system to capture espeak_EVENT data with positions

## Patches Location

The modified files are located at:
- `/Users/zachswift/projects/sherpa-onnx/build-test/_deps/piper_phonemize-src/src/phonemize.hpp`
- `/Users/zachswift/projects/sherpa-onnx/build-test/_deps/piper_phonemize-src/src/phonemize.cpp`

## How to Create Patches

```bash
# From build-test/_deps/piper_phonemize-src
git diff src/phonemize.hpp > ../../piper-phonemize-position-tracking-hpp.patch
git diff src/phonemize.cpp > ../../piper-phonemize-position-tracking-cpp.patch
```

## How to Apply Patches

If piper-phonemize is updated, reapply these patches:

```bash
cd build-test/_deps/piper_phonemize-src
patch -p1 < ../../piper-phonemize-position-tracking-hpp.patch
patch -p1 < ../../piper-phonemize-position-tracking-cpp.patch
```

## Alternative: Fork Piper-Phonemize

A better long-term solution is to fork piper-phonemize:

1. Fork https://github.com/rhasspy/piper-phonemize
2. Apply the changes
3. Update `cmake/piper-phonemize.cmake` to point to the fork:
   ```cmake
   set(piper_phonemize_URL "https://github.com/YOUR_USERNAME/piper-phonemize/archive/YOUR_COMMIT.zip")
   ```

## Future Work

- [ ] Create pull request to upstream piper-phonemize
- [ ] Or create maintained fork
- [ ] Update sherpa-onnx to use modified version
- [ ] Add configuration option to enable/disable position tracking
