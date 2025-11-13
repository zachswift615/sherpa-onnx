# Building sherpa-onnx with Phoneme Position Tracking

**Created**: 2025-11-12
**Author**: Zach Swift
**Purpose**: Document the complete build process for sherpa-onnx with phoneme sequence tracking for Listen2 app

## Overview

This guide documents how to build sherpa-onnx with phoneme position tracking support. The modifications enable precise word-level timing alignment in TTS by tracking which source text characters generated each phoneme.

**What This Adds**:
- Phoneme symbols (IPA format) for each phoneme generated
- Character offset tracking (maps phonemes to source text positions)
- Character length tracking (handles multi-character phonemes)
- C API exposure for Swift integration

**Use Case**: Listen2 app uses this to highlight individual words as they're spoken during TTS playback.

---

## Prerequisites

### Required Tools
- **macOS**: 12.0 or later (for iOS development)
- **Xcode**: 14.0+ with Command Line Tools
- **CMake**: 3.14 or later
- **Git**: For cloning repositories
- **Python 3**: (optional) For model re-export if needed

### Install Command Line Tools
```bash
xcode-select --install
```

### Install CMake
```bash
brew install cmake
```

### Verify Prerequisites
```bash
cmake --version      # Should show 3.14+
xcodebuild -version  # Should show Xcode 14.0+
git --version        # Any recent version
```

---

## Part 1: Fork and Modify piper-phonemize

### Step 1.1: Fork the Repository

**Original Repository**: https://github.com/rhasspy/piper-phonemize

1. Go to https://github.com/rhasspy/piper-phonemize
2. Click "Fork" button
3. Fork to your GitHub account (e.g., `zachswift615/piper-phonemize`)

### Step 1.2: Clone Your Fork

```bash
cd ~/projects
git clone git@github.com:YOUR_USERNAME/piper-phonemize.git
cd piper-phonemize
```

### Step 1.3: Create Feature Branch

```bash
git checkout -b feature/espeak-position-tracking
```

### Step 1.4: Add Phoneme Position Tracking

#### Modify `src/phonemize.hpp`

Add the `PhonemePosition` struct and new function declaration:

```cpp
// After the eSpeakPhonemeConfig struct (around line 46)

// Position information for a single phoneme
struct PhonemePosition {
  int32_t text_position;  // Character offset in source text
  int32_t length;         // Number of characters
};

// After phonemize_eSpeak function declaration (around line 60)

// Phonemizes text using espeak-ng with position tracking.
// Returns phonemes and their corresponding source text positions.
//
// Assumes espeak_Initialize has already been called.
PIPERPHONEMIZE_EXPORT void
phonemize_eSpeak_with_positions(std::string text, eSpeakPhonemeConfig &config,
                                 std::vector<std::vector<Phoneme>> &phonemes,
                                 std::vector<std::vector<PhonemePosition>> &positions);
```

#### Modify `src/phonemize.cpp`

Add the implementation after the `phonemize_eSpeak` function (around line 134):

