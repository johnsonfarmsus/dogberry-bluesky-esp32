# TFT Display Behavior

## Overview

The LilyGo T-Display S3's 1.9" TFT screen (170x320 pixels, ST7789V) cycles between showing user questions and Dogberry's replies in an alternating display pattern.

## Display Specifications

**Hardware:**
- Display: 1.9" TFT LCD
- Resolution: 170 (width) x 320 (height) pixels
- Driver: ST7789V
- Colors: 65K (16-bit RGB565)
- Orientation: Portrait mode (170x320)

**Text Rendering:**
- Font: TFT_eSPI built-in fonts (configurable)
- Recommended: Font 2 (16px) or Font 4 (26px) for readability
- Text color: Configurable (white on black background recommended)
- Word wrapping: Enabled to fit screen width

## Cycling Behavior

### Display Cycle Pattern

The display alternates between two screens every 6 seconds:

```
┌─────────────────────┐
│   USER QUESTION     │  ← Screen 1 (6 seconds)
│   (with username)   │
└─────────────────────┘
           ↓ (6 sec delay)
┌─────────────────────┐
│  DOGBERRY'S REPLY   │  ← Screen 2 (6 seconds)
│  (with @constable)  │
└─────────────────────┘
           ↓ (6 sec delay)
           ↓ (repeats)
```

**Timing:**
- Screen 1 (User Question): **6 seconds**
- Screen 2 (Dogberry Reply): **6 seconds**
- Total cycle: **12 seconds**
- Continuous loop until new interaction occurs

### Screen 1: User Question

**Layout:**
```
┌──────────────────────────┐
│ ┌──────────────────────┐ │
│ │  @username           │ │  ← Username (Font 2, color: CYAN)
│ └──────────────────────┘ │
│                          │
│ "What makes a good      │  ← Question text (Font 2, WHITE)
│  constable?             │     Word-wrapped, centered or
│  #askdogberry"          │     left-aligned
│                          │
│                          │
│ ┌──────────────────────┐ │
│ │ 🔵 ASKED             │ │  ← Status indicator
│ └──────────────────────┘ │
└──────────────────────────┘
```

**Content:**
- **Username**: `@username` (from Bluesky post author)
- **Question**: Full text of the #askdogberry post (truncated if > 280 chars)
- **Status**: "ASKED" with blue indicator

### Screen 2: Dogberry's Reply

**Layout:**
```
┌──────────────────────────┐
│ ┌──────────────────────┐ │
│ │  @constabledogberry  │ │  ← Bot name (Font 2, color: GREEN)
│ └──────────────────────┘ │
│                          │
│ "A good constable must  │  ← Reply text (Font 2, WHITE)
│  comprehend all vagrom  │     Word-wrapped, centered or
│  men with great         │     left-aligned
│  vigilance..."          │
│                          │
│ ┌──────────────────────┐ │
│ │ ✅ REPLIED           │ │  ← Status indicator
│ └──────────────────────┘ │
└──────────────────────────┘
```

**Content:**
- **Bot Name**: `@constabledogberry`
- **Reply**: Full generated response (truncated if > 280 chars)
- **Status**: "REPLIED" with green checkmark

## Implementation

### Display Update Trigger Points

**When to update display content:**
1. **New #askdogberry post detected** → Load new user question
2. **Reply generated** → Load new Dogberry response
3. **Daily post created** → Show daily post (special screen)
4. **Boot/Idle** → Show status screen

### Cycling Logic

```cpp
unsigned long lastDisplayUpdate = 0;
int currentDisplayState = 0;  // 0 = user question, 1 = dogberry reply

String currentUsername = "";
String currentQuestion = "";
String currentReply = "";

void updateDisplay() {
  unsigned long now = millis();

  // Switch display every 6 seconds
  if (now - lastDisplayUpdate >= 6000) {
    if (currentDisplayState == 0) {
      showUserQuestion(currentUsername, currentQuestion);
      currentDisplayState = 1;
    } else {
      showDogberryReply(currentReply);
      currentDisplayState = 0;
    }
    lastDisplayUpdate = now;
  }
}

void showUserQuestion(String username, String question) {
  tft.fillScreen(TFT_BLACK);

  // Username header
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("@" + username);

  // Question text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.setTextWrap(true);
  tft.println(question);

  // Status indicator
  tft.fillCircle(10, 300, 5, TFT_BLUE);
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.setCursor(25, 295);
  tft.println("ASKED");
}

void showDogberryReply(String reply) {
  tft.fillScreen(TFT_BLACK);

  // Bot name header
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("@constabledogberry");

  // Reply text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.setTextWrap(true);
  tft.println(reply);

  // Status indicator
  tft.fillCircle(10, 300, 5, TFT_GREEN);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(25, 295);
  tft.println("REPLIED");
}
```

### State Management

**Store current interaction:**
```cpp
struct DisplayState {
  String username;
  String question;
  String reply;
  unsigned long timestamp;
  bool hasContent;
};

DisplayState currentDisplay;

void setNewInteraction(String user, String quest, String rep) {
  currentDisplay.username = user;
  currentDisplay.question = quest;
  currentDisplay.reply = rep;
  currentDisplay.timestamp = millis();
  currentDisplay.hasContent = true;

  // Reset cycle to start with user question
  currentDisplayState = 0;
  lastDisplayUpdate = 0;
}
```

## Special Display Modes

### 1. Daily Post Screen

When daily witticism is posted, show a special screen for 12 seconds:

