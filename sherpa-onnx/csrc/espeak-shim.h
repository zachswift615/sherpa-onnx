// sherpa-onnx/csrc/espeak-shim.h
//
// Listen2 fork: espeak-ng (GPL-3.0) and piper-phonemize are optional.
//
// With -DSHERPA_ONNX_ENABLE_ESPEAK=OFF neither library is compiled or linked.
// Piper VITS models then only accept ready-made phonemes (see
// kPhonemeInputMarker in piper-phonemize-lexicon.cc); every other code path
// that would have called espeak logs an error and produces no phonemes.

#ifndef SHERPA_ONNX_CSRC_ESPEAK_SHIM_H_
#define SHERPA_ONNX_CSRC_ESPEAK_SHIM_H_

#ifndef SHERPA_ONNX_ENABLE_ESPEAK
#define SHERPA_ONNX_ENABLE_ESPEAK 1
#endif

#if SHERPA_ONNX_ENABLE_ESPEAK

#include "espeak-ng/speak_lib.h"
#include "phoneme_ids.hpp"  // NOLINT
#include "phonemize.hpp"    // NOLINT

#else

#include <string>

namespace piper {

typedef char32_t Phoneme;

struct eSpeakPhonemeConfig {
  std::string voice = "en-us";
};

}  // namespace piper

#endif  // SHERPA_ONNX_ENABLE_ESPEAK

#endif  // SHERPA_ONNX_CSRC_ESPEAK_SHIM_H_