```cpp
// ----------------------------------------------------------------------------
// Position tracking implementation
// ----------------------------------------------------------------------------

// Thread-local storage for capturing phoneme events during synthesis
struct PhonemeEventCapture {
  std::vector<int32_t> positions;
  bool capturing = false;
};

thread_local PhonemeEventCapture g_phoneme_capture;

// Synthesis callback that captures phoneme events from espeak-ng
static int synth_callback(short *wav, int numsamples, espeak_EVENT *events) {
  if (!g_phoneme_capture.capturing) {
    return 0;
  }

  while (events && events->type != espeakEVENT_LIST_TERMINATED) {
    if (events->type == espeakEVENT_PHONEME) {
      // Capture the text position for this phoneme
      g_phoneme_capture.positions.push_back(events->text_position);
    }
    events++;
  }

  return 0;
}

PIPERPHONEMIZE_EXPORT void
phonemize_eSpeak_with_positions(std::string text, eSpeakPhonemeConfig &config,
                                 std::vector<std::vector<Phoneme>> &phonemes,
                                 std::vector<std::vector<PhonemePosition>> &positions) {

  auto voice = config.voice;
  int result = espeak_SetVoiceByName(voice.c_str());
  if (result != 0) {
    throw std::runtime_error("Failed to set eSpeak-ng voice");
  }

  std::shared_ptr<PhonemeMap> phonemeMap;
  if (config.phonemeMap) {
    phonemeMap = config.phonemeMap;
  } else if (DEFAULT_PHONEME_MAP.count(voice) > 0) {
    phonemeMap = std::make_shared<PhonemeMap>(DEFAULT_PHONEME_MAP[voice]);
  }

  // Set up synthesis callback to capture events
  espeak_SetSynthCallback(synth_callback);

  // Modified by eSpeak
  std::string textCopy(text);

  std::vector<Phoneme> *sentencePhonemes = nullptr;
  std::vector<PhonemePosition> *sentencePositions = nullptr;
  const char *inputTextPointer = textCopy.c_str();
  int terminator = 0;

  while (inputTextPointer != NULL) {
    // Clear and enable capture for this clause
    g_phoneme_capture.positions.clear();
    g_phoneme_capture.capturing = true;

    // Synthesize to trigger callbacks (output is ignored)
    espeak_Synth(inputTextPointer, strlen(inputTextPointer),
                 0, POS_CHARACTER, 0, espeakCHARS_AUTO | espeakENDPAUSE, NULL, NULL);
    espeak_Synchronize();

    g_phoneme_capture.capturing = false;

    // Get IPA phonemes using the standard API
    std::string clausePhonemes(espeak_TextToPhonemesWithTerminator(
        (const void **)&inputTextPointer,
        /*textmode*/ espeakCHARS_AUTO,
        /*phonememode = IPA*/ 0x02, &terminator));

    // Decompose, e.g. "ç" -> "c" + "̧"
    auto phonemesNorm = una::norm::to_nfd_utf8(clausePhonemes);
    auto phonemesRange = una::ranges::utf8_view{phonemesNorm};

    if (!sentencePhonemes) {
      // Start new sentence
      phonemes.emplace_back();
      positions.emplace_back();
      sentencePhonemes = &phonemes[phonemes.size() - 1];
      sentencePositions = &positions[positions.size() - 1];
    }

    // Maybe use phoneme map
    std::vector<Phoneme> mappedSentPhonemes;
    if (phonemeMap) {
      for (auto phoneme : phonemesRange) {
        if (phonemeMap->count(phoneme) < 1) {
          mappedSentPhonemes.push_back(phoneme);
        } else {
          auto mappedPhonemes = &(phonemeMap->at(phoneme));
          mappedSentPhonemes.insert(mappedSentPhonemes.end(),
                                    mappedPhonemes->begin(),
                                    mappedPhonemes->end());
        }
      }
    } else {
      mappedSentPhonemes.insert(mappedSentPhonemes.end(), phonemesRange.begin(),
                                phonemesRange.end());
    }

    auto phonemeIter = mappedSentPhonemes.begin();
    auto phonemeEnd = mappedSentPhonemes.end();
    size_t posIdx = 0;

    if (config.keepLanguageFlags) {
      // No phoneme filter
      while (phonemeIter != phonemeEnd) {
        sentencePhonemes->push_back(*phonemeIter);

        // Create position info
        PhonemePosition pos;
        if (posIdx < g_phoneme_capture.positions.size()) {
          pos.text_position = g_phoneme_capture.positions[posIdx];
          // Calculate length from next position or use 1 as default
          if (posIdx + 1 < g_phoneme_capture.positions.size()) {
            pos.length = g_phoneme_capture.positions[posIdx + 1] - pos.text_position;
          } else {
            pos.length = 1;
          }
        } else {
          // No position data available (synthetic phonemes)
          pos.text_position = -1;
          pos.length = 0;
        }
        sentencePositions->push_back(pos);

        phonemeIter++;
        posIdx++;
      }
    } else {
      // Filter out (lang) switch (flags).
      bool inLanguageFlag = false;

      while (phonemeIter != phonemeEnd) {
        if (inLanguageFlag) {
          if (*phonemeIter == U')') {
            inLanguageFlag = false;
          }
        } else if (*phonemeIter == U'(') {
          inLanguageFlag = true;
        } else {
          sentencePhonemes->push_back(*phonemeIter);

          // Create position info
          PhonemePosition pos;
          if (posIdx < g_phoneme_capture.positions.size()) {
            pos.text_position = g_phoneme_capture.positions[posIdx];
            if (posIdx + 1 < g_phoneme_capture.positions.size()) {
              pos.length = g_phoneme_capture.positions[posIdx + 1] - pos.text_position;
            } else {
              pos.length = 1;
            }
          } else {
            pos.text_position = -1;
            pos.length = 0;
          }
          sentencePositions->push_back(pos);
        }

        phonemeIter++;
        posIdx++;
      }
    }

    // Sentence boundary
    if ((terminator & CLAUSE_PERIOD) || (terminator & CLAUSE_QUESTION) ||
        (terminator & CLAUSE_EXCLAMATION)) {
      sentencePhonemes = nullptr;
      sentencePositions = nullptr;
    }
  }
}
```

