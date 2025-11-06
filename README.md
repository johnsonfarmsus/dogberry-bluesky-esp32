# Dogberry Bot

An autonomous ESP32-powered Bluesky account (@constabledogberry.bsky.social) posting as Constable Dogberry with on-device AI text generation.

## About

Constable Dogberry operates his own Bluesky account, sharing wisdom (and malapropisms) from the watch. The bot monitors the `#askdogberry` hashtag, responds to questions, and posts daily witticisms - all powered by a micro language model running entirely on an ESP32-S3R8 microcontroller.

**Character**: Constable Dogberry from Shakespeare's "Much Ado About Nothing"
**Famous For**: Comic malapropisms and self-important bumbling wisdom

## Hardware

**LilyGo T-Display S3R8**
- MCU: ESP32-S3R8 Dual-core LX7 @ 240MHz
- PSRAM: 8MB OPI PSRAM
- Flash: 16MB
- Display: 1.9" TFT (ST7789V, 170x320)
- Wireless: WiFi 802.11 b/g/n, BLE 5
- Programming: Arduino IDE / PlatformIO

## Bot Behavior

### 1. Hashtag Monitoring: #askdogberry
- **Checks**: Every 60 seconds
- **Action**: Searches for new posts with `#askdogberry`
- **Response**: Replies to the most recent unanswered post
- **Tracking**: Remembers which posts have been answered to avoid duplicates

**Example**:
```
User: "What makes a good constable? #askdogberry"

Dogberry: "A good constable must comprehend all vagrom men with great
vigilance, and be most senseless in thy duty. For honesty is the mark
of a desartless fellow, as I am!"
```

### 2. Daily Wit/Witticism
- **Posts**: Once per day (23:00 UTC / 11:00 PM UTC)
- **Content**: Original Dogberry-style observations and wisdom
- **Uniqueness**: Date-based seed ensures different content each day

**Example Daily Posts**:
- "I have observed that the most senseless men are often the wisest..."
- "The watch doth report all vagrom men have been comprehended..."
- "Hear ye! 'Tis most tolerable when citizens obey the law..."

[→ See Full Behavior Documentation](BLUESKY_BEHAVIOR.md)

## Features

- ✅ **Autonomous Account** - Posts and replies as @dogberry.bsky.social
- ✅ **On-Device AI** - No cloud API calls for text generation
- ✅ **TensorFlow Lite Micro** - 121KB model fits in ESP32 PSRAM
- ✅ **Hashtag Monitoring** - Responds to #askdogberry every minute
- ✅ **Daily Posts** - Unique daily witticisms using date-based seeds
- ✅ **Duplicate Prevention** - Tracks replied posts to avoid spam
- ✅ **TFT Display** - Cycles between user questions and Dogberry's replies
- ✅ **Display Cycling** - Alternates every 6 seconds (user → reply → user...)
- ✅ **Low Power** - WiFi sleep mode between checks

### 3. TFT Display Behavior
- **Cycle Time**: 6 seconds per screen
- **Screen 1**: User's question with username (6 sec)
- **Screen 2**: Dogberry's reply (6 sec)
- **Pattern**: Continuously alternates until new interaction
- **Special Modes**: Daily post, generating, idle status, boot, errors

[→ See Full Display Documentation](DISPLAY_BEHAVIOR.md)

## Architecture

```
┌─────────────────────────┐
│    Bluesky API          │
│    (Cloud)              │
│  - Search #askdogberry  │
│  - Post daily wit       │
│  - Reply to questions   │
└───────────┬─────────────┘
            │ WiFi
            ▼
┌─────────────────────────┐
│  ESP32-S3R8             │
│  ┌───────────────────┐  │
│  │ Dogberry Model    │  │  ← 121KB LSTM
│  │ (TFLite Micro)    │  │
│  └───────────────────┘  │
│  ┌───────────────────┐  │
│  │ Response Engine   │  │  ← Generate text
│  └───────────────────┘  │
│  ┌───────────────────┐  │
│  │ Hashtag Monitor   │  │  ← Check every 60s
│  └───────────────────┘  │
│  ┌───────────────────┐  │
│  │ Daily Poster      │  │  ← Post once/day
│  └───────────────────┘  │
│  ┌───────────────────┐  │
│  │ TFT Display       │  │  ← Show status
│  └───────────────────┘  │
└─────────────────────────┘
```

