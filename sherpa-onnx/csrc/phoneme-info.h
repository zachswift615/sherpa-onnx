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
