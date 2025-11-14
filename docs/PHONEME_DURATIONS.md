# Phoneme Duration Extraction in sherpa-onnx

## Overview

The sherpa-onnx C++ layer **already has phoneme duration support**! This document describes how phoneme durations are extracted from the VITS model's w_ceil tensor and stored in the `GeneratedAudio` struct.

## Architecture

### 1. Field Location

**File:** `sherpa-onnx/csrc/offline-tts.h` (line 60)

```cpp
struct GeneratedAudio {
  std::vector<float> samples;
  int32_t sample_rate;
  std::vector<int32_t> phoneme_durations;  // w_ceil tensor: sample count per phoneme
  PhonemeSequence phonemes;

  GeneratedAudio ScaleSilence(float scale) const;
};
```

### 2. Extraction Location

**File:** `sherpa-onnx/csrc/offline-tts-vits-impl.h` (lines 492-508)

The extraction happens in the `Generate()` method after VITS model inference:

```cpp
// Extract phoneme durations (w_ceil tensor) if available
if (vits_output.phoneme_durations) {
  try {
    auto durations_shape = vits_output.phoneme_durations.GetTensorTypeAndShapeInfo().GetShape();
    int64_t num_phonemes = durations_shape[0];
    const int64_t* duration_data = vits_output.phoneme_durations.GetTensorData<int64_t>();

    ans.phoneme_durations.reserve(num_phonemes);
    for (int64_t i = 0; i < num_phonemes; i++) {
      // Multiply by 256 to get actual sample counts (per Piper VITS implementation)
      ans.phoneme_durations.push_back(static_cast<int32_t>(duration_data[i] * 256));
    }
  } catch (...) {
    // If extracting durations fails, leave the vector empty
    // This ensures backward compatibility with models that don't output durations
    ans.phoneme_durations.clear();
  }
}
```

### 3. Data Flow

```
VITS Model
   ↓
w_ceil tensor (int64 frame counts)
   ↓
offline-tts-vits-impl.h: Extract tensor data
   ↓
Multiply each frame count by 256 → sample counts
   ↓
GeneratedAudio::phoneme_durations (std::vector<int32_t>)
   ↓
C API (pending - Task 4)
   ↓
Swift bridge (pending - Task 6)
```

## Data Format

### Units

- **Type:** `std::vector<int32_t>`
- **Units:** Sample counts (NOT seconds, NOT frames)
- **Conversion:** `duration_seconds = samples / sample_rate`

### Example

```cpp
// Example: 256 samples at 22050 Hz
int32_t samples = 256;
float duration_seconds = static_cast<float>(samples) / 22050.0f;
// Result: 0.0116 seconds (11.6ms)
```

### Realistic Values

For the word "Hello" synthesized with Piper:

| Phoneme | Duration (ms) | Sample Count @ 22050 Hz |
|---------|---------------|-------------------------|
| h       | ~40ms         | 882 samples             |
| ə       | ~30ms         | 662 samples             |
| l       | ~50ms         | 1103 samples            |
| oʊ      | ~80ms         | 1764 samples            |

Total: ~200ms / 4411 samples

## w_ceil Tensor Details

### What is w_ceil?

The `w_ceil` tensor is output by the Piper VITS model and contains the duration (in frames) that each phoneme should be synthesized for. It's called "w_ceil" because it represents the ceiling of the alignment widths from the attention mechanism.

### Tensor Properties

- **Location:** Second output tensor from VITS model
- **Shape:** `[num_phonemes]`
- **Data Type:** `int64`
- **Values:** Frame counts (before multiplication)

### Frame to Sample Conversion

The Piper VITS model uses a **frame shift of 256 samples**. This means:

- 1 frame = 256 audio samples
- At 22050 Hz sample rate: 1 frame ≈ 11.6ms

Therefore, the extraction code multiplies each frame count by 256 to get the actual sample count:

```cpp
phoneme_durations.push_back(static_cast<int32_t>(duration_data[i] * 256));
```

## Backward Compatibility

The implementation is **fully backward compatible**:

- If the VITS model doesn't output durations, the vector remains empty
- The extraction is wrapped in a try-catch block to handle missing tensors
- Code that doesn't use durations continues to work unchanged

## Testing

### Unit Test

Run the test to verify the implementation:

```bash
cd /Users/zachswift/projects/sherpa-onnx
./test_duration_extraction
```

Expected output:
```
✅ All tests passed!
✅ C++ duration field exists and is being populated
✅ Ready to proceed to Task 4 (C API exposure)
```

### What the Test Verifies

1. ✅ GeneratedAudio struct has phoneme_durations field
2. ✅ Sample count to time conversion works correctly
3. ✅ Empty durations (backward compatibility)
4. ✅ Realistic duration values
5. ✅ w_ceil extraction logic (multiply by 256)

## Next Steps

- **Task 3:** ~~Extract w_ceil tensor~~ **SKIPPED** (already implemented!)
- **Task 4:** Expose durations through C API
- **Task 5:** Build iOS framework with duration support
- **Task 6:** Update Swift bridge to read durations

## Implementation Status

| Component | Status | Location |
|-----------|--------|----------|
| C++ struct field | ✅ Complete | `offline-tts.h:60` |
| w_ceil extraction | ✅ Complete | `offline-tts-vits-impl.h:492-508` |
| C API exposure | ⏳ Pending | Task 4 |
| iOS framework | ⏳ Pending | Task 5 |
| Swift bridge | ⏳ Pending | Task 6 |

## References

- Original plan: `/Users/zachswift/projects/Listen2/docs/plans/2025-11-14-premium-word-highlighting.md`
- Test file: `/Users/zachswift/projects/sherpa-onnx/test_duration_extraction.cpp`
- Piper VITS implementation: Based on https://github.com/rhasspy/piper
