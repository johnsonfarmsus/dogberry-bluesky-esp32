/*
 * Dogberry Bot - Shakespeare AI with Personality
 *
 * Hardware: LilyGo T-Display S3R8
 * - ESP32-S3R8 (8MB PSRAM, 16MB Flash)
 * - 1.9" TFT Display (ST7789V, 170x320)
 *
 * Features:
 * - TensorFlow Lite Micro inference
 * - Dogberry personality layer
 * - Bluesky social media integration
 * - On-device AI (no cloud for generation!)
 */

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "FS.h"
#include "SPIFFS.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

// WiFi credentials
const char* WIFI_SSID = "your-network-name";
const char* WIFI_PASSWORD = "your-password";

// Bluesky credentials (use App Password, not main password!)
const char* BLUESKY_HANDLE = "dogberry.bsky.social";
const char* BLUESKY_APP_PASSWORD = "xxxx-xxxx-xxxx-xxxx";

// Model configuration
#define VOCAB_SIZE 99
#define SEQ_LENGTH 40
#define TENSOR_ARENA_SIZE (2 * 1024 * 1024)  // 2MB

// Generation settings
#define TEMPERATURE 0.8
#define GENERATION_LENGTH 150
#define POLL_INTERVAL_MS 60000  // Check mentions every 60s

// ============================================================================
// DOGBERRY PERSONALITY
// ============================================================================

const char* dogberry_intros[] = {
  "Good morrow, friend! ",
  "As a most wise fellow, I say: ",
  "The watch doth comprehend that ",
  "Marry, 'tis most tolerable to note: ",
  "In my capacity as constable, I observe: ",
  "Verily, friend, ",
  "By my troth, ",
  "As one most desartless in these matters, "
};
const int NUM_INTROS = 8;

// Malapropism replacements (Dogberry's famous mistakes)
struct Malapropism {
  const char* correct;
  const char* dogberry;
};

const Malapropism malapropisms[] = {
  {"apprehend", "comprehend"},
  {"vagrant", "vagrom"},
  {"deserving", "desartless"},
  {"examination", "excommunication"},
  {"respected", "suspected"},
  {"assembly", "dissembly"},
  {"sensible", "senseless"},
  {"intolerable", "most tolerable"}
};
const int NUM_MALAPROPISMS = 8;

// ============================================================================
// GLOBALS
// ============================================================================

// TensorFlow Lite
namespace {
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* input = nullptr;
  TfLiteTensor* output = nullptr;
  uint8_t* tensor_arena = nullptr;
}

// Display
TFT_eSPI tft = TFT_eSPI();

// Vocabulary
StaticJsonDocument<16384> vocabDoc;
int actualVocabSize = 0;
char idx_to_char[256];
int char_to_idx[256];

// Bluesky
String blueskyDID = "";
String blueskyAccessToken = "";
unsigned long lastPollTime = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n========================================");
  Serial.println("DOGBERRY BOT");
  Serial.println("Shakespeare AI with Personality");
  Serial.println("========================================\n");

  // Initialize display
  initDisplay();

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("ERROR: SPIFFS failed");
    displayError("SPIFFS Error");
    return;
  }
  Serial.println("✓ SPIFFS mounted");

  // Allocate tensor arena in PSRAM
  if (!psramFound()) {
    Serial.println("ERROR: No PSRAM!");
    displayError("No PSRAM");
    return;
  }

  tensor_arena = (uint8_t*)ps_malloc(TENSOR_ARENA_SIZE);
  if (!tensor_arena) {
    Serial.println("ERROR: Tensor arena allocation failed");
    displayError("Memory Error");
    return;
  }
  Serial.printf("✓ Tensor arena: %d bytes in PSRAM\n", TENSOR_ARENA_SIZE);

  // Load vocabulary and model
  if (!loadVocabulary()) {
    displayError("Vocab Error");
    return;
  }

  if (!loadModel()) {
    displayError("Model Error");
    return;
  }

  // Connect to WiFi
  connectWiFi();

  // Authenticate with Bluesky
  if (!authenticateBluesky()) {
    displayError("Bluesky Auth Failed");
    Serial.println("⚠️  Continuing in local mode...");
  }

  // Display ready message
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.println("DOGBERRY");
  tft.println("Ready!");
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  Serial.println("\n✓ Bot ready!");
  Serial.println("Press IO14 to test generation");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Check for button press (test generation)
  if (digitalRead(14) == LOW) {
    delay(200);  // Debounce
    testGeneration();
  }

  // Poll Bluesky for mentions
  if (blueskyAccessToken.length() > 0 &&
      millis() - lastPollTime > POLL_INTERVAL_MS) {
    checkBlueskyMentions();
    lastPollTime = millis();
  }

  delay(100);
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void initDisplay() {
  tft.init();
  tft.setRotation(1);  // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.println("Dogberry Bot");
  tft.println("Initializing...");
}