#### Modify `CMakeLists.txt`

Fix iOS bundle installation (around line 217):

```cmake
install(
    TARGETS piper_phonemize_exe
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR})
```

### Step 1.5: Commit Changes

```bash
git add src/phonemize.hpp src/phonemize.cpp CMakeLists.txt
git commit -m "feat: add phoneme position tracking via espeak-ng callbacks

- Add PhonemePosition struct with text_position and length fields
- Implement phonemize_eSpeak_with_positions() function
- Use espeak_SetSynthCallback() to capture espeakEVENT_PHONEME events
- Extract text_position from espeak-ng callbacks
- Calculate phoneme lengths from consecutive position values
- Mark synthetic punctuation with position -1

This enables precise tracking of which source text characters
generated each phoneme for accurate word-level TTS alignment."
```

### Step 1.6: Push to GitHub

```bash
git push origin feature/espeak-position-tracking
```

**Result**: You now have a fork at `https://github.com/YOUR_USERNAME/piper-phonemize` with phoneme position tracking.

---

## Part 2: Modify sherpa-onnx

### Step 2.1: Fork sherpa-onnx (if not already done)

**Original Repository**: https://github.com/k2-fsa/sherpa-onnx

1. Go to https://github.com/k2-fsa/sherpa-onnx
2. Click "Fork"
3. Fork to your account

### Step 2.2: Clone Your Fork

```bash
cd ~/projects
git clone https://github.com/YOUR_USERNAME/sherpa-onnx.git
cd sherpa-onnx
git checkout -b feature/phoneme-position-tracking
```

### Step 2.3: Create PhonemeInfo Header

Create new file `sherpa-onnx/csrc/phoneme-info.h`:

```cpp
// sherpa-onnx/csrc/phoneme-info.h
#ifndef SHERPA_ONNX_CSRC_PHONEME_INFO_H_
#define SHERPA_ONNX_CSRC_PHONEME_INFO_H_

#include <string>
#include <vector>

namespace sherpa_onnx {

/// Information about a single phoneme in the input text
struct PhonemeInfo {
  /// IPA phoneme symbol (e.g., "h", "ə", "l", "oʊ")
  std::string symbol;

  /// Character offset in the original input text (0-indexed)
  int32_t char_start;

  /// Number of characters in the original text that this phoneme represents
  /// Example: "ough" in "thought" might be 1 phoneme but 4 characters
  int32_t char_length;

  PhonemeInfo() : char_start(0), char_length(0) {}

  PhonemeInfo(std::string s, int32_t start, int32_t length)
      : symbol(std::move(s)), char_start(start), char_length(length) {}
};

/// Sequence of phonemes with their text positions
using PhonemeSequence = std::vector<PhonemeInfo>;

}  // namespace sherpa_onnx

#endif  // SHERPA_ONNX_CSRC_PHONEME_INFO_H_
```

### Step 2.4: Update piper-phonemize Dependency

Modify `cmake/piper-phonemize.cmake` to use your fork:

**Before** (around line 28):
```cmake
FetchContent_Declare(piper_phonemize
  URL
    ${piper_phonemize_URL}
    ${piper_phonemize_URL2}
  URL_HASH          ${piper_phonemize_HASH}
)
```

**After**:
```cmake
FetchContent_Declare(piper_phonemize
  GIT_REPOSITORY https://github.com/YOUR_USERNAME/piper-phonemize.git
  GIT_TAG feature/espeak-position-tracking
)
```

### Step 2.5: Add Position Extraction to Lexicon

Modify `sherpa-onnx/csrc/piper-phonemize-lexicon.h`:

**Add include** (around line 12):
```cpp
#include "sherpa-onnx/csrc/phoneme-info.h"
```

**Add function declaration** after `CallPhonemizeEspeak` (around line 30):
```cpp
void CallPhonemizeEspeakWithPositions(
    const std::string &text,
    piper::eSpeakPhonemeConfig &config,
    std::vector<std::vector<piper::Phoneme>> *phonemes,
    std::vector<PhonemeSequence> *phoneme_info);
```

