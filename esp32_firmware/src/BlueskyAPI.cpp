#include "BlueskyAPI.h"

BlueskyAPI::BlueskyAPI(const char* handle, const char* appPassword)
    : handle(handle), appPassword(appPassword) {}

bool BlueskyAPI::authenticate() {
    Serial.println("Authenticating with Bluesky...");

    JsonDocument doc;
    doc["identifier"] = handle;
    doc["password"] = appPassword;

    String body;
    serializeJson(doc, body);

    JsonDocument response;
    if (!makeRequest("https://bsky.social/xrpc/com.atproto.server.createSession",
                     "POST", body, response)) {
        Serial.println("Authentication failed");
        return false;
    }

    accessJwt = response["accessJwt"].as<String>();
    did = response["did"].as<String>();

    Serial.println("Authenticated successfully");
    return true;
}

bool BlueskyAPI::postHasReplies(const String& uri) {
    // Extract AT URI components: at://did/collection/rkey
    String endpoint = "https://bsky.social/xrpc/app.bsky.feed.getPostThread?uri=" + uri;

    JsonDocument response;
    if (!makeRequest(endpoint, "GET", "", response)) {
        Serial.println("Failed to fetch thread");
        return false;  // Assume no replies if we can't fetch
    }

    // Check if thread has replies
    JsonObject thread = response["thread"];
    if (!thread.isNull()) {
        JsonArray replies = thread["replies"];
        if (!replies.isNull() && replies.size() > 0) {
            return true;  // Post has replies
        }
    }

    return false;  // No replies found
}

bool BlueskyAPI::checkMentions(String& mentionText, String& replyUri, String& replyCid) {
    if (accessJwt.isEmpty()) {
        Serial.println("Not authenticated");
        return false;
    }

    // Fetch only the 3 most recent mentions
    String endpoint = "https://bsky.social/xrpc/app.bsky.notification.listNotifications?limit=3";

    JsonDocument response;
    if (!makeRequest(endpoint, "GET", "", response)) {
        return false;
    }

    JsonArray notifications = response["notifications"];

    // Iterate through mentions (newest first) and find the first one without replies
    for (JsonObject notification : notifications) {
        String reason = notification["reason"].as<String>();

        // Skip if not a mention
        if (reason != "mention") {
            continue;
        }

        String uri = notification["uri"].as<String>();

        // Check if this post already has replies
        if (postHasReplies(uri)) {
            Serial.print("Skipping mention (already has replies): ");
            Serial.println(uri);
            continue;
        }

        // Found a mention without replies - this is the one to respond to
        JsonObject record = notification["record"];
        mentionText = record["text"].as<String>();
        replyUri = uri;
        replyCid = notification["cid"].as<String>();

        Serial.print("Found new mention without replies: ");
        Serial.println(mentionText);
        return true;
    }

    return false;  // No unreplied mentions found
}

bool BlueskyAPI::postReply(const String& text, const String& replyUri, const String& replyCid) {
    if (accessJwt.isEmpty()) {
        Serial.println("Not authenticated");
        return false;
    }

    JsonDocument doc;
    doc["repo"] = did;
    doc["collection"] = "app.bsky.feed.post";

    JsonObject record = doc["record"].to<JsonObject>();
    record["text"] = text;
    record["createdAt"] = "2024-01-01T00:00:00Z";

    JsonObject reply = record["reply"].to<JsonObject>();
    JsonObject parent = reply["parent"].to<JsonObject>();
    parent["uri"] = replyUri;
    parent["cid"] = replyCid;

    JsonObject root = reply["root"].to<JsonObject>();
    root["uri"] = replyUri;
    root["cid"] = replyCid;

    String body;
    serializeJson(doc, body);

    JsonDocument response;
    if (!makeRequest("https://bsky.social/xrpc/com.atproto.repo.createRecord",
                     "POST", body, response)) {
        Serial.println("Failed to post reply");
        return false;
    }

    Serial.println("Reply posted successfully");
    return true;
}

bool BlueskyAPI::postStatus(const String& text) {
    if (accessJwt.isEmpty()) {
        Serial.println("Not authenticated");
        return false;
    }

    JsonDocument doc;
    doc["repo"] = did;
    doc["collection"] = "app.bsky.feed.post";

    JsonObject record = doc["record"].to<JsonObject>();
    record["text"] = text;
    record["createdAt"] = "2024-01-01T00:00:00Z";

    String body;
    serializeJson(doc, body);

    JsonDocument response;
    if (!makeRequest("https://bsky.social/xrpc/com.atproto.repo.createRecord",
                     "POST", body, response)) {
        Serial.println("Failed to post status");
        return false;
    }

    Serial.println("Status posted successfully");
    return true;
}

bool BlueskyAPI::makeRequest(const String& endpoint, const String& method,
                              const String& body, JsonDocument& response) {
    HTTPClient http;
    http.begin(endpoint);
    http.addHeader("Content-Type", "application/json");

    if (!accessJwt.isEmpty()) {
        http.addHeader("Authorization", "Bearer " + accessJwt);
    }

    int httpCode;
    if (method == "POST") {
        httpCode = http.POST(body);
    } else {
        httpCode = http.GET();
    }

    if (httpCode != 200) {
        Serial.print("HTTP error: ");
        Serial.println(httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    DeserializationError error = deserializeJson(response, payload);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        http.end();
        return false;
    }

    http.end();
    return true;
}
