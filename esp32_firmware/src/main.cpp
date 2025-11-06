#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "secrets.h"
#include "BlueskyAPI.h"
#include "DogberryAI_Word.h"

BlueskyAPI* bluesky = nullptr;
DogberryAI_Word* ai = nullptr;

unsigned long lastCheckTime = 0;
const unsigned long CHECK_INTERVAL = 60000; // 60 seconds

int lastPostDay = -1;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0; // UTC
const int daylightOffset_sec = 0;

// Daily post seed phrases - AI will generate from these
const char* dailySeeds[] = {
    "much ado about",
    "i say unto thee",
    "marry good people",
    "what ho my friends",
    "by my troth i",
    "verily i tell you",
    "forsooth the world is",
    "mark my words for",
    "thou shouldst know that",
    "wisdom tells us that"
};
const int numSeeds = 10;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=== Dogberry Bot Starting ===");

    // Connect to WiFi
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Sync time with NTP
    Serial.println("Syncing time with NTP...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
    } else {
        Serial.print("Time synced: ");
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }

    // Initialize AI model
    ai = new DogberryAI_Word();
    if (!ai->initialize()) {
        Serial.println("ERROR: Failed to initialize AI model");
        while (1) delay(1000);
    }

    // Initialize Bluesky API
    bluesky = new BlueskyAPI(BLUESKY_HANDLE, BLUESKY_APP_PASSWORD);
    if (!bluesky->authenticate()) {
        Serial.println("ERROR: Failed to authenticate with Bluesky");
        while (1) delay(1000);
    }

    Serial.println("=== Dogberry Bot Ready ===\n");
}

void loop() {
    unsigned long currentTime = millis();

    // Check current time for daily post
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        // Check if it's 20:00 UTC and we haven't posted today
        if (timeinfo.tm_hour == 20 && timeinfo.tm_min == 0 &&
            timeinfo.tm_mday != lastPostDay) {

            Serial.println("TIME FOR DAILY POST!");
            lastPostDay = timeinfo.tm_mday;

            // Pick a random seed phrase and generate AI content
            int seedIndex = random(0, numSeeds);
            String seed = String(dailySeeds[seedIndex]);

            Serial.print("Daily post seed: ");
            Serial.println(seed);

            // Generate AI response (30-40 words for a good daily quote)
            String dailyPost = ai->generateResponse(seed, 35);

            Serial.print("Daily post generated: ");
            Serial.println(dailyPost);

            // Post the AI-generated daily status
            if (bluesky->postStatus(dailyPost)) {
                Serial.println("Daily post published!\n");
            } else {
                Serial.println("Failed to publish daily post\n");
            }
        }
    }

    // Check for mentions every 60 seconds
    if (currentTime - lastCheckTime >= CHECK_INTERVAL) {
        lastCheckTime = currentTime;

        Serial.println("Checking for mentions...");

        String mentionText, replyUri, replyCid;
        if (bluesky->checkMentions(mentionText, replyUri, replyCid)) {
            Serial.println("Processing mention...");

            // Extract user's message (remove @dogberry mention)
            String cleanedMention = mentionText;
            cleanedMention.replace("@dogberry", "");
            cleanedMention.replace("@Dogberry", "");
            cleanedMention.replace("@constabledogberry", "");
            cleanedMention.trim();
            cleanedMention.toLowerCase();

            // Create contextual seed
            String seed = "";

            // Detect question patterns
            if (cleanedMention.indexOf("?") != -1 ||
                cleanedMention.startsWith("what") ||
                cleanedMention.startsWith("who") ||
                cleanedMention.startsWith("why") ||
                cleanedMention.startsWith("how") ||
                cleanedMention.startsWith("when") ||
                cleanedMention.startsWith("where")) {
                seed = "i think that";
            }
            // Detect greetings
            else if (cleanedMention.indexOf("hello") != -1 ||
                     cleanedMention.indexOf("hi ") != -1 ||
                     cleanedMention.indexOf("hey") != -1 ||
                     cleanedMention.indexOf("greetings") != -1) {
                seed = "good morrow to thee";
            }
            // Detect requests for help
            else if (cleanedMention.indexOf("help") != -1) {
                seed = "i shall assist thee";
            }
            // Detect insults or negative sentiment
            else if (cleanedMention.indexOf("fool") != -1 ||
                     cleanedMention.indexOf("stupid") != -1 ||
                     cleanedMention.indexOf("idiot") != -1 ||
                     cleanedMention.indexOf("villain") != -1) {
                seed = "thou art a";
            }
            // Default to general response
            else {
                seed = "marry i say";
            }

            Serial.print("Using seed: ");
            Serial.println(seed);

            String response = ai->generateResponse(seed, 40);

            Serial.print("Generated: ");
            Serial.println(response);

            if (bluesky->postReply(response, replyUri, replyCid)) {
                Serial.println("Reply posted!\n");
            } else {
                Serial.println("Failed to post reply\n");
            }
        }
    }

    delay(100);
}