Modify `sherpa-onnx/csrc/piper-phonemize-lexicon.cc`:

**Add include** (around line 32):
```cpp
#include "sherpa-onnx/csrc/phoneme-info.h"
```

**Add implementation** after `CallPhonemizeEspeak` (around line 71):
```cpp
void CallPhonemizeEspeakWithPositions(
    const std::string &text,
    piper::eSpeakPhonemeConfig &config,
    std::vector<std::vector<piper::Phoneme>> *phonemes,
    std::vector<PhonemeSequence> *phoneme_info) {
  static std::mutex espeak_mutex;

  std::lock_guard<std::mutex> lock(espeak_mutex);

  // Vector to capture PhonemePosition data from piper
  std::vector<std::vector<piper::PhonemePosition>> positions;

  // Call the new piper API with position tracking
  piper::phonemize_eSpeak_with_positions(text, config, *phonemes, positions);

  // Convert piper::PhonemePosition to sherpa_onnx::PhonemeInfo
  phoneme_info->clear();
  phoneme_info->reserve(phonemes->size());

  for (size_t i = 0; i < phonemes->size(); ++i) {
    PhonemeSequence sequence;
    const auto &sentence_phonemes = (*phonemes)[i];
    const auto &sentence_positions = positions[i];

    sequence.reserve(sentence_phonemes.size());

    for (size_t j = 0; j < sentence_phonemes.size(); ++j) {
      // Convert char32_t phoneme to UTF-8 string
      std::string phoneme_str = ToString(sentence_phonemes[j]);

      // Create PhonemeInfo from PhonemePosition
      PhonemeInfo info(
          phoneme_str,
          sentence_positions[j].text_position,
          sentence_positions[j].length);

      sequence.push_back(info);
    }

    phoneme_info->push_back(std::move(sequence));
  }
}
```

### Step 2.6: Thread Phoneme Sequence Through Pipeline

This requires modifying the VITS TTS implementation to extract and pass phoneme sequences.

**Note**: The complete implementation involves modifications to:
- `sherpa-onnx/csrc/offline-tts-vits-impl.h` - Add phoneme extraction
- `sherpa-onnx/csrc/offline-tts-frontend.h` - Add PhonemeSequence to GeneratedAudio struct
- Threading the sequence through the generation pipeline

See the actual commits `68f85499` and `75029c44` for complete details.

### Step 2.7: Expose Through C API

Modify `sherpa-onnx/c-api/c-api.h`:

Add fields to `SherpaOnnxGeneratedAudio` struct (around line 1094):

```c
typedef struct SherpaOnnxGeneratedAudio {
  const float *samples;
  int32_t n;
  int32_t sample_rate;
  const int32_t *phoneme_durations;  // sample count per phoneme (w_ceil tensor)
  int32_t num_phonemes;              // number of phonemes

  // Phoneme sequence data (aligned with phoneme_durations)
  const char **phoneme_symbols;       // Array of IPA symbol strings
  const int32_t *phoneme_char_start;  // Character offset for each phoneme
  const int32_t *phoneme_char_length; // Character count for each phoneme
} SherpaOnnxGeneratedAudio;
```

Modify `sherpa-onnx/c-api/c-api.cc`:

Update allocation in `SherpaOnnxOfflineTtsGenerateInternal` (around line 1328):

```cpp
// Copy phoneme sequence data if available
if (!audio.phonemes.empty()) {
  // Allocate arrays for phoneme data
  const char **symbols = new const char*[audio.phonemes.size()];
  int32_t *char_starts = new int32_t[audio.phonemes.size()];
  int32_t *char_lengths = new int32_t[audio.phonemes.size()];

  for (size_t i = 0; i < audio.phonemes.size(); ++i) {
    // Use strdup to allocate C string
    symbols[i] = strdup(audio.phonemes[i].symbol.c_str());
    char_starts[i] = audio.phonemes[i].char_start;
    char_lengths[i] = audio.phonemes[i].char_length;
  }

  ans->phoneme_symbols = symbols;
  ans->phoneme_char_start = char_starts;
  ans->phoneme_char_length = char_lengths;
} else {
  ans->phoneme_symbols = nullptr;
  ans->phoneme_char_start = nullptr;
  ans->phoneme_char_length = nullptr;
}
```

