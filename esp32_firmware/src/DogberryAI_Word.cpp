#include "DogberryAI_Word.h"
#include "model_weights_word.h"
#include "vocab_data_word.h"
#include <cmath>
#include <cstring>

DogberryAI_Word::DogberryAI_Word() {
    embedding_output = nullptr;
    lstm_h = nullptr;
    lstm_c = nullptr;
    lstm_output = nullptr;
    logits = nullptr;
    probs = nullptr;
    lstm_gates = nullptr;
}

DogberryAI_Word::~DogberryAI_Word() {
    if (embedding_output) free(embedding_output);
    if (lstm_h) free(lstm_h);
    if (lstm_c) free(lstm_c);
    if (lstm_output) free(lstm_output);
    if (logits) free(logits);
    if (probs) free(probs);
    if (lstm_gates) free(lstm_gates);
}

bool DogberryAI_Word::initialize() {
    Serial.println("Initializing DogberryAI (Word-level)...");

    // Allocate buffers in PSRAM
    embedding_output = (float*)ps_malloc(EMBEDDING_DIM * sizeof(float));
    lstm_h = (float*)ps_malloc(LSTM_UNITS * sizeof(float));
    lstm_c = (float*)ps_malloc(LSTM_UNITS * sizeof(float));
    lstm_output = (float*)ps_malloc(LSTM_UNITS * sizeof(float));
    logits = (float*)ps_malloc(VOCAB_SIZE * sizeof(float));
    probs = (float*)ps_malloc(VOCAB_SIZE * sizeof(float));
    lstm_gates = (float*)ps_malloc(LSTM_UNITS * 4 * sizeof(float));

    if (!embedding_output || !lstm_h || !lstm_c || !lstm_output || !logits || !probs || !lstm_gates) {
        Serial.println("Failed to allocate model buffers");
        return false;
    }

    // Initialize LSTM state to zero
    memset(lstm_h, 0, LSTM_UNITS * sizeof(float));
    memset(lstm_c, 0, LSTM_UNITS * sizeof(float));

    Serial.println("DogberryAI initialized successfully");
    return true;
}

String DogberryAI_Word::generateResponse(const String& seedText, int maxWords) {
    Serial.println("Generating response...");

    // Tokenize seed text
    int seed_tokens[SEQ_LENGTH];
    int seed_len = 0;

    // Split by spaces
    String word = "";
    for (unsigned int i = 0; i < seedText.length() && seed_len < SEQ_LENGTH; i++) {
        char c = seedText.charAt(i);
        if (c == ' ' || c == '\n' || c == '\t') {
            if (word.length() > 0) {
                seed_tokens[seed_len++] = tokenizeWord(word);
                word = "";
            }
        } else {
            word += c;
        }
    }
    if (word.length() > 0 && seed_len < SEQ_LENGTH) {
        seed_tokens[seed_len++] = tokenizeWord(word);
    }

    // Pad with zeros if needed
    for (int i = seed_len; i < SEQ_LENGTH; i++) {
        seed_tokens[i] = 0;
    }

    // Reset LSTM state
    memset(lstm_h, 0, LSTM_UNITS * sizeof(float));
    memset(lstm_c, 0, LSTM_UNITS * sizeof(float));

    // Process seed sequence
    for (int i = 0; i < seed_len; i++) {
        embedding(seed_tokens[i], embedding_output);
        lstm_step(embedding_output, lstm_h, lstm_c, lstm_output);
    }

    // Generate new words
    String response = "";
    for (int i = 0; i < maxWords; i++) {
        dense(lstm_output, logits);
        int next_word_idx = sample(logits, 0.8);

        String next_word = detokenizeWord(next_word_idx);

        // Stop on special tokens or period
        if (next_word == "<PAD>" || next_word == "<UNK>" || next_word == "<START>") {
            continue;
        }
        if (next_word == ".") {
            response += ".";
            break;
        }

        if (response.length() > 0) {
            response += " ";
        }
        response += next_word;

        // Continue LSTM
        embedding(next_word_idx, embedding_output);
        lstm_step(embedding_output, lstm_h, lstm_c, lstm_output);
    }

    cleanResponse(response);
    return response;
}

int DogberryAI_Word::tokenizeWord(const String& word) {
    String lower = word;
    lower.toLowerCase();

    for (int i = 0; i < VOCAB_SIZE; i++) {
        if (lower == String(VOCAB_WORDS[i])) {
            return i;
        }
    }
    return 1; // <UNK>
}

