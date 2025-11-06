# Implementation Reference

## Overview

This document provides implementation guidance for the Dogberry Bot, referencing proven patterns from the [SeaGL 2025 Badge](https://github.com/johnsonfarmsus/seagl2025-badge-unofficial) project which successfully implements Bluesky integration and TFT display on the same hardware (LilyGo T-Display S3R8).

## Reference Project Analysis

**SeaGL 2025 Badge:**
- Hardware: LilyGo T-Display S3R8 (identical to Dogberry Bot)
- Display: 320x170 ST7789 TFT (landscape orientation)
- Framework: PlatformIO with Arduino
- Libraries: LVGL 8.4.0, ArduinoJson 6.21.5, QRCode 0.0.1
- Function: Monitors #seagl2025 hashtag, displays 3 most recent posts
- Refresh: 180 seconds (3 minutes) for API calls
- Display: 10-second screen cycling

**Key Differences for Dogberry Bot:**
- Dogberry: 170x320 (portrait) vs SeaGL: 320x170 (landscape)
- Dogberry: Reply generation (on-device AI) vs SeaGL: Display only
- Dogberry: 60-second hashtag check vs SeaGL: 180-second
- Dogberry: TFT_eSPI library vs SeaGL: LVGL framework

## Configuration Structure

Based on SeaGL's `config.h.example`, here's the recommended structure for Dogberry:

### dogberry-bot/esp32_inference/config.h

```cpp
#ifndef CONFIG_H
#define CONFIG_H

// ====== WiFi Configuration ======
#define WIFI_SSID "your-network-name"
#define WIFI_PASSWORD "your-password"
#define WIFI_TIMEOUT 10000  // 10 seconds

// ====== Bluesky Configuration ======
#define BLUESKY_IDENTIFIER "constabledogberry.bsky.social"
#define BLUESKY_APP_PASSWORD "xxxx-xxxx-xxxx-xxxx"
#define BLUESKY_SEARCH_TAG "askdogberry"  // Without # symbol
#define BLUESKY_API_BASE "https://bsky.social/xrpc"

// ====== Display Configuration ======
#define SCREEN_WIDTH 170
#define SCREEN_HEIGHT 320
#define SCREEN_ROTATION 0  // 0=portrait, 1=landscape, 2=portrait-flip, 3=landscape-flip
#define USER_SCREEN_TIME 6000     // 6 seconds
#define REPLY_SCREEN_TIME 6000    // 6 seconds
#define SPECIAL_SCREEN_TIME 12000 // 12 seconds for daily posts
#define BRIGHTNESS_DEFAULT 70     // 0-100%

// ====== Bot Behavior ======
#define HASHTAG_CHECK_INTERVAL 60000   // 60 seconds
#define DAILY_POST_HOUR 23              // 23:00 UTC
#define DAILY_POST_MINUTE 0
#define MAX_TRACKED_POSTS 50            // Recent replies to track

// ====== Text Generation ======
#define TEMPERATURE 0.8
#define RESPONSE_LENGTH 250
#define SEED_LENGTH 40

// ====== Model Configuration ======
#define MODEL_PATH "/dogberry_model.tflite"
#define VOCAB_PATH "/vocab.json"
#define TENSOR_ARENA_SIZE (2 * 1024 * 1024)  // 2MB

// ====== Debug Settings ======
#define DEBUG_MODE 1
#define SERIAL_BAUD 115200

#endif
```

## Bluesky API Implementation

### Authentication Pattern (from SeaGL)

```cpp
#include <HTTPClient.h>
#include <ArduinoJson.h>

String accessJwt = "";
String did = "";

bool authenticateBluesky() {
  HTTPClient http;
  http.begin(String(BLUESKY_API_BASE) + "/com.atproto.server.createSession");
  http.addHeader("Content-Type", "application/json");

  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["identifier"] = BLUESKY_IDENTIFIER;
  doc["password"] = BLUESKY_APP_PASSWORD;

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    String response = http.getString();

    // Parse response
    DynamicJsonDocument responseDoc(2048);
    deserializeJson(responseDoc, response);

    accessJwt = responseDoc["accessJwt"].as<String>();
    did = responseDoc["did"].as<String>();

    #ifdef DEBUG_MODE
    Serial.println("✓ Bluesky authenticated");
    Serial.println("  DID: " + did);
    #endif

    http.end();
    return true;
  } else {
    #ifdef DEBUG_MODE
    Serial.println("✗ Auth failed: " + String(httpCode));
    #endif
    http.end();
    return false;
  }
}
```

### Search Hashtag Pattern

```cpp
struct BlueskyPost {
  String uri;
  String cid;
  String author;
  String authorHandle;
  String text;
  String createdAt;
};

BlueskyPost latestPost;

bool searchHashtag() {
  if (accessJwt == "") {
    if (!authenticateBluesky()) return false;
  }

  HTTPClient http;
  String url = String(BLUESKY_API_BASE) +
               "/app.bsky.feed.searchPosts?q=%23" +
               BLUESKY_SEARCH_TAG +
               "&limit=1";

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + accessJwt);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String response = http.getString();

    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      #ifdef DEBUG_MODE
      Serial.println("✗ JSON parse error: " + String(error.c_str()));
      #endif
      http.end();
      return false;
    }

    if (doc["posts"].size() > 0) {
      JsonObject post = doc["posts"][0];

      latestPost.uri = post["uri"].as<String>();
      latestPost.cid = post["cid"].as<String>();
      latestPost.author = post["author"]["displayName"].as<String>();
      latestPost.authorHandle = post["author"]["handle"].as<String>();
      latestPost.text = post["record"]["text"].as<String>();
      latestPost.createdAt = post["record"]["createdAt"].as<String>();

      #ifdef DEBUG_MODE
      Serial.println("✓ Found post:");
      Serial.println("  @" + latestPost.authorHandle + ": " + latestPost.text);
      #endif

      http.end();
      return true;
    }
  } else if (httpCode == 401) {
    // Token expired, re-authenticate
    accessJwt = "";
    http.end();
    return searchHashtag();  // Retry
  }

  http.end();
  return false;
}
```

### Create Reply Pattern

```cpp
bool postReply(String replyText, String parentUri, String parentCid, String rootUri, String rootCid) {
  if (accessJwt == "") {
    if (!authenticateBluesky()) return false;
  }

  HTTPClient http;
  http.begin(String(BLUESKY_API_BASE) + "/com.atproto.repo.createRecord");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + accessJwt);

  // Get current timestamp
  time_t now;
  time(&now);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

  // Create JSON payload
  DynamicJsonDocument doc(2048);
  doc["repo"] = did;
  doc["collection"] = "app.bsky.feed.post";

  JsonObject record = doc.createNestedObject("record");
  record["text"] = replyText;
  record["createdAt"] = timestamp;
  record["$type"] = "app.bsky.feed.post";

  JsonObject reply = record.createNestedObject("reply");
  JsonObject parent = reply.createNestedObject("parent");
  parent["uri"] = parentUri;
  parent["cid"] = parentCid;

  JsonObject root = reply.createNestedObject("root");
  root["uri"] = rootUri;
  root["cid"] = rootCid;

  String payload;
  serializeJson(doc, payload);

  #ifdef DEBUG_MODE
  Serial.println("Posting reply:");
  Serial.println(payload);
  #endif

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    #ifdef DEBUG_MODE
    Serial.println("✓ Reply posted successfully");
    #endif
    http.end();
    return true;
  } else {
    #ifdef DEBUG_MODE
    Serial.println("✗ Reply failed: " + String(httpCode));
    Serial.println(http.getString());
    #endif
    http.end();
    return false;
  }
}
```

### Create Daily Post Pattern

```cpp
bool postDailyWit(String text) {
  if (accessJwt == "") {
    if (!authenticateBluesky()) return false;
  }

  HTTPClient http;
  http.begin(String(BLUESKY_API_BASE) + "/com.atproto.repo.createRecord");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + accessJwt);

  // Get current timestamp
  time_t now;
  time(&now);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

  // Create JSON payload
  DynamicJsonDocument doc(1024);
  doc["repo"] = did;
  doc["collection"] = "app.bsky.feed.post";

  JsonObject record = doc.createNestedObject("record");
  record["text"] = text;
  record["createdAt"] = timestamp;
  record["$type"] = "app.bsky.feed.post";

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    #ifdef DEBUG_MODE
    Serial.println("✓ Daily post published");
    #endif
    http.end();
    return true;
  } else {
    #ifdef DEBUG_MODE
    Serial.println("✗ Daily post failed: " + String(httpCode));
    #endif
    http.end();
    return false;
  }
}
```

## Display Implementation (TFT_eSPI)

### Setup Pattern

```cpp
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setupDisplay() {
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(TFT_BLACK);
  tft.setTextWrap(true);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Set brightness (using PWM on backlight pin)
  analogWrite(TFT_BL, map(BRIGHTNESS_DEFAULT, 0, 100, 0, 255));

  showBootScreen();
}
```

### Screen Cycling Implementation

```cpp
enum DisplayState {
  DISPLAY_USER_QUESTION,
  DISPLAY_DOGBERRY_REPLY,
  DISPLAY_DAILY_POST,
  DISPLAY_GENERATING,
  DISPLAY_IDLE
};

DisplayState currentDisplay = DISPLAY_IDLE;
unsigned long lastDisplayUpdate = 0;
int displayCycleState = 0;  // 0 = user, 1 = reply

void updateDisplay() {
  unsigned long now = millis();
  unsigned long displayTime = (currentDisplay == DISPLAY_DAILY_POST) ?
                               SPECIAL_SCREEN_TIME :
                               (displayCycleState == 0 ? USER_SCREEN_TIME : REPLY_SCREEN_TIME);

  if (now - lastDisplayUpdate >= displayTime) {
    if (currentDisplay == DISPLAY_USER_QUESTION || currentDisplay == DISPLAY_DOGBERRY_REPLY) {
      // Toggle between user and reply
      displayCycleState = 1 - displayCycleState;

      if (displayCycleState == 0) {
        showUserQuestion();
      } else {
        showDogberryReply();
      }
    } else if (currentDisplay == DISPLAY_DAILY_POST) {
      // Daily post shown for 12 seconds, then back to cycling
      currentDisplay = DISPLAY_USER_QUESTION;
      displayCycleState = 0;
      showUserQuestion();
    }

    lastDisplayUpdate = now;
  }
}

void showUserQuestion() {
  tft.fillScreen(TFT_BLACK);

  // Header
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("@" + latestPost.authorHandle);

  // Question text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.println(latestPost.text);

  // Status indicator
  tft.fillCircle(10, 300, 5, TFT_BLUE);
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.setCursor(25, 295);
  tft.println("ASKED");

  currentDisplay = DISPLAY_USER_QUESTION;
}

void showDogberryReply() {
  tft.fillScreen(TFT_BLACK);

  // Header
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("@constabledogberry");

  // Reply text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.println(lastGeneratedReply);

  // Status indicator
  tft.fillCircle(10, 300, 5, TFT_GREEN);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(25, 295);
  tft.println("REPLIED");

  currentDisplay = DISPLAY_DOGBERRY_REPLY;
}
```

## State Persistence (Preferences)

### Tracking Replied Posts

```cpp
#include <Preferences.h>

Preferences prefs;

void setupPreferences() {
  prefs.begin("dogberry", false);  // false = read/write
}

bool hasRepliedTo(String postUri) {
  String key = "replied_" + String(postUri.hashCode());
  return prefs.getBool(key.c_str(), false);
}

void markAsReplied(String postUri) {
  String key = "replied_" + String(postUri.hashCode());
  prefs.putBool(key.c_str(), true);

  #ifdef DEBUG_MODE
  Serial.println("Marked as replied: " + postUri);
  #endif
}

void cleanOldReplies() {
  // Periodically clear old replied posts (e.g., >7 days)
  // Prevents Preferences from filling up
  // Implementation depends on timestamp tracking
}
```

### Daily Post Tracking

```cpp
struct DailyPostState {
  int lastDay;
  int lastMonth;
  int lastYear;
};

bool hasPostedToday() {
  DailyPostState state;
  state.lastDay = prefs.getInt("lastDay", 0);
  state.lastMonth = prefs.getInt("lastMonth", 0);
  state.lastYear = prefs.getInt("lastYear", 0);

  time_t now;
  time(&now);
  struct tm* timeinfo = gmtime(&now);

  return (state.lastDay == timeinfo->tm_mday &&
          state.lastMonth == timeinfo->tm_mon &&
          state.lastYear == timeinfo->tm_year);
}

void markPostedToday() {
  time_t now;
  time(&now);
  struct tm* timeinfo = gmtime(&now);

  prefs.putInt("lastDay", timeinfo->tm_mday);
  prefs.putInt("lastMonth", timeinfo->tm_mon);
  prefs.putInt("lastYear", timeinfo->tm_year);

  #ifdef DEBUG_MODE
  Serial.println("Marked daily post complete");
  #endif
}
```

## Main Loop Structure

### Non-Blocking Pattern (from SeaGL approach)

```cpp
unsigned long lastHashtagCheck = 0;
unsigned long lastDailyCheck = 0;

void loop() {
  unsigned long now = millis();

  // Check #askdogberry every 60 seconds
  if (now - lastHashtagCheck >= HASHTAG_CHECK_INTERVAL) {
    checkAndReply();
    lastHashtagCheck = now;
  }

  // Check daily post every 5 minutes
  if (now - lastDailyCheck >= 300000) {
    checkDailyPost();
    lastDailyCheck = now;
  }

  // Update display (non-blocking)
  updateDisplay();

  // Handle button presses
  handleButtons();

  delay(100);  // Small delay to prevent tight loop
}

void checkAndReply() {
  #ifdef DEBUG_MODE
  Serial.println("Checking #askdogberry...");
  #endif

  if (searchHashtag()) {
    // Check if we've already replied to this post
    if (!hasRepliedTo(latestPost.uri)) {
      // Generate reply using TFLite model
      String seed = latestPost.text.substring(0, min(40, (int)latestPost.text.length()));
      String reply = generateText(seed, RESPONSE_LENGTH, TEMPERATURE);

      // Post reply
      if (postReply(reply, latestPost.uri, latestPost.cid, latestPost.uri, latestPost.cid)) {
        markAsReplied(latestPost.uri);

        // Update display with new interaction
        lastGeneratedReply = reply;
        currentDisplay = DISPLAY_USER_QUESTION;
        displayCycleState = 0;
        lastDisplayUpdate = millis();
      }
    } else {
      #ifdef DEBUG_MODE
      Serial.println("Already replied to this post");
      #endif
    }
  }
}

void checkDailyPost() {
  time_t now;
  time(&now);
  struct tm* timeinfo = gmtime(&now);

  // Check if it's 23:00 UTC and we haven't posted today
  if (timeinfo->tm_hour == DAILY_POST_HOUR &&
      timeinfo->tm_min < 5 &&  // 5-minute window
      !hasPostedToday()) {

    // Generate daily wit using date-based seed
    char seedBuffer[100];
    sprintf(seedBuffer, "Day %d of month %d brings wisdom: ",
            timeinfo->tm_mday, timeinfo->tm_mon + 1);

    String dailyWit = generateText(String(seedBuffer), 200, 0.8);

    if (postDailyWit(dailyWit)) {
      markPostedToday();

      // Show daily post on display
      showDailyPost(dailyWit);
      currentDisplay = DISPLAY_DAILY_POST;
      lastDisplayUpdate = millis();
    }
  }
}
```

## Library Dependencies

### platformio.ini

```ini
[env:lilygo-t-display-s3]
platform = espressif32
board = lilygo-t-display-s3
framework = arduino

lib_deps =
    bodmer/TFT_eSPI @ ^2.5.43
    bblanchon/ArduinoJson @ ^6.21.5
    tensorflow/TensorFlowLite_ESP32 @ ^2.0.0

build_flags =
    -DUSER_SETUP_LOADED=1
    -DST7789_DRIVER=1
    -DCGRAM_OFFSET=1
    -DTFT_WIDTH=170
    -DTFT_HEIGHT=320
    -DTFT_MISO=-1
    -DTFT_MOSI=35
    -DTFT_SCLK=36
    -DTFT_CS=34
    -DTFT_DC=37
    -DTFT_RST=38
    -DTFT_BL=33
    -DLOAD_GLCD=1
    -DLOAD_FONT2=1
    -DLOAD_FONT4=1
    -DLOAD_FONT6=1
    -DLOAD_FONT7=1
    -DLOAD_FONT8=1
    -DLOAD_GFXFF=1
    -DSMOOTH_FONT=1
    -DSPI_FREQUENCY=40000000
```

## Key Takeaways from SeaGL Badge

1. **Use App Passwords**: Never use main account password
2. **ArduinoJson**: Industry-standard for AT Protocol JSON parsing
3. **Non-blocking timers**: Use millis() for all timing
4. **Token refresh**: Handle 401 errors by re-authenticating
5. **Preferences library**: Built-in ESP32 NVS for persistence
6. **Serial debugging**: Essential for development (115200 baud)
7. **Screen timing**: 10 seconds works well for SeaGL, 6 seconds for Dogberry
8. **API refresh**: 3 minutes for SeaGL, 1 minute for Dogberry (more responsive)

## Testing Checklist

- [ ] WiFi connection on 2.4GHz network
- [ ] Bluesky authentication with app password
- [ ] Hashtag search returns posts
- [ ] JSON parsing works correctly
- [ ] Reply posting succeeds
- [ ] Display cycles properly
- [ ] Preferences persist across reboots
- [ ] Daily post triggers at correct time
- [ ] Duplicate reply prevention works
- [ ] Model loads and generates text
- [ ] TFT displays text correctly
- [ ] Time sync via NTP works

---

**Status**: Implementation guide complete
**Reference**: [SeaGL 2025 Badge](https://github.com/johnsonfarmsus/seagl2025-badge-unofficial)
**Last Updated**: October 24, 2024