Update cleanup in `SherpaOnnxDestroyOfflineTtsGeneratedAudio` (around line 1421):

```cpp
void SherpaOnnxDestroyOfflineTtsGeneratedAudio(
    const SherpaOnnxGeneratedAudio *p) {
  if (p) {
    delete[] p->samples;
    delete[] p->phoneme_durations;

    // Free phoneme sequence data
    if (p->phoneme_symbols) {
      for (int32_t i = 0; i < p->num_phonemes; ++i) {
        free((void*)p->phoneme_symbols[i]);  // Free strdup'd strings
      }
      delete[] p->phoneme_symbols;
    }
    delete[] p->phoneme_char_start;
    delete[] p->phoneme_char_length;

    delete p;
  }
}
```

### Step 2.8: Commit sherpa-onnx Changes

```bash
git add sherpa-onnx/csrc/phoneme-info.h
git add cmake/piper-phonemize.cmake
git add sherpa-onnx/csrc/piper-phonemize-lexicon.h
git add sherpa-onnx/csrc/piper-phonemize-lexicon.cc
git add sherpa-onnx/c-api/c-api.h
git add sherpa-onnx/c-api/c-api.cc

git commit -m "feat: integrate forked piper-phonemize with position tracking

- Update cmake to use forked piper-phonemize with position tracking
- Add CallPhonemizeEspeakWithPositions() function
- Convert piper::PhonemePosition to sherpa_onnx::PhonemeInfo
- Expose phoneme sequence through C API
- Add proper memory management for C strings

This enables precise word-level timing alignment in TTS applications."
```

### Step 2.9: Push Changes

```bash
git push origin feature/phoneme-position-tracking
```

---

## Part 3: Build for macOS (Testing)

### Step 3.1: Create Build Directory

```bash
cd ~/projects/sherpa-onnx
mkdir -p build-macos
cd build-macos
```

### Step 3.2: Configure with CMake

```bash
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DSHERPA_ONNX_ENABLE_PYTHON=OFF \
  -DSHERPA_ONNX_ENABLE_TESTS=OFF \
  -DSHERPA_ONNX_ENABLE_CHECK=OFF \
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF \
  -DSHERPA_ONNX_ENABLE_JNI=OFF \
  -DSHERPA_ONNX_ENABLE_C_API=ON \
  -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF \
  -DSHERPA_ONNX_ENABLE_BINARY=ON \
  ..
```

### Step 3.3: Build

```bash
cmake --build . -j 8
```

**Expected output**:
- Build completes without errors
- Libraries in `build-macos/lib/`
- Binaries in `build-macos/bin/`

### Step 3.4: Test Basic TTS

```bash
# Download a test model
cd ~/projects/sherpa-onnx
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/vits-piper-en_US-amy-low.tar.bz2
tar xf vits-piper-en_US-amy-low.tar.bz2

# Test synthesis
./build-macos/bin/sherpa-onnx-offline-tts \
  --vits-model=vits-piper-en_US-amy-low/en_US-amy-low.onnx \
  --vits-tokens=vits-piper-en_US-amy-low/tokens.txt \
  --vits-data-dir=vits-piper-en_US-amy-low/espeak-ng-data \
  --output-filename=test.wav \
  "Hello world, this is a test."
```

**Verify**: `test.wav` should be created and playable.

---

## Part 4: Build for iOS

### Step 4.1: Clean Previous Builds

```bash
cd ~/projects/sherpa-onnx
rm -rf build-ios
```

### Step 4.2: Run iOS Build Script

```bash
./build-ios.sh
```

**What this does**:
1. Downloads ONNX Runtime iOS xcframework (v1.17.1)
2. Builds for iOS Simulator x86_64
3. Builds for iOS Simulator arm64 (Apple Silicon Macs)
4. Builds for iOS arm64 (actual devices)
5. Creates fat library for simulator (combines x86_64 + arm64)
6. Creates sherpa-onnx.xcframework (contains all architectures)

**Duration**: 15-30 minutes depending on machine

**Expected output**:
```
Building for simulator (x86_64)
...
Building for simulator (arm64)
...
Building for arm64
...
Generate xcframework
...
```

### Step 4.3: Verify Build Output

```bash
ls -lh build-ios/sherpa-onnx.xcframework
```

