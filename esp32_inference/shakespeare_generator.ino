/*
 * Shakespeare Text Generator for ESP32-S3R8
 * Using TensorFlow Lite Micro
 *
 * Hardware: LilyGo T-Display S3R8
 * - ESP32-S3R8 (8MB PSRAM, 16MB Flash)
 * - 1.9" TFT Display (ST7789V, 170x320)
 *
 * This sketch loads a quantized character-level LSTM model
 * trained on Shakespeare's works and generates text.
 */

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <TFT_eSPI.h>  // For T-Display
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"

// Model configuration
#define VOCAB_SIZE 100  // Will be updated from vocab.json
#define SEQ_LENGTH 40
#define TENSOR_ARENA_SIZE (2 * 1024 * 1024)  // 2MB for inference

// TensorFlow Lite globals
namespace {
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* input = nullptr;
  TfLiteTensor* output = nullptr;

  // Tensor arena in PSRAM
  uint8_t* tensor_arena = nullptr;
}

// Display
TFT_eSPI tft = TFT_eSPI();

// Vocabulary
StaticJsonDocument<8192> vocabDoc;
int actualVocabSize = 0;
int seqLength = 40;

// Character mappings
char idx_to_char[256];
int char_to_idx[256];

// Current sequence buffer
int current_sequence[100];  // Max sequence length

// Forward declarations
bool loadVocabulary();
bool loadModel();
char generateNextChar(float temperature = 0.8);
void displayText(const String& text);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=================================");
  Serial.println("Shakespeare Generator - ESP32-S3R8");
  Serial.println("=================================\n");

  // Initialize display
  tft.init();
  tft.setRotation(1);  // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.println("Shakespeare Generator");
  tft.println("Initializing...");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("ERROR: Failed to mount SPIFFS");
    tft.println("ERROR: SPIFFS failed");
    return;
  }
  Serial.println("✓ SPIFFS mounted");

  // Allocate tensor arena in PSRAM
  if (psramFound()) {
    Serial.printf("PSRAM available: %d bytes\n", ESP.getPsramSize());
    tensor_arena = (uint8_t*)ps_malloc(TENSOR_ARENA_SIZE);
    if (tensor_arena == nullptr) {
      Serial.println("ERROR: Failed to allocate tensor arena in PSRAM");
      return;
    }
    Serial.printf("✓ Tensor arena allocated: %d bytes in PSRAM\n", TENSOR_ARENA_SIZE);
  } else {
    Serial.println("ERROR: No PSRAM found!");
    return;
  }

  // Load vocabulary
  if (!loadVocabulary()) {
    Serial.println("ERROR: Failed to load vocabulary");
    tft.println("ERROR: Vocab failed");
    return;
  }

  // Load model
  if (!loadModel()) {
    Serial.println("ERROR: Failed to load model");
    tft.println("ERROR: Model failed");
    return;
  }

  // Initialize with a seed phrase
  String seed = "To be, or not to be, that is the quest";
  Serial.println("\n=================================");
  Serial.println("Seed: " + seed);
  Serial.println("=================================\n");

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.print(seed);

  // Initialize sequence buffer
  initSequence(seed);

  // Generate text
  generateText(300);

  Serial.println("\n\n✓ Generation complete!");
}

void loop() {
  // Generate continuously
  delay(2000);

  // Generate a few more characters
  for (int i = 0; i < 20; i++) {
    char nextChar = generateNextChar(0.8);
    Serial.print(nextChar);
    tft.print(nextChar);
    delay(50);
  }
}

bool loadVocabulary() {
  Serial.println("Loading vocabulary...");

  File file = SPIFFS.open("/vocab.json", "r");
  if (!file) {
    Serial.println("ERROR: Failed to open vocab.json");
    return false;
  }

  DeserializationError error = deserializeJson(vocabDoc, file);
  file.close();

  if (error) {
    Serial.printf("ERROR: Failed to parse vocab.json: %s\n", error.c_str());
    return false;
  }

  actualVocabSize = vocabDoc["vocab_size"];
  seqLength = vocabDoc["seq_length"];

  Serial.printf("  Vocab size: %d\n", actualVocabSize);
  Serial.printf("  Sequence length: %d\n", seqLength);

  // Build character mappings
  JsonObject idx_to_char_obj = vocabDoc["idx_to_char"];
  JsonObject char_to_idx_obj = vocabDoc["char_to_idx"];

  // Initialize arrays
  for (int i = 0; i < 256; i++) {
    idx_to_char[i] = '?';
    char_to_idx[i] = 0;
  }

  // Load idx_to_char
  for (JsonPair kv : idx_to_char_obj) {
    int idx = atoi(kv.key().c_str());
    const char* ch = kv.value();
    if (idx >= 0 && idx < 256) {
      idx_to_char[idx] = ch[0];
    }
  }

  // Load char_to_idx
  for (JsonPair kv : char_to_idx_obj) {
    const char* ch = kv.key().c_str();
    int idx = kv.value();
    if (ch[0] >= 0) {
      char_to_idx[(unsigned char)ch[0]] = idx;
    }
  }

  Serial.println("✓ Vocabulary loaded");
  return true;
}

