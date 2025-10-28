# Dogberry Bot Bluesky Behavior

## Overview

Constable Dogberry operates as an autonomous Bluesky account (@dogberry.bsky.social or similar), posting and responding with his characteristic malapropisms and self-important wisdom.

## Bot Behaviors

### 1. Hashtag Monitoring: #askdogberry

**Frequency**: Every 60 seconds

**Action**: Check for new posts with `#askdogberry` hashtag

**Logic**:
1. Search Bluesky for posts containing `#askdogberry`
2. Get most recent post
3. Check if Dogberry has already replied to this post (track by post URI)
4. If new and unanswered:
   - Generate response using on-device LLM
   - Reply to the post as Constable Dogberry
   - Store post URI to avoid duplicate replies

**Response Generation**:
- Use seed text from the original post (first 40 characters)
- Generate 200-300 characters of Dogberry-style response
- Temperature: 0.7-0.8 for creative but coherent responses

**Example**:
```
User posts: "What's the secret to being a good constable? #askdogberry"

Dogberry replies: "Mark thee well, friend! The secret to being a most
desartless constable is to comprehend all vagrom men with great vigilance,
and to be senseless in thy duty. For I am as honest a man as any living
that is an old man and no honester than I. 'Tis most tolerable when the
watch doth sleep on their duty!"
```

### 2. Daily Wit/Witticism Post

**Frequency**: Once per day at a consistent time (e.g., 9:00 AM UTC)

**Action**: Post an original Dogberry-style observation or wisdom

**Uniqueness Strategy**: Use daily seed variation to ensure unique posts

**Implementation Options**:

#### Option A: Date-Based Seed (Recommended)
```cpp
// Use current date as seed modifier
String dailySeed = "Good morrow! " + String(day) + String(month);
// Generates different output each day based on date
```

**Pros**:
- Deterministic but unique per day
- No external randomness needed
- Repeatable for testing

**Cons**:
- Will repeat after 365 days (but with different model, may vary)

#### Option B: Incremental Counter
```cpp
// Store counter in EEPROM/preferences
int postNumber = getStoredPostNumber();
String dailySeed = "Wisdom the " + String(postNumber) + "th: ";
incrementPostNumber();
```

**Pros**:
- Guaranteed unique forever
- Sequential tracking

**Cons**:
- Requires persistent storage
- Counter management

#### Option C: Hybrid (Date + Time Components)
```cpp
// Combine date with week number and day of year
String dailySeed = "Day " + String(dayOfYear) + " wisdom: ";
```

**Pros**:
- More variation than simple date
- Still deterministic

**Cons**:
- More complex seed generation

### Recommended Approach: Option A (Date-Based)

**Implementation**:
```cpp
void postDailyWit() {
  // Get current date
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  // Create unique daily seed
  char seedBuffer[100];
  sprintf(seedBuffer, "Day %d of month %d brings wisdom: ",
          timeinfo.tm_mday, timeinfo.tm_mon + 1);

  // Generate text (200-250 chars for a tweet-sized post)
  String wisdom = generateText(String(seedBuffer), 200, 0.8);

  // Post to Bluesky
  postToBluesky(wisdom);

  // Store that we posted today (prevent duplicates)
  saveLastPostDate(timeinfo.tm_mday, timeinfo.tm_mon);
}
```

**Daily Post Types** (varied by seed):
- **Observations**: "I have observed that..."
- **Proclamations**: "Hear ye, citizens of the realm..."
- **Instructions**: "Let all men know that..."
- **Wisdom**: "As a fellow of great desart, I say..."
- **Watch Reports**: "The watch doth report that..."

**Example Daily Posts**:
```
Day 1: "I have observed that those who are most senseless in their
learning are often the most wise, for they comprehend the world with
fresh eyes. Be thou senseless, good citizens!"

Day 2: "Hear ye! The watch doth report all is well in the realm,
save for three vagrom men who were most tolerable in their behavior.
They have been comprehended and shall face excommunication forthwith!"

Day 3: "As a constable of great desart, I proclaim that honesty is
the best policy, unless one is being questioned by the watch, in
which case truth is also most aspicious!"
```

## Technical Implementation

### State Management

**Track Replied Posts**:
```cpp
// Store in preferences (persistent)
const int MAX_TRACKED_POSTS = 50;
String repliedPosts[MAX_TRACKED_POSTS];
int repliedCount = 0;

bool hasRepliedTo(String postUri) {
  for (int i = 0; i < repliedCount; i++) {
    if (repliedPosts[i] == postUri) return true;
  }
  return false;
}

void markAsReplied(String postUri) {
  if (repliedCount < MAX_TRACKED_POSTS) {
    repliedPosts[repliedCount++] = postUri;
  } else {
    // Rotate: remove oldest, add newest
    for (int i = 0; i < MAX_TRACKED_POSTS - 1; i++) {
      repliedPosts[i] = repliedPosts[i + 1];
    }
    repliedPosts[MAX_TRACKED_POSTS - 1] = postUri;
  }
  saveRepliedPosts(); // Persist to NVS
}
```

