// test_phoneme_positions.cpp
// Quick test to demonstrate phoneme position tracking

#include <iostream>
#include <vector>
#include <string>

// Include piper-phonemize headers
#include "phonemize.hpp"
#include "sherpa-onnx/csrc/phoneme-info.h"

// Simplified version of CallPhonemizeEspeakWithPositions for testing
void TestPhonemizeWithPositions(const std::string &text) {
    piper::eSpeakPhonemeConfig config;
    config.voice = "en-us";

    std::vector<std::vector<piper::Phoneme>> phonemes;
    std::vector<std::vector<piper::PhonemePosition>> positions;

    // Call the new API with position tracking
    piper::phonemize_eSpeak_with_positions(text, config, phonemes, positions);

    std::cout << "Text: \"" << text << "\"\n\n";

    for (size_t i = 0; i < phonemes.size(); ++i) {
        std::cout << "Sentence " << i << ":\n";
        const auto &sentence_phonemes = phonemes[i];
        const auto &sentence_positions = positions[i];

        for (size_t j = 0; j < sentence_phonemes.size(); ++j) {
            // Convert char32_t to string for display
            char32_t cp = sentence_phonemes[j];
            std::string phoneme_str;
            if (cp <= 0x7F) {
                phoneme_str += static_cast<char>(cp);
            } else if (cp <= 0x7FF) {
                phoneme_str += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
                phoneme_str += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp <= 0xFFFF) {
                phoneme_str += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
                phoneme_str += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                phoneme_str += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp <= 0x10FFFF) {
                phoneme_str += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
                phoneme_str += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                phoneme_str += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                phoneme_str += static_cast<char>(0x80 | (cp & 0x3F));
            }

            std::cout << "  Phoneme " << j << ": '" << phoneme_str << "' "
                     << "(pos: " << sentence_positions[j].text_position
                     << ", len: " << sentence_positions[j].length << ")\n";
        }
        std::cout << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Testing with default text: \"Hello world\"\n\n";
        TestPhonemizeWithPositions("Hello world");
    } else {
        TestPhonemizeWithPositions(argv[1]);
    }

    return 0;
}
