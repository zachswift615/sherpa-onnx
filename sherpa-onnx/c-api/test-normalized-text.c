// sherpa-onnx/c-api/test-normalized-text.c
//
// Test for normalized text exposure through C API

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c-api.h"

int main() {
  printf("Testing normalized text exposure in sherpa-onnx C API\n");
  printf("=====================================================\n\n");

  // NOTE: This test requires a TTS model to be available
  // For a real test, you would need to configure with actual model paths
  // This is a minimal example showing how to access the normalized text

  const char* test_text = "Dr. Smith's office is at 42 Main St.";
  printf("Input text: \"%s\"\n\n", test_text);

  // Example of what the code would look like with a real TTS instance:
  /*
  SherpaOnnxOfflineTtsConfig config;
  memset(&config, 0, sizeof(config));

  // Configure TTS model...
  config.model.vits.model = "/path/to/model.onnx";
  config.model.vits.tokens = "/path/to/tokens.txt";
  config.model.vits.data_dir = "/path/to/espeak-ng-data";

  SherpaOnnxOfflineTts* tts = SherpaOnnxCreateOfflineTts(&config);
  if (!tts) {
    fprintf(stderr, "Failed to create TTS\n");
    return 1;
  }

  const SherpaOnnxGeneratedAudio* audio = SherpaOnnxOfflineTtsGenerate(
      tts, test_text, 0, 1.0);

  if (!audio) {
    fprintf(stderr, "Failed to generate audio\n");
    SherpaOnnxDestroyOfflineTts(tts);
    return 1;
  }

  // Verify normalized text is populated
  if (audio->normalized_text) {
    printf("✓ Normalized text: \"%s\"\n", audio->normalized_text);

    // Should expand "Dr." to "Doctor" and "42" to "forty two", etc.
    if (strstr(audio->normalized_text, "Doctor") != NULL) {
      printf("✓ Found 'Doctor' (expansion of 'Dr.')\n");
    }
  } else {
    printf("✗ normalized_text is NULL\n");
  }

  // Verify character mapping exists
  if (audio->char_mapping && audio->char_mapping_count > 0) {
    printf("✓ Character mapping count: %d\n", audio->char_mapping_count);

    // Print first few mappings
    printf("  First 3 mappings:\n");
    for (int i = 0; i < 3 && i < audio->char_mapping_count; i++) {
      int orig_pos = audio->char_mapping[i * 2];
      int norm_pos = audio->char_mapping[i * 2 + 1];
      printf("    [%d]: original pos %d -> normalized pos %d\n",
             i, orig_pos, norm_pos);
    }
  } else {
    printf("✗ char_mapping is empty\n");
  }

  printf("\n✓ sherpa-onnx C API normalized text test structure verified\n");

  SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
  SherpaOnnxDestroyOfflineTts(tts);
  */

  printf("\nNOTE: This is a structural test showing API usage.\n");
  printf("To run a real test, configure with actual TTS model paths.\n");
  printf("The new C API fields are:\n");
  printf("  - normalized_text: const char*\n");
  printf("  - char_mapping: const int32_t* (pairs of [orig_pos, norm_pos])\n");
  printf("  - char_mapping_count: int32_t\n");

  return 0;
}
