#include <iostream>
#include <vector>
#include <string>
#include "espeak-ng/speak_lib.h"
#include "phonemize.hpp"

// Helper to convert char32_t to UTF-8 string
std::string ToUTF8(char32_t cp) {
  std::string result;
  if (cp <= 0x7F) {
    result += static_cast<char>(cp);
  } else if (cp <= 0x7FF) {
    result += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
    result += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp <= 0xFFFF) {
    result += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
    result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    result += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp <= 0x10FFFF) {
    result += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
    result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    result += static_cast<char>(0x80 | (cp & 0x3F));
  }
  return result;
}

int main(int argc, char* argv[]) {
  std::string espeak_data_dir = argc > 1 ? argv[1] : "./install/share/espeak-ng-data";

  std::cout << "Initializing espeak-ng with data dir: " << espeak_data_dir << std::endl;

  // Initialize espeak with phoneme events enabled
  int result = espeak_Initialize(AUDIO_OUTPUT_SYNCHRONOUS, 0, espeak_data_dir.c_str(),
                                  espeakINITIALIZE_PHONEME_EVENTS | espeakINITIALIZE_PHONEME_IPA);
  if (result < 0) {
    std::cerr << "Failed to initialize espeak-ng" << std::endl;
    return 1;
  }
  std::cout << "Espeak initialized with sample rate: " << result << std::endl;

  // Test text
  std::string test_text = "Hello world";
  std::cout << "\nTest text: \"" << test_text << "\"" << std::endl;
  std::cout << "Length: " << test_text.length() << " characters\n" << std::endl;

  // Set up config
  piper::eSpeakPhonemeConfig config;
  config.voice = "en-us";

  // Call the new function with position tracking
  std::vector<std::vector<piper::Phoneme>> phonemes;
  std::vector<std::vector<piper::PhonemePosition>> positions;

  try {
    piper::phonemize_eSpeak_with_positions(test_text, config, phonemes, positions);

    std::cout << "Phonemization complete!" << std::endl;
    std::cout << "Number of sentences: " << phonemes.size() << "\n" << std::endl;

    for (size_t sent_idx = 0; sent_idx < phonemes.size(); sent_idx++) {
      std::cout << "Sentence " << (sent_idx + 1) << ":" << std::endl;
      std::cout << "  Phonemes: " << phonemes[sent_idx].size() << std::endl;

      for (size_t i = 0; i < phonemes[sent_idx].size(); i++) {
        std::string phoneme_utf8 = ToUTF8(phonemes[sent_idx][i]);
        int32_t pos = positions[sent_idx][i].text_position;
        int32_t len = positions[sent_idx][i].length;

        std::cout << "    [" << i << "] '"  << phoneme_utf8
                  << "' at position " << pos
                  << " length " << len;

        if (pos >= 0 && pos < (int32_t)test_text.length()) {
          std::string text_segment = test_text.substr(pos, std::min(len, (int32_t)test_text.length() - pos));
          std::cout << " (text: \"" << text_segment << "\")";
        }

        std::cout << std::endl;
      }
      std::cout << std::endl;
    }

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  espeak_Terminate();
  return 0;
}