void displayError(const char* msg) {
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setCursor(10, 10);
  tft.setTextSize(2);
  tft.println("ERROR:");
  tft.println(msg);
  Serial.printf("ERROR: %s\n", msg);
}

void displayText(const String& text) {
  tft.print(text);

  // Handle screen wrap
  if (tft.getCursorX() > tft.width() - 10) {
    tft.println();
  }
  if (tft.getCursorY() > tft.height() - 20) {
    tft.scroll(-10);
  }
}

// ============================================================================
// MODEL LOADING
// ============================================================================

bool loadVocabulary() {
  Serial.println("Loading vocabulary...");

  File file = SPIFFS.open("/vocab.json", "r");
  if (!file) {
    Serial.println("ERROR: vocab.json not found");
    return false;
  }

  DeserializationError error = deserializeJson(vocabDoc, file);
  file.close();

  if (error) {
    Serial.printf("ERROR: JSON parse failed: %s\n", error.c_str());
    return false;
  }

  actualVocabSize = vocabDoc["vocab_size"];
  Serial.printf("  Vocab size: %d\n", actualVocabSize);

  // Build character mappings
  JsonObject idx_to_char_obj = vocabDoc["idx_to_char"];
  JsonObject char_to_idx_obj = vocabDoc["char_to_idx"];

  for (int i = 0; i < 256; i++) {
    idx_to_char[i] = '?';
    char_to_idx[i] = 0;
  }

  for (JsonPair kv : idx_to_char_obj) {
    int idx = atoi(kv.key().c_str());
    const char* ch = kv.value();
    if (idx >= 0 && idx < 256) {
      idx_to_char[idx] = ch[0];
    }
  }

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
  Serial.println("Loading model...");

  File file = SPIFFS.open("/shakespeare_model.tflite", "r");
  if (!file) {
    Serial.println("ERROR: Model file not found");
    return false;
  }

  size_t modelSize = file.size();
  Serial.printf("  Model size: %d bytes (%.1f KB)\n", modelSize, modelSize/1024.0);

  uint8_t* model_data = (uint8_t*)ps_malloc(modelSize);
  if (!model_data) {
    Serial.println("ERROR: Model allocation failed");
    file.close();
    return false;
  }

  file.read(model_data, modelSize);
  file.close();

  model = tflite::GetModel(model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("ERROR: Model schema mismatch\n");
    return false;
  }

  static tflite::MicroMutableOpResolver<10> resolver;
  resolver.AddFullyConnected();
  resolver.AddSoftmax();
  resolver.AddLSTM();
  resolver.AddReshape();
  resolver.AddQuantize();
  resolver.AddDequantize();

  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, TENSOR_ARENA_SIZE
  );
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("ERROR: Tensor allocation failed");
    return false;
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("✓ Model loaded");
  Serial.printf("  Arena used: %d / %d bytes\n",
                interpreter->arena_used_bytes(), TENSOR_ARENA_SIZE);

  return true;
}

// ============================================================================
// TEXT GENERATION
// ============================================================================

