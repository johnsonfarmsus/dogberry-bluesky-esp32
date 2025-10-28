# Dogberry Bot Deployment Guide

Complete guide to deploy your Dogberry Shakespeare bot on ESP32-S3R8.

## Hardware Requirements

**LilyGo T-Display S3R8**
- ESP32-S3R8 (8MB PSRAM, 16MB Flash)
- 1.9" TFT Display (170x320)
- USB-C connection

## Prerequisites

### 1. Arduino IDE Setup

**Install Arduino IDE 2.x**
- Download from: https://www.arduino.cc/en/software

**Add ESP32 Board Support**
1. File → Preferences
2. Additional Board Manager URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Tools → Board → Boards Manager
4. Search "esp32" → Install "esp32" by Espressif

### 2. Install Libraries

Via Library Manager (Tools → Manage Libraries):

1. **TensorFlowLite_ESP32**
   - Search: "TensorFlowLite_ESP32"
   - Install latest version

2. **TFT_eSPI**
   - Search: "TFT_eSPI"
   - Install by Bodmer

3. **ArduinoJson**
   - Search: "ArduinoJson"
   - Install by Benoit Blanchon

### 3. Configure TFT_eSPI

**Important:** Configure for T-Display S3

Edit: `libraries/TFT_eSPI/User_Setup_Select.h`

Comment out default setup:
```cpp
// #include <User_Setup.h>
```

Enable T-Display S3:
```cpp
#include <User_Setups/Setup206_LilyGo_T_Display_S3.h>
```

## Model Preparation

### 1. Get Trained Model

After training completes (check with `tail -f shakespeare_training.log`):

```bash
cd dogberry-model
python3 export_tflite.py  # If not already exported
```

### 2. Prepare SPIFFS Upload

Create `data/` folder in sketch directory:

```bash
mkdir -p dogberry-bot/esp32_inference/data
```

Copy model files:

```bash
# Copy from training output
cp dogberry-model/model/shakespeare_model.tflite dogberry-bot/esp32_inference/data/
cp dogberry-model/model/vocab.json dogberry-bot/esp32_inference/data/
```

Verify files:

```bash
ls -lh dogberry-bot/esp32_inference/data/
# Should show:
# shakespeare_model.tflite (~120KB)
# vocab.json (~2KB)
```

## Configuration

### 1. WiFi Settings

Edit `dogberry_bot.ino`:

```cpp
const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASSWORD = "YourPassword";
```

### 2. Bluesky Credentials

**Create App Password** (NOT your main password!):
1. Go to https://bsky.app/settings/app-passwords
2. Click "Add App Password"
3. Name it "dogberry-bot"
4. Copy the generated password

**Update sketch:**

```cpp
const char* BLUESKY_HANDLE = "yourhandle.bsky.social";
const char* BLUESKY_APP_PASSWORD = "abcd-efgh-ijkl-mnop";
```

### 3. Adjust Generation Settings (Optional)

```cpp
#define TEMPERATURE 0.8         // 0.5-1.0 (higher = more creative)
#define GENERATION_LENGTH 150   // Characters to generate
#define POLL_INTERVAL_MS 60000  // Check mentions every 60s
```

## Upload to ESP32

### 1. Board Configuration

Tools → Board → ESP32 Arduino → **ESP32S3 Dev Module**

**Critical Settings:**
```
USB CDC On Boot: Enabled
CPU Frequency: 240MHz (WiFi)
Flash Mode: QIO 80MHz
Flash Size: 16MB (3MB APP / 9.9MB FATFS)
Partition Scheme: 16MB Flash (3MB APP / 9.9MB FATFS)
PSRAM: OPI PSRAM
Upload Speed: 921600
```

### 2. Upload SPIFFS Data

**Install SPIFFS Upload Tool:**
- Arduino IDE 2.x: Use [arduino-esp32fs-plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin)
- Or use ESP32 Sketch Data Upload tool

**Upload:**
1. Close Serial Monitor
2. Tools → ESP32 Sketch Data Upload
3. Wait for upload to complete (~30 seconds)

### 3. Compile and Upload Sketch

1. Open `dogberry_bot.ino`
2. Verify (checkmark button)
3. Upload (arrow button)
4. Monitor progress in output window

### 4. Monitor Serial Output

Tools → Serial Monitor (115200 baud)

You should see:
```
========================================
DOGBERRY BOT
Shakespeare AI with Personality
========================================

✓ SPIFFS mounted
✓ Tensor arena: 2097152 bytes in PSRAM
Loading vocabulary...
  Vocab size: 99
✓ Vocabulary loaded
Loading model...
  Model size: 123968 bytes (121.1 KB)
✓ Model loaded
  Arena used: 1523456 / 2097152 bytes
Connecting to WiFi...
✓ WiFi connected
  IP: 192.168.1.XXX
Authenticating with Bluesky...
✓ Bluesky authenticated
  DID: did:plc:...

✓ Bot ready!
Press IO14 to test generation
```

