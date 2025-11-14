// sherpa-onnx/csrc/piper-phonemize-lexicon.h
//
// Copyright (c)  2022-2023  Xiaomi Corporation

#ifndef SHERPA_ONNX_CSRC_PIPER_PHONEMIZE_LEXICON_H_
#define SHERPA_ONNX_CSRC_PIPER_PHONEMIZE_LEXICON_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "sherpa-onnx/csrc/offline-tts-frontend.h"
#include "sherpa-onnx/csrc/phoneme-info.h"
#include "sherpa-onnx/csrc/offline-tts-kitten-model-meta-data.h"
#include "sherpa-onnx/csrc/offline-tts-kokoro-model-meta-data.h"
#include "sherpa-onnx/csrc/offline-tts-matcha-model-meta-data.h"
#include "sherpa-onnx/csrc/offline-tts-vits-model-meta-data.h"

namespace sherpa_onnx {

class PiperPhonemizeLexicon : public OfflineTtsFrontend {
 public:
  PiperPhonemizeLexicon(const std::string &tokens, const std::string &data_dir,
                        const OfflineTtsVitsModelMetaData &vits_meta_data);

  PiperPhonemizeLexicon(const std::string &tokens, const std::string &data_dir,
                        const OfflineTtsMatchaModelMetaData &matcha_meta_data);

  PiperPhonemizeLexicon(const std::string &tokens, const std::string &data_dir,
                        const OfflineTtsKokoroModelMetaData &kokoro_meta_data);

  PiperPhonemizeLexicon(const std::string &tokens, const std::string &data_dir,
                        const OfflineTtsKittenModelMetaData &kitten_meta_data);

  template <typename Manager>
  PiperPhonemizeLexicon(Manager *mgr, const std::string &tokens,
                        const std::string &data_dir,
                        const OfflineTtsVitsModelMetaData &vits_meta_data);

  template <typename Manager>
  PiperPhonemizeLexicon(Manager *mgr, const std::string &tokens,
                        const std::string &data_dir,
                        const OfflineTtsMatchaModelMetaData &matcha_meta_data);

  template <typename Manager>
  PiperPhonemizeLexicon(Manager *mgr, const std::string &tokens,
                        const std::string &data_dir,
                        const OfflineTtsKokoroModelMetaData &kokoro_meta_data);

  template <typename Manager>
  PiperPhonemizeLexicon(Manager *mgr, const std::string &tokens,
                        const std::string &data_dir,
                        const OfflineTtsKittenModelMetaData &kitten_meta_data);

  std::vector<TokenIDs> ConvertTextToTokenIds(
      const std::string &text, const std::string &voice = "") const override;

  // Get the last phoneme sequences captured during ConvertTextToTokenIds
  const std::vector<PhonemeSequence>& GetLastPhonemeSequences() const {
    return last_phoneme_sequences_;
  }

  // Get the normalized text from the last phonemize call
  const std::string& GetLastNormalizedText() const {
    return last_normalized_text_;
  }

  // Get the character mapping from the last phonemize call
  const std::vector<std::pair<int32_t, int32_t>>& GetLastCharMapping() const {
    return last_char_mapping_;
  }

 private:
  std::vector<TokenIDs> ConvertTextToTokenIdsVits(
      const std::string &text, const std::string &voice = "") const;

  std::vector<TokenIDs> ConvertTextToTokenIdsMatcha(
      const std::string &text, const std::string &voice = "") const;

 private:
  // map unicode codepoint to an integer ID
  std::unordered_map<char32_t, int32_t> token2id_;
  OfflineTtsVitsModelMetaData vits_meta_data_;
  OfflineTtsMatchaModelMetaData matcha_meta_data_;
  OfflineTtsKokoroModelMetaData kokoro_meta_data_;
  OfflineTtsKittenModelMetaData kitten_meta_data_;
  bool is_matcha_ = false;
  bool is_kokoro_ = false;
  bool is_kitten_ = false;

  // Store the last phoneme sequences captured during ConvertTextToTokenIds
  mutable std::vector<PhonemeSequence> last_phoneme_sequences_;

  // Store the normalized text and character mapping from the last phonemize call
  mutable std::string last_normalized_text_;
  mutable std::vector<std::pair<int32_t, int32_t>> last_char_mapping_;
};

}  // namespace sherpa_onnx

#endif  // SHERPA_ONNX_CSRC_PIPER_PHONEMIZE_LEXICON_H_