## Installation

### Prerequisites

1. **Arduino IDE** or **PlatformIO**
2. **ESP32 Board Support**
3. **Required Libraries:**
   - TensorFlowLite_ESP32
   - TFT_eSPI
   - ArduinoJson
   - WiFi (built-in)
   - HTTPClient (built-in)
   - Preferences (built-in) - For state persistence

### Arduino IDE Setup

1. Install ESP32 board support:
   ```
   File → Preferences → Additional Board Manager URLs:
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

2. Install libraries via Library Manager:
   ```
   Tools → Manage Libraries:
   - Search "TensorFlowLite_ESP32" → Install
   - Search "TFT_eSPI" → Install
   - Search "ArduinoJson" → Install
   ```

3. Configure TFT_eSPI for LilyGo T-Display S3:
   ```
   Edit: libraries/TFT_eSPI/User_Setup_Select.h

   // Comment out:
   // #include <User_Setup.h>

   // Enable:
   #include <User_Setups/Setup206_LilyGo_T_Display_S3.h>
   ```

### Upload Model to ESP32

The trained model files must be uploaded to SPIFFS:

1. **Get the model files** from [dogberry-model](../dogberry-model/):
   ```bash
   cp ../dogberry-model/model/shakespeare_dogberry_finetuned.tflite esp32_inference/data/dogberry_model.tflite
   cp ../dogberry-model/model/vocab.json esp32_inference/data/
   ```

2. **Upload to SPIFFS:**
   - Arduino IDE: `Tools → ESP32 Sketch Data Upload`
   - PlatformIO: `pio run -t uploadfs`

3. **Flash the sketch:**
   ```
   Board: ESP32S3 Dev Module
   PSRAM: OPI PSRAM
   Flash Size: 16MB
   Partition: 16MB Flash (3MB APP / 9.9MB FATFS)
   Upload Speed: 921600
   ```

## Configuration

### 1. WiFi Setup

Create `esp32_inference/secrets.h`:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

// WiFi Credentials
const char* WIFI_SSID = "your-network-name";
const char* WIFI_PASSWORD = "your-password";

// Bluesky Credentials
const char* BLUESKY_HANDLE = "constabledogberry.bsky.social";
const char* BLUESKY_APP_PASSWORD = "xxxx-xxxx-xxxx-xxxx";

#endif
```

**Note:** `secrets.h` should be added to `.gitignore` to keep credentials private!

### 2. Bluesky Account Setup

1. Create a Bluesky account for Dogberry: https://bsky.app/
2. Create an App Password (NOT your main password):
   - Settings → App Passwords → Add App Password
   - Name it "ESP32 Bot"
   - Copy the generated password to `secrets.h`

### 3. Bot Behavior Settings

Edit `dogberry_bot.ino`:

```cpp
// Hashtag monitoring
#define HASHTAG_CHECK_INTERVAL 60000   // 60 seconds
#define HASHTAG_SEARCH "#askdogberry"

// Daily post
#define DAILY_POST_HOUR 23             // 23:00 UTC (11:00 PM)
#define DAILY_POST_MINUTE 0

// Text generation
#define TEMPERATURE 0.8                 // 0.5 = coherent, 1.0 = creative
#define RESPONSE_LENGTH 250             // Characters per response
```

## Usage

### Standalone Mode

1. Power on ESP32
2. Model loads from SPIFFS (~2 seconds)
3. Displays "Dogberry Bot Ready"
4. Press **IO14 button** to generate test text

### Bot Mode (Automatic)