String generateText(const String& seed, int length) {
  String generated = seed;

  // Pad seed if too short
  while (generated.length() < SEQ_LENGTH) {
    generated = " " + generated;
  }

  // Use last SEQ_LENGTH characters
  generated = generated.substring(generated.length() - SEQ_LENGTH);

  for (int i = 0; i < length; i++) {
    // Encode current sequence
    for (int j = 0; j < SEQ_LENGTH; j++) {
      char ch = generated.charAt(generated.length() - SEQ_LENGTH + j);
      input->data.int8[j] = (int8_t)char_to_idx[(unsigned char)ch];
    }

    // Run inference
    if (interpreter->Invoke() != kTfLiteOk) {
      Serial.println("ERROR: Inference failed");
      break;
    }

    // Sample next character
    char nextChar = sampleNextChar(TEMPERATURE);
    generated += nextChar;
  }

  return generated;
}

char sampleNextChar(float temperature) {
  float probs[256];
  float sum = 0.0;

  for (int i = 0; i < actualVocabSize; i++) {
    float val = (output->data.int8[i] - output->params.zero_point) * output->params.scale;
    val = val / temperature;
    probs[i] = exp(val);
    sum += probs[i];
  }

  for (int i = 0; i < actualVocabSize; i++) {
    probs[i] /= sum;
  }

  // Sample
  float randVal = random(10000) / 10000.0;
  float cumsum = 0.0;

  for (int i = 0; i < actualVocabSize; i++) {
    cumsum += probs[i];
    if (randVal <= cumsum) {
      return idx_to_char[i];
    }
  }

  return idx_to_char[0];
}

// ============================================================================
// DOGBERRY PERSONALITY
// ============================================================================

String addDogberryPersonality(const String& text) {
  // Add random Dogberry intro
  String result = String(dogberry_intros[random(NUM_INTROS)]);
  result += text;

  // Apply malapropisms
  for (int i = 0; i < NUM_MALAPROPISMS; i++) {
    result.replace(malapropisms[i].correct, malapropisms[i].dogberry);
  }

  return result;
}

void testGeneration() {
  Serial.println("\n--- Test Generation ---");

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("Dogberry says:");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  String seed = "What say you";
  String rawText = generateText(seed, GENERATION_LENGTH);
  String dogberryText = addDogberryPersonality(rawText.substring(seed.length()));

  Serial.println(dogberryText);
  displayText(dogberryText);
}

// ============================================================================
// WIFI & BLUESKY
// ============================================================================

void connectWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected");
    Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n⚠️  WiFi failed");
  }
}

bool authenticateBluesky() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  Serial.println("Authenticating with Bluesky...");

  HTTPClient http;
  http.begin("https://bsky.social/xrpc/com.atproto.server.createSession");
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"identifier\":\"" + String(BLUESKY_HANDLE) +
                   "\",\"password\":\"" + String(BLUESKY_APP_PASSWORD) + "\"}";

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    String response = http.getString();

    StaticJsonDocument<2048> doc;
    deserializeJson(doc, response);

    blueskyDID = doc["did"].as<String>();
    blueskyAccessToken = doc["accessJwt"].as<String>();

    http.end();

    Serial.println("✓ Bluesky authenticated");
    Serial.printf("  DID: %s\n", blueskyDID.c_str());
    return true;
  }

  http.end();
  Serial.printf("✗ Bluesky auth failed: %d\n", httpCode);
  return false;
}

void checkBlueskyMentions() {
  // TODO: Implement mention checking
  // This would poll the Bluesky API for new mentions
  // and generate responses using the model
  Serial.println("Checking mentions...");
}

void postToBluesky(const String& text) {
  if (blueskyAccessToken.length() == 0) {
    Serial.println("Not authenticated");
    return;
  }

  HTTPClient http;
  http.begin("https://bsky.social/xrpc/com.atproto.repo.createRecord");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + blueskyAccessToken);

  String payload = "{\"repo\":\"" + blueskyDID +
                   "\",\"collection\":\"app.bsky.feed.post\"," +
                   "\"record\":{\"text\":\"" + text +
                   "\",\"createdAt\":\"" + getISO8601Time() + "\"}}";

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    Serial.println("✓ Posted to Bluesky");
  } else {
    Serial.printf("✗ Post failed: %d\n", httpCode);
  }

  http.end();
}

String getISO8601Time() {
  // Simple ISO8601 timestamp (would need RTC for accuracy)
  return "2025-01-01T00:00:00.000Z";
}