**Should see**:
```
sherpa-onnx.xcframework/
├── Info.plist
├── ios-arm64/
│   ├── libsherpa-onnx.a
│   └── Headers/
└── ios-arm64_x86_64-simulator/
    ├── libsherpa-onnx.a
    └── Headers/
```

### Step 4.4: Check Framework Size

```bash
du -sh build-ios/sherpa-onnx.xcframework
```

**Expected**: ~100-150 MB (includes espeak-ng, piper-phonemize, etc.)

---

## Part 5: Integration with Listen2

### Step 5.1: Replace Framework in Xcode Project

```bash
cd ~/projects/Listen2
rm -rf Listen2/Frameworks/sherpa-onnx.xcframework
cp -r ~/projects/sherpa-onnx/build-ios/sherpa-onnx.xcframework \
      Listen2/Frameworks/
```

### Step 5.2: Verify Framework Integration

Open Listen2 project in Xcode:

```bash
cd ~/projects/Listen2/Listen2
open Listen2.xcodeproj
```

**Check**:
1. Framework is visible in "Frameworks" folder
2. Build Settings → Search "sherpa-onnx" → Framework Search Paths should include `$(PROJECT_DIR)/Frameworks`
3. Clean build folder (⇧⌘K)
4. Build project (⌘B)

**Expected**: Project builds successfully

### Step 5.3: Update Swift Bindings

The C API now provides phoneme sequence data. Update your Swift code to access it:

```swift
// Example: Accessing phoneme data in PiperTTSProvider.swift
func synthesize(text: String) async throws -> (audio: Data, phonemes: [PhonemeInfo]) {
    // ... existing synthesis code ...

    let generatedAudio = SherpaOnnxOfflineTtsGenerate(tts, text, speed: 1.0)
    defer { SherpaOnnxDestroyOfflineTtsGeneratedAudio(generatedAudio) }

    // Extract audio
    let audioData = Data(bytes: generatedAudio.samples,
                         count: Int(generatedAudio.n) * MemoryLayout<Float>.size)

    // Extract phoneme sequence
    var phonemes: [PhonemeInfo] = []
    if generatedAudio.num_phonemes > 0 {
        for i in 0..<Int(generatedAudio.num_phonemes) {
            let symbol = String(cString: generatedAudio.phoneme_symbols[i])
            let charStart = generatedAudio.phoneme_char_start[i]
            let charLength = generatedAudio.phoneme_char_length[i]

            phonemes.append(PhonemeInfo(
                symbol: symbol,
                charStart: Int(charStart),
                charLength: Int(charLength)
            ))
        }
    }

    return (audioData, phonemes)
}
```

### Step 5.4: Test on Device/Simulator

1. Build and run on iOS Simulator
2. Generate TTS audio
3. Verify phoneme data is returned
4. Check that char_start values map to correct text positions

---

## Part 6: Verification & Testing

### Test Case 1: Basic Phoneme Extraction

**Input Text**: `"Hello world"`

**Expected Output**:
```
Phonemes:
  [0] "h" at position 0, length 1
  [1] "ə" at position 1, length 1
  [2] "l" at position 2, length 1
  [3] "oʊ" at position 3, length 2
  [4] " " at position 5, length 1 (space)
  [5] "w" at position 6, length 1
  [6] "ɝ" at position 7, length 1
  [7] "l" at position 8, length 1
  [8] "d" at position 9, length 1
```

### Test Case 2: Multi-Character Phonemes

**Input Text**: `"thought"`

**Expected**: Some phonemes map to multiple characters (e.g., "ough" → one phoneme)

### Test Case 3: Punctuation

**Input Text**: `"Hello, world!"`

**Expected**: Punctuation may generate synthetic phonemes with position -1

### Test Case 4: Apostrophes

**Input Text**: `"it's the author's book"`

**Expected**: Apostrophes are tracked correctly without crashes

---

## Troubleshooting

### Build Error: "espeak_SetSynthCallback undeclared"

**Solution**: The espeak-ng dependency is too old. sherpa-onnx should fetch the correct version automatically via CMake. Check that `cmake/piper-phonemize.cmake` is using the FetchContent approach.

### Build Error: "piper::PhonemePosition not found"

**Solution**: Your piper-phonemize fork wasn't properly integrated. Verify:
```bash
cd ~/projects/sherpa-onnx/build-ios/_deps/piper_phonemize-src
git log --oneline | head -5
```

Should show your phoneme position tracking commit.

### iOS Framework Not Found in Xcode