## Testing

### 1. Test Text Generation

**Press IO14 button** on the T-Display

Watch the display and serial monitor for generated text with Dogberry personality:

```
Dogberry says:
Good morrow, friend! What say you to the watch tonight?
The most desartless men shall comprehend all vagrom persons...
```

### 2. Test Bluesky Connection

Monitor serial output for:
```
Checking mentions...
```

### 3. Post Test Message

Modify code to test posting:

```cpp
void loop() {
  static bool tested = false;
  if (!tested && blueskyAccessToken.length() > 0) {
    postToBluesky("Good morrow! I am Dogberry, a most wise constable!");
    tested = true;
  }
  // ... rest of loop
}
```

## Troubleshooting

### Model Won't Load

**Problem:** "ERROR: Model file not found"

**Solution:**
- Verify SPIFFS upload succeeded
- Check file size: `ls -lh data/`
- Re-upload with ESP32 Sketch Data Upload

### Out of Memory

**Problem:** "ERROR: Tensor allocation failed"

**Solution:**
- Check PSRAM is enabled in board config
- Reduce TENSOR_ARENA_SIZE in code:
  ```cpp
  #define TENSOR_ARENA_SIZE (1 * 1024 * 1024)  // Try 1MB
  ```

### WiFi Connection Fails

**Problem:** Can't connect to WiFi

**Solution:**
- Check SSID/password are correct
- Use 2.4GHz network (ESP32 doesn't support 5GHz)
- Check router MAC filtering
- Try increasing timeout

### Bluesky Auth Fails

**Problem:** "✗ Bluesky auth failed"

**Solution:**
- Verify handle format: `handle.bsky.social`
- Ensure using **App Password**, not main password
- Check internet connection
- Try re-creating app password

### Text Generation is Gibberish

**Problem:** Generated text doesn't make sense

**Solution:**
- Wait for training to complete (40 epochs)
- Check `shakespeare_training.log` for errors
- Try adjusting temperature:
  ```cpp
  #define TEMPERATURE 0.7  // Lower = more coherent
  ```
- Verify correct model file uploaded

### Display Issues

**Problem:** Display is blank or garbled

**Solution:**
- Verify TFT_eSPI configuration
- Check User_Setup_Select.h points to Setup206
- Test with TFT_eSPI examples first
- Check ribbon cable connection

## Bot Behavior

### How It Works

1. **Polls Bluesky** every 60 seconds for mentions
2. **Generates response** using Shakespeare model
3. **Adds Dogberry personality:**
   - Random Dogberry intro phrase
   - Malapropism replacements
4. **Posts reply** to Bluesky

### Dogberry Personality Features

**Intros:**
- "Good morrow, friend!"
- "As a most wise fellow, I say:"
- "The watch doth comprehend that..."

**Malapropisms:**
- "comprehend" (instead of "apprehend")
- "vagrom" (instead of "vagrant")
- "desartless" (instead of "deserving")
- "most tolerable" (instead of "intolerable")

### Example Interaction

**Mention:** "@dogberry what's happening?"

**Dogberry:** "Good morrow, friend! The watch doth comprehend all vagrom men tonight. As a most wise fellow, I say the matter is most tolerable and not to be endured..."

## Performance

| Metric | Value |
|--------|-------|
| Model Load Time | ~2-3 seconds |
| Generation Speed | 50-100ms/character |
| 150 characters | ~7-15 seconds |
| Memory Usage | ~1.5MB / 8MB PSRAM |
| Power Draw | ~200mA @ 5V |

## Next Steps

### Enhancements

1. **Add More Personality:**
   - Expand malapropism list
   - Context-aware responses
   - Multiple character modes

2. **Improve Bluesky Integration:**
   - Reply threading
   - Mention context
   - Rate limiting
   - Error handling

3. **Add Features:**
   - Web interface for config
   - OTA updates
   - Multiple personalities
   - Conversation history

4. **Optimize Performance:**
   - Caching
   - Batch generation
   - Model compression

## Resources

- [Project README](README.md)
- [Model Training](../dogberry-model/README.md)
- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [Bluesky AT Protocol](https://atproto.com/)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)

## Support

- GitHub Issues: (your-repo-url)
- Bluesky: @dogberry.bsky.social
- Email: (your-email)

---

**Created with Claude Code**
Bot powered by Shakespeare, personality by Dogberry!
