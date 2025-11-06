#ifndef BLUESKY_API_H
#define BLUESKY_API_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

class BlueskyAPI {
public:
    BlueskyAPI(const char* handle, const char* appPassword);

    bool authenticate();
    bool checkMentions(String& mentionText, String& replyUri, String& replyCid);
    bool postReply(const String& text, const String& replyUri, const String& replyCid);
    bool postStatus(const String& text);

private:
    const char* handle;
    const char* appPassword;
    String accessJwt;
    String did;

    bool postHasReplies(const String& uri);

    bool makeRequest(const String& endpoint, const String& method,
                     const String& body, JsonDocument& response);
};

#endif