**Solution**:
1. Check framework path: `Listen2/Frameworks/sherpa-onnx.xcframework`
2. Xcode → Target → Build Settings → Framework Search Paths
3. Add: `$(PROJECT_DIR)/Frameworks`
4. Clean build folder and rebuild

### Crash on TTS Generation

**Solution**: Check that:
1. `espeak-ng-data` directory is included in app bundle
2. Model files (.onnx, tokens.txt) are accessible
3. Memory management: Are you calling `SherpaOnnxDestroyOfflineTtsGeneratedAudio`?

### Phoneme Count Mismatch

If phoneme count doesn't match expectations:
1. Check that espeak-ng is initialized properly
2. Verify the voice matches the model
3. Some punctuation generates synthetic phonemes

---

## Build Output Summary

After successful build, you should have:

### macOS Build
- **Location**: `~/projects/sherpa-onnx/build-macos/`
- **Artifacts**:
  - `lib/libsherpa-onnx-c-api.dylib`
  - `lib/libpiper_phonemize.a`
  - `bin/sherpa-onnx-offline-tts` (test binary)

### iOS Build
- **Location**: `~/projects/sherpa-onnx/build-ios/`
- **Artifacts**:
  - `sherpa-onnx.xcframework/` (multi-architecture)
  - `install/include/sherpa-onnx/c-api/c-api.h`
  - `install/lib/` (static libraries for each architecture)

### Git Repositories

**piper-phonemize fork**:
- URL: `https://github.com/YOUR_USERNAME/piper-phonemize`
- Branch: `feature/espeak-position-tracking`
- Key commit: "feat: add phoneme position tracking via espeak-ng callbacks"

**sherpa-onnx fork**:
- URL: `https://github.com/YOUR_USERNAME/sherpa-onnx`
- Branch: `feature/phoneme-position-tracking`
- Key commits:
  - "feat: integrate forked piper-phonemize with position tracking"
  - "feat: expose phoneme sequence through C API"

---

## Maintaining the Fork

### Syncing with Upstream

Periodically sync with upstream repositories:

```bash
# piper-phonemize
cd ~/projects/piper-phonemize
git remote add upstream https://github.com/rhasspy/piper-phonemize.git
git fetch upstream
git merge upstream/main
# Resolve conflicts if needed
git push origin feature/espeak-position-tracking

# sherpa-onnx
cd ~/projects/sherpa-onnx
git remote add upstream https://github.com/k2-fsa/sherpa-onnx.git
git fetch upstream
git merge upstream/master
# Resolve conflicts if needed
git push origin feature/phoneme-position-tracking
```

### Rebuilding After Updates

```bash
cd ~/projects/sherpa-onnx
rm -rf build-ios
./build-ios.sh
cp -r build-ios/sherpa-onnx.xcframework ~/projects/Listen2/Listen2/Frameworks/
```

---

## Performance Notes

### Build Times (Apple M1 Max)
- Initial iOS build: ~25 minutes
- Incremental rebuild: ~5 minutes
- macOS build: ~10 minutes

### Framework Size
- sherpa-onnx.xcframework: ~120 MB
- Includes: espeak-ng data, piper-phonemize, ONNX Runtime linkage

### Runtime Performance
- Phoneme extraction: < 1ms overhead per synthesis
- No noticeable impact on TTS generation time

---

## Next Steps

After successful build:

1. **Implement PhonemeAlignmentService** in Listen2
   - Map phonemes to VoxPDF words using char_start/char_length
   - Replace ASR-based alignment

2. **Testing**
   - Unit tests for phoneme-to-word mapping
   - Integration tests with real TTS audio
   - Edge case testing (apostrophes, punctuation, etc.)

3. **Cleanup**
   - Remove ASR models and code
   - Update documentation
   - Performance benchmarking

---

## References

- **piper-phonemize**: https://github.com/rhasspy/piper-phonemize
- **sherpa-onnx**: https://github.com/k2-fsa/sherpa-onnx
- **espeak-ng**: https://github.com/espeak-ng/espeak-ng
- **iOS Build Guide**: https://k2-fsa.github.io/sherpa/onnx/install/ios.html
- **ONNX Runtime**: https://onnxruntime.ai/

---

## Credits

**Author**: Zach Swift
**Date**: 2025-11-12
**AI Assistance**: Claude (Anthropic)
**License**: Same as sherpa-onnx and piper-phonemize (Apache 2.0)
