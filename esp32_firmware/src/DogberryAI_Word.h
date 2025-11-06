#ifndef DOGBERRYAI_WORD_H
#define DOGBERRYAI_WORD_H

#include <Arduino.h>

// Model architecture
#define VOCAB_SIZE 4000
#define SEQ_LENGTH 40
#define EMBEDDING_DIM 64
#define LSTM_UNITS 256

class DogberryAI_Word {
public:
    DogberryAI_Word();
    ~DogberryAI_Word();

    bool initialize();
    String generateResponse(const String& seedText, int maxWords = 40);

private:
    // Pre-allocated buffers (in PSRAM)
    float* embedding_output;
    float* lstm_h;
    float* lstm_c;
    float* lstm_output;
    float* logits;
    float* probs;  // Probability distribution buffer
    float* lstm_gates;  // Buffer for LSTM gate computations

    // Helper functions
    int tokenizeWord(const String& word);
    String detokenizeWord(int idx);
    void embedding(int word_idx, float* output);
    void lstm_step(const float* input, float* h, float* c, float* output);
    void dense(const float* input, float* output);
    int sample(const float* logits, float temperature);
    void cleanResponse(String& response);
};

#endif