**Track Daily Post**:
```cpp
struct LastPost {
  int day;
  int month;
  int year;
};

bool hasPostedToday() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  LastPost last = loadLastPostDate();

  return (last.day == timeinfo.tm_mday &&
          last.month == timeinfo.tm_mon &&
          last.year == timeinfo.tm_year);
}
```

### Main Loop Structure

```cpp
void loop() {
  static unsigned long lastHashtagCheck = 0;
  static unsigned long lastDailyCheck = 0;

  unsigned long now = millis();

  // Check #askdogberry every 60 seconds
  if (now - lastHashtagCheck >= 60000) {
    checkAndReplyToHashtag();
    lastHashtagCheck = now;
  }

  // Check daily post every 5 minutes (to catch the posting time)
  if (now - lastDailyCheck >= 300000) {
    checkAndPostDaily();
    lastDailyCheck = now;
  }

  // Other ESP32 tasks...
  delay(100);
}

void checkAndPostDaily() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  // Post at 9:00 AM UTC (adjust as needed)
  if (timeinfo.tm_hour == 9 && timeinfo.tm_min < 5) {
    if (!hasPostedToday()) {
      postDailyWit();
    }
  }
}
```

## Bluesky API Integration

### Required Endpoints

**Authentication**:
```
POST https://bsky.social/xrpc/com.atproto.server.createSession
Body: {"identifier": "dogberry.bsky.social", "password": "***"}
Response: {"accessJwt": "...", "did": "did:plc:..."}
```

**Search Hashtag**:
```
GET https://bsky.social/xrpc/app.bsky.feed.searchPosts?q=%23askdogberry&limit=1
Response: {posts: [{uri, cid, author, text, ...}]}
```

**Create Reply**:
```
POST https://bsky.social/xrpc/com.atproto.repo.createRecord
Body: {
  "repo": "did:plc:...",
  "collection": "app.bsky.feed.post",
  "record": {
    "text": "Dogberry's response...",
    "createdAt": "2024-10-24T...",
    "reply": {
      "root": {"uri": "...", "cid": "..."},
      "parent": {"uri": "...", "cid": "..."}
    }
  }
}
```

**Create Daily Post**:
```
POST https://bsky.social/xrpc/com.atproto.repo.createRecord
Body: {
  "repo": "did:plc:...",
  "collection": "app.bsky.feed.post",
  "record": {
    "text": "Daily wisdom...",
    "createdAt": "2024-10-24T..."
  }
}
```

## Rate Limiting

**Bluesky Rate Limits** (as of 2024):
- 5000 requests per day per account
- 300 requests per 5 minutes

**Bot Compliance**:
- Hashtag checks: 1440 requests/day (60 sec intervals)
- Daily post: 1 request/day
- Replies: Variable (depends on hashtag usage)
- **Total**: ~1500 requests/day (well under limit)

## Error Handling

**Failed to fetch hashtag posts**:
- Log error
- Retry next interval
- Don't mark as checked

**Failed to post reply**:
- Log error
- Don't mark post as replied (will retry)
- Check rate limits

**Failed daily post**:
- Log error
- Retry in next 5-minute window
- Max 3 retries before giving up for the day

## Testing Strategy

**Development Mode**:
```cpp
#define TESTING_MODE true
#define TEST_CHECK_INTERVAL 10000  // 10 seconds instead of 60
#define TEST_DAILY_HOUR 14         // Test daily post at 2 PM local
```

**Test Checklist**:
- ✓ Hashtag detection works
- ✓ Duplicate reply prevention works
- ✓ Daily post triggers at correct time
- ✓ Daily posts are unique across multiple days
- ✓ WiFi reconnection after disconnect
- ✓ Time sync after power cycle

## Future Enhancements

**Possible Features**:
1. **React to mentions** (without hashtag) with a coin flip reply
2. **Thread awareness** - don't reply twice in the same thread
3. **User memory** - remember users who frequently use #askdogberry
4. **Seasonal greetings** - special posts on holidays
5. **Analytics** - track engagement metrics
6. **Multiple hashtags** - respond to #constable or #shakespeare too

## Security Considerations

**Credentials Storage**:
- Store Bluesky password in `secrets.h` (not committed to git)
- Use environment variables or config file
- Never log credentials

**Input Validation**:
- Sanitize hashtag search results
- Limit response length (300 chars max)
- Filter inappropriate content in seed text

**Spam Prevention**:
- Track replied posts to avoid spam
- Limit daily replies (e.g., max 50/day)
- Implement cooldown between replies (2 minutes minimum)

---

**Implementation Status**: Planned
**Last Updated**: October 24, 2024
