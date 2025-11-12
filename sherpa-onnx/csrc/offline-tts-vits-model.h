// sherpa-onnx/csrc/offline-tts-vits-model.h
//
// Copyright (c)  2023  Xiaomi Corporation

#ifndef SHERPA_ONNX_CSRC_OFFLINE_TTS_VITS_MODEL_H_
#define SHERPA_ONNX_CSRC_OFFLINE_TTS_VITS_MODEL_H_

#include <memory>
#include <string>

#include "onnxruntime_cxx_api.h"  // NOLINT
#include "sherpa-onnx/csrc/offline-tts-model-config.h"
#include "sherpa-onnx/csrc/offline-tts-vits-model-meta-data.h"

namespace sherpa_onnx {

// Output structure containing both audio samples and phoneme durations
struct VitsOutput {
  Ort::Value audio;
  Ort::Value phoneme_durations;  // w_ceil tensor (phoneme sample counts)

  // Default constructor
  VitsOutput() : audio(nullptr), phoneme_durations(nullptr) {}

  // Constructor for when phoneme durations are available
  VitsOutput(Ort::Value a, Ort::Value d)
    : audio(std::move(a)), phoneme_durations(std::move(d)) {}

  // Constructor for when only audio is available (fallback)
  explicit VitsOutput(Ort::Value a)
    : audio(std::move(a)), phoneme_durations(nullptr) {}
};

class OfflineTtsVitsModel {
 public:
  ~OfflineTtsVitsModel();

  explicit OfflineTtsVitsModel(const OfflineTtsModelConfig &config);

  template <typename Manager>
  OfflineTtsVitsModel(Manager *mgr, const OfflineTtsModelConfig &config);

  /** Run the model.
   *
   * @param x A int64 tensor of shape (1, num_tokens)
  // @param sid Speaker ID. Used only for multi-speaker models, e.g., models
  //            trained using the VCTK dataset. It is not used for
  //            single-speaker models, e.g., models trained using the ljspeech
  //            dataset.
   * @return Return VitsOutput containing audio samples and phoneme durations
   */
  VitsOutput Run(Ort::Value x, int64_t sid = 0, float speed = 1.0);

  // This is for MeloTTS
  VitsOutput Run(Ort::Value x, Ort::Value tones, int64_t sid = 0,
                 float speed = 1.0) const;

  const OfflineTtsVitsModelMetaData &GetMetaData() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sherpa_onnx

#endif  // SHERPA_ONNX_CSRC_OFFLINE_TTS_VITS_MODEL_H_