1. ESP32 connects to WiFi
2. Syncs time with NTP server
3. Authenticates with Bluesky
4. Enters monitoring loop:
   - Every 60s: Check #askdogberry hashtag
   - Every 5 min: Check if daily post needed
   - On button press: Generate test text

### Manual Testing

Serial monitor commands (if implemented):
```
GENERATE - Generate and print text
SEARCH - Search #askdogberry
POST - Force daily post (for testing)
STATUS - Show bot status
```

## API Integration

### Bluesky AT Protocol Endpoints

**Authentication**:
```cpp
POST https://bsky.social/xrpc/com.atproto.server.createSession
Body: {"identifier": "dogberry.bsky.social", "password": "app_password"}
Response: {"accessJwt": "...", "did": "did:plc:..."}
```

**Search Hashtag**:
```cpp
GET https://bsky.social/xrpc/app.bsky.feed.searchPosts?q=%23askdogberry&limit=1
Response: {posts: [{uri, cid, author, text, ...}]}
```

**Create Reply**:
```cpp
POST https://bsky.social/xrpc/com.atproto.repo.createRecord
Body: {
  "repo": "did:plc:...",
  "collection": "app.bsky.feed.post",
  "record": {
    "text": "Dogberry's response...",
    "createdAt": "2024-10-24T12:00:00Z",
    "reply": {
      "root": {"uri": "at://...", "cid": "..."},
      "parent": {"uri": "at://...", "cid": "..."}
    }
  }
}
```

**Create Daily Post**:
```cpp
POST https://bsky.social/xrpc/com.atproto.repo.createRecord
Body: {
  "repo": "did:plc:...",
  "collection": "app.bsky.feed.post",
  "record": {
    "text": "Daily wisdom from Dogberry...",
    "createdAt": "2024-10-24T09:00:00Z"
  }
}
```

## Performance

| Metric | Value |
|--------|-------|
| Model load time | ~2 seconds |
| Inference per character | 50-100ms |
| 250 characters | ~12-25 seconds |
| Memory usage | ~2.1MB / 8MB PSRAM |
| Power consumption | ~200mA @ 5V (active) |
| WiFi sleep mode | ~80mA @ 5V |
| Hashtag check frequency | 60 seconds |
| Daily post | Once per day (23:00 UTC) |

## Examples

### Sample Interactions

**User:** "How should the watch behave? #askdogberry"

**Dogberry:** "The watch must be vigilant and senseless in their duty! Comprehend all vagrom men who wander the streets, and bid them stand in the Prince's name. If they will not stand, then let them go, and be thankful we are rid of such knaves..."

**User:** "What's the best advice for constables? #askdogberry"

**Dogberry:** "To be a desartless constable, one must be senseless and tolerable in all dealings. Remember that honesty is the mark of a true officer, and those who are most aspicious should be comprehended immediately!"

### Sample Daily Posts

```
Day 1: "I have observed most preciessly that the world is full of vagrom
men who require comprehension. The watch remains ever vigilant!"

Day 2: "Hear ye, citizens! 'Tis a truth most senseless that wisdom comes
to those who are desartless in their learning. Be thou wise!"

Day 3: "The watch doth report: All is well in the realm, save for those
benefactors - nay, malefactors - who disturb the peace!"
```

## Project Structure

```
dogberry-bot/
├── esp32_inference/
│   ├── dogberry_bot.ino           # Main sketch with bot logic
│   ├── secrets.h                  # WiFi & Bluesky credentials (not in git)
│   └── data/                      # SPIFFS data
│       ├── dogberry_model.tflite  # 121KB fine-tuned model
│       └── vocab.json             # Character mappings
├── BLUESKY_BEHAVIOR.md           # Detailed bot behavior docs
├── DEPLOYMENT_GUIDE.md           # Deployment instructions
└── README.md                     # This file
```

## Development

### Test Model Locally

```cpp
// In setup() or loop():
String seed = "Good morrow, friend! ";
String generated = generateText(seed, 200, 0.8);
Serial.println(generated);
```