bool loadModel() {
  Serial.println("Loading TFLite model...");

  // Load model file
  File file = SPIFFS.open("/shakespeare_model.tflite", "r");
  if (!file) {
    Serial.println("ERROR: Failed to open model file");
    return false;
  }

  size_t modelSize = file.size();
  Serial.printf("  Model size: %d bytes (%.2f MB)\n", modelSize, modelSize / 1024.0 / 1024.0);

  // Allocate model buffer in PSRAM
  uint8_t* model_data = (uint8_t*)ps_malloc(modelSize);
  if (model_data == nullptr) {
    Serial.println("ERROR: Failed to allocate model buffer");
    file.close();
    return false;
  }

  // Read model
  file.read(model_data, modelSize);
  file.close();

  // Map the model
  model = tflite::GetModel(model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("ERROR: Model schema version %d not supported\n", model->version());
    return false;
  }

  // Set up ops resolver
  static tflite::MicroMutableOpResolver<10> resolver;
  resolver.AddFullyConnected();
  resolver.AddSoftmax();
  resolver.AddLSTM();
  resolver.AddReshape();
  resolver.AddQuantize();
  resolver.AddDequantize();

  // Build interpreter
  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, TENSOR_ARENA_SIZE
  );
  interpreter = &static_interpreter;

  // Allocate tensors
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("ERROR: AllocateTensors() failed");
    return false;
  }

  // Get input/output tensors
  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.printf("✓ Model loaded\n");
  Serial.printf("  Input shape: [%d, %d]\n", input->dims->data[0], input->dims->data[1]);
  Serial.printf("  Output shape: [%d, %d]\n", output->dims->data[0], output->dims->data[1]);
  Serial.printf("  Arena used: %d / %d bytes\n",
                interpreter->arena_used_bytes(), TENSOR_ARENA_SIZE);

  return true;
}

void initSequence(const String& seed) {
  // Initialize sequence buffer with seed text
  int len = seed.length();
  int start = max(0, len - seqLength);

  for (int i = 0; i < seqLength; i++) {
    if (i < len - start) {
      char ch = seed.charAt(start + i);
      current_sequence[i] = char_to_idx[(unsigned char)ch];
    } else {
      current_sequence[i] = char_to_idx[(unsigned char)' '];
    }
  }
}

char generateNextChar(float temperature) {
  // Copy sequence to input tensor
  for (int i = 0; i < seqLength; i++) {
    input->data.int8[i] = (int8_t)current_sequence[i];
  }

  // Run inference
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    Serial.println("ERROR: Invoke failed");
    return '?';
  }

  // Get output probabilities
  float probs[256];
  float sum = 0.0;

  for (int i = 0; i < actualVocabSize; i++) {
    // Dequantize output
    float val = (output->data.int8[i] - output->params.zero_point) * output->params.scale;

    // Apply temperature
    val = val / temperature;
    probs[i] = exp(val);
    sum += probs[i];
  }

  // Normalize
  for (int i = 0; i < actualVocabSize; i++) {
    probs[i] /= sum;
  }

  // Sample from distribution
  float rand_val = random(0, 10000) / 10000.0;
  float cumsum = 0.0;
  int next_idx = 0;

  for (int i = 0; i < actualVocabSize; i++) {
    cumsum += probs[i];
    if (rand_val <= cumsum) {
      next_idx = i;
      break;
    }
  }

  // Update sequence buffer (shift left, add new)
  for (int i = 0; i < seqLength - 1; i++) {
    current_sequence[i] = current_sequence[i + 1];
  }
  current_sequence[seqLength - 1] = next_idx;

  return idx_to_char[next_idx];
}

void generateText(int length) {
  Serial.println("Generating text...\n");

  for (int i = 0; i < length; i++) {
    char nextChar = generateNextChar(0.8);
    Serial.print(nextChar);
    tft.print(nextChar);

    // Handle screen wrap
    if (tft.getCursorX() > tft.width() - 10) {
      tft.println();
    }
    if (tft.getCursorY() > tft.height() - 20) {
      tft.scroll(10);
    }

    delay(20);  // Slow down for readability
  }
}