String DogberryAI_Word::detokenizeWord(int idx) {
    if (idx < 0 || idx >= VOCAB_SIZE) {
        return "<UNK>";
    }
    return String(VOCAB_WORDS[idx]);
}

void DogberryAI_Word::embedding(int word_idx, float* output) {
    if (word_idx < 0 || word_idx >= VOCAB_SIZE) {
        memset(output, 0, EMBEDDING_DIM * sizeof(float));
        return;
    }

    int offset = word_idx * EMBEDDING_DIM;
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        output[i] = pgm_read_float(&EMBEDDING_WEIGHTS[offset + i]);
    }
}

void DogberryAI_Word::lstm_step(const float* input, float* h, float* c, float* output) {
    // Use pre-allocated buffer instead of stack array
    float* gates = lstm_gates;

    // Compute input transformation: Wx
    for (int i = 0; i < LSTM_UNITS * 4; i++) {
        float sum = pgm_read_float(&LSTM_BIAS[i]);
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            sum += input[j] * pgm_read_float(&LSTM_KERNEL[j * LSTM_UNITS * 4 + i]);
        }
        gates[i] = sum;
    }

    // Add recurrent transformation: Uh
    for (int i = 0; i < LSTM_UNITS * 4; i++) {
        for (int j = 0; j < LSTM_UNITS; j++) {
            gates[i] += h[j] * pgm_read_float(&LSTM_RECURRENT[j * LSTM_UNITS * 4 + i]);
        }
    }

    // Apply activations and update cell state
    for (int i = 0; i < LSTM_UNITS; i++) {
        float i_gate = 1.0f / (1.0f + expf(-gates[i]));                    // input gate (sigmoid)
        float f_gate = 1.0f / (1.0f + expf(-gates[LSTM_UNITS + i]));       // forget gate (sigmoid)
        float c_gate = tanhf(gates[LSTM_UNITS * 2 + i]);                   // cell gate (tanh)
        float o_gate = 1.0f / (1.0f + expf(-gates[LSTM_UNITS * 3 + i]));   // output gate (sigmoid)

        c[i] = f_gate * c[i] + i_gate * c_gate;
        h[i] = o_gate * tanhf(c[i]);
    }

    memcpy(output, h, LSTM_UNITS * sizeof(float));
}

void DogberryAI_Word::dense(const float* input, float* output) {
    for (int i = 0; i < VOCAB_SIZE; i++) {
        float sum = pgm_read_float(&DENSE_BIAS[i]);
        for (int j = 0; j < LSTM_UNITS; j++) {
            sum += input[j] * pgm_read_float(&DENSE_KERNEL[j * VOCAB_SIZE + i]);
        }
        output[i] = sum;
    }
}

int DogberryAI_Word::sample(const float* logits, float temperature) {
    // Find max for numerical stability
    float max_logit = logits[0];
    for (int i = 1; i < VOCAB_SIZE; i++) {
        if (logits[i] > max_logit) {
            max_logit = logits[i];
        }
    }

    // Compute exp(logit / temperature) and sum
    // Use pre-allocated probs buffer instead of stack array
    float sum = 0.0f;
    for (int i = 0; i < VOCAB_SIZE; i++) {
        probs[i] = expf((logits[i] - max_logit) / temperature);
        sum += probs[i];
    }

    // Normalize
    for (int i = 0; i < VOCAB_SIZE; i++) {
        probs[i] /= sum;
    }

    // Sample from distribution
    float r = (float)random(0, 10000) / 10000.0f;
    float cumulative = 0.0f;
    for (int i = 0; i < VOCAB_SIZE; i++) {
        cumulative += probs[i];
        if (r <= cumulative) {
            return i;
        }
    }

    return VOCAB_SIZE - 1;
}

void DogberryAI_Word::cleanResponse(String& response) {
    // Remove any leading/trailing whitespace
    response.trim();

    // Remove the seed prompt if it appears at the start
    // (This happens when the model echoes back the input)
    int firstSpace = response.indexOf(' ');
    if (firstSpace > 0) {
        String firstWord = response.substring(0, firstSpace);
        firstWord.toLowerCase();
        if (firstWord == "dogberry" || firstWord == "hey" || firstWord == "hello") {
            response = response.substring(firstSpace + 1);
            response.trim();
        }
    }

    // Capitalize first letter
    if (response.length() > 0) {
        response.setCharAt(0, toupper(response.charAt(0)));
    }
}