### Monitor Serial Output

```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# Or with PlatformIO:
pio device monitor
```

### Debug Mode

Enable verbose logging in `dogberry_bot.ino`:

```cpp
#define DEBUG_MODE 1
```

This will print:
- Model loading progress
- WiFi connection status
- API request/response details
- Text generation progress
- State persistence operations

## Troubleshooting

### Model Won't Load

- Check SPIFFS upload succeeded: Monitor shows "Model loaded"
- Verify file size: `dogberry_model.tflite` should be ~121KB
- Ensure partition has FATFS space (9.9MB partition)
- Try re-uploading SPIFFS data

### Out of Memory

- Check PSRAM enabled: Board config shows "OPI PSRAM"
- Verify model is quantized (file size ~121KB not 1MB+)
- Reduce `TENSOR_ARENA_SIZE` if needed (currently 2MB)

### WiFi Connection Fails

- Check SSID/password in `secrets.h`
- Use 2.4GHz network only (ESP32 doesn't support 5GHz)
- Increase WiFi timeout in code
- Check WiFi signal strength

### Bluesky Authentication Fails

- Verify App Password (not main password)
- Check handle format: `dogberry.bsky.social` (no @ symbol in code)
- Ensure internet connectivity
- Check Bluesky API status

### Hashtag Not Detected

- Verify #askdogberry search works on Bluesky web
- Check API response in serial monitor (debug mode)
- Ensure time is synced (NTP)
- Verify JSON parsing works

### Daily Post Not Posting

- Check time sync (serial shows correct UTC time)
- Verify post hour setting matches UTC
- Check "last posted" date in preferences
- Enable debug mode to see post attempts

### Poor Text Quality

- Retrain model with more epochs (40+)
- Adjust temperature (try 0.7-0.9 range)
- Check fine-tuning completed successfully
- Verify correct model file uploaded

## Limitations

- **Context length:** 40 characters (model input)
- **No conversation history:** Each response is independent
- **No semantic understanding:** Pattern-based, not comprehension
- **Limited coherence:** May drift off topic in long responses
- **Character-level:** Slower than word-level models
- **Single language:** English only

## Future Enhancements

### Planned
- [ ] Thread awareness - don't reply twice in same thread
- [ ] User memory - track frequent users
- [ ] Multiple responses - generate several, pick best
- [ ] Engagement analytics - track likes/reposts

### Ideas
- [ ] React to mentions (without hashtag) occasionally
- [ ] Seasonal greetings - special posts on holidays
- [ ] Image generation - Dogberry ASCII art on TFT
- [ ] Web dashboard - view stats and logs
- [ ] OTA updates - update firmware remotely
- [ ] Battery support - run on LiPo battery

## Related Documentation

- [IMPLEMENTATION_REFERENCE.md](IMPLEMENTATION_REFERENCE.md) - **Code patterns and examples** (based on SeaGL badge)
- [BLUESKY_BEHAVIOR.md](BLUESKY_BEHAVIOR.md) - Detailed bot behavior specification
- [DISPLAY_BEHAVIOR.md](DISPLAY_BEHAVIOR.md) - TFT display cycling and UI design
- [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md) - Step-by-step deployment
- [dogberry-model](../dogberry-model/) - Model training repository

**External References:**
- [SeaGL 2025 Badge](https://github.com/johnsonfarmsus/seagl2025-badge-unofficial) - Reference implementation (same hardware)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
- [Bluesky AT Protocol](https://atproto.com/)

## License

MIT License - See LICENSE file

## Credits

- **Character**: Dogberry from Shakespeare's "Much Ado About Nothing"
- **Hardware**: LilyGo T-Display S3R8
- **ML Framework**: TensorFlow Lite for Microcontrollers
- **Platform**: Bluesky Social (AT Protocol)
- **Training Data**: Shakespeare's Complete Works + Synthetic Dogberry corpus