```
┌──────────────────────────┐
│ ┌──────────────────────┐ │
│ │ DAILY WISDOM         │ │  ← Header (YELLOW)
│ └──────────────────────┘ │
│                          │
│ "I have observed most   │  ← Daily post (WHITE)
│  preciessly that the    │
│  world is full of       │
│  vagrom men..."         │
│                          │
│ ┌──────────────────────┐ │
│ │ 📜 Posted 23:00 UTC  │ │  ← Timestamp
│ └──────────────────────┘ │
└──────────────────────────┘
```

**Duration**: 12 seconds, then resume normal cycling

### 2. Idle/Status Screen

When no recent interactions (> 5 minutes idle):

```
┌──────────────────────────┐
│                          │
│    CONSTABLE DOGBERRY    │  ← Title (GREEN, large font)
│                          │
│  Watching for           │  ← Status text (WHITE)
│  #askdogberry           │
│                          │
│  Last check:            │
│  2m ago                 │
│                          │
│  ┌────────────────────┐ │
│  │ WiFi: Connected    │ │  ← Connection status
│  │ Bluesky: Active    │ │
│  └────────────────────┘ │
└──────────────────────────┘
```

### 3. Generating Screen

While generating a reply (can take 12-25 seconds):

```
┌──────────────────────────┐
│                          │
│  GENERATING RESPONSE...  │  ← Title (YELLOW)
│                          │
│  ▓▓▓▓▓▓▓▓▓▓░░░░░░░       │  ← Progress bar
│                          │
│  "Good morrow, friend!   │  ← Generated text (real-time)
│   The watch must be      │     Shows characters as they
│   vigilant..."           │     are generated
│                          │
└──────────────────────────┘
```

**Updates in real-time** as characters are generated (every 50-100ms per character)

### 4. Boot Screen

On startup (2-3 seconds):

```
┌──────────────────────────┐
│                          │
│   ╔══════════════════╗   │
│   ║  CONSTABLE       ║   │  ← ASCII art or text logo
│   ║    DOGBERRY      ║   │     (GREEN/WHITE)
│   ╚══════════════════╝   │
│                          │
│   Loading model...       │  ← Status (YELLOW)
│   ░░░░░░▓▓▓░░░░░░░░      │     Progress bar
│                          │
└──────────────────────────┘
```

### 5. Error Screen

When errors occur:

```
┌──────────────────────────┐
│                          │
│     ⚠️  ERROR  ⚠️        │  ← Title (RED)
│                          │
│  WiFi connection lost    │  ← Error message (WHITE)
│                          │
│  Retrying in 30s...      │  ← Status (YELLOW)
│                          │
│  ┌────────────────────┐ │
│  │ Press button to    │ │  ← Instructions
│  │ retry now          │ │
│  └────────────────────┘ │
└──────────────────────────┘
```

## Text Truncation

**For long text that exceeds screen:**

```cpp
String truncateText(String text, int maxChars) {
  if (text.length() <= maxChars) {
    return text;
  }
  return text.substring(0, maxChars - 3) + "...";
}

// Usage:
String displayQuestion = truncateText(question, 200);  // ~8-10 lines
String displayReply = truncateText(reply, 200);
```

**Screen capacity** (approximate):
- Width: ~28 characters per line (Font 2, size 1)
- Height: ~20 lines visible
- Total: ~560 characters max (with wrapping)
- Recommended: **200-250 characters** for comfortable reading

## Color Scheme

**Recommended colors:**

```cpp
// Headers
#define USER_COLOR    TFT_CYAN      // User questions
#define DOGBERRY_COLOR TFT_GREEN    // Dogberry replies
#define DAILY_COLOR   TFT_YELLOW    // Daily posts
#define ERROR_COLOR   TFT_RED       // Errors

// Text
#define TEXT_COLOR    TFT_WHITE     // Main text
#define BG_COLOR      TFT_BLACK     // Background

// Status indicators
#define ASKED_COLOR   TFT_BLUE      // User asked
#define REPLIED_COLOR TFT_GREEN     // Bot replied
#define WAITING_COLOR TFT_ORANGE    // Processing
```

## Display Timing Summary

| Screen | Duration | Trigger |
|--------|----------|---------|
| User Question | 6 seconds | Part of cycle |
| Dogberry Reply | 6 seconds | Part of cycle |
| Daily Post | 12 seconds | After daily post |
| Idle Status | Continuous | No activity > 5 min |
| Generating | Real-time | During text generation |
| Boot | 2-3 seconds | On startup |
| Error | Until resolved | When error occurs |

## Loop Integration

**Main loop structure:**

```cpp
void loop() {
  // Update bot logic
  checkHashtagAndReply();    // Every 60s
  checkDailyPost();          // Every 5 min

  // Update display (non-blocking)
  updateDisplay();           // Every 6s

  delay(100);  // Small delay to prevent tight loop
}
```

**Non-blocking approach** ensures display updates don't interfere with WiFi operations or text generation.

## Future Enhancements

**Possible display improvements:**
- [ ] Smooth scrolling for long text
- [ ] Fade transitions between screens
- [ ] Touch support for manual screen switching
- [ ] QR code for Bluesky profile
- [ ] Real-time generation visualization (letter-by-letter)
- [ ] Engagement metrics (likes/reposts counter)
- [ ] Clock/time display in corner
- [ ] Battery level indicator

---

**Implementation Status**: Specification Complete
**Last Updated**: October 24, 2024
