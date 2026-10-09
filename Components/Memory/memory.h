#pragma once

#include <condition_variable>
#include <iostream>
#include <json.hpp>
#include <math.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

class NLPClient;

typedef struct {
    std::string subject;
    std::string relation;
    std::string object;
} fact;

typedef struct {
    std::string subject;
    float confidence;
} ex_record;

class state;

enum class AffectValence {
    NEUTRAL,
    POSITIVE,
    NEGATIVE
};

enum class AffectEnergy {
    LOW,
    MODERATE,
    HIGH
};

struct AffectiveState {
    AffectValence valence = AffectValence::NEUTRAL;
    AffectEnergy energy = AffectEnergy::MODERATE;
    float arousal = 0.5f;
    float engagement = 0.5f;
    float stress = 0.5f;
    float affinity = 0.5f;

    void updateFromFeedback(float reward, bool positive_feedback, bool explicit_negative);
};

enum class MemorySource {
    USER_STATEMENT,
    OBSERVATION,
    ACTION_HISTORY,
    INFERENCE
};

enum class MemoryScope {
    GENERAL,
    USER_SPECIFIC
};

struct MemoryRecord {
    fact raw_fact;
    float confidence = 0.0f;
    float generalization_factor = 0.0f;
    MemoryScope scope = MemoryScope::GENERAL;
    std::vector<std::string> relevant_users;
    MemorySource source = MemorySource::OBSERVATION;
    int observation_count = 0;
    std::string last_updated;
    float relevance = 0.0f;

    void update_record();
};

struct record {
    fact raw_fact;
    float confidence = 0.0f;
    float generalization_factor = 0.0f;
    enum user_context { GENERAL, USER_SPECIFIC };
    user_context current_context = GENERAL;
    std::vector<std::string> relevant_users;
    enum source_type { USER_STATEMENT, OBSERVATION, ACTION_HISTORY, INFERENCE };
    source_type source = OBSERVATION;
    int observation_count = 0;
    std::string last_updated;
    float relevance = 0.0f;

    record();
    void update_record();
};

class action_log {
public:
    std::string action_taken;
    std::string time;
    float count = 0.0f;

    action_log();
};

class cache {
public:
    std::vector<ex_record> data_store;
};

class memory_loader {
public:
    nlohmann::json read_data(std::string path);
    bool save_data(std::string path);
};

class NLPClient {
private:
    std::thread worker;
    std::queue<std::string> requestQueue;
    std::queue<nlohmann::json> responseQueue;
    std::mutex requestMutex;
    std::mutex responseMutex;
    std::condition_variable requestCV;
    std::condition_variable responseCV;
    bool running = false;

    void workerLoop();
    void sendRequest(const std::string &text);
    nlohmann::json receiveResponse();

public:
    NLPClient();
    ~NLPClient();
    void start();
    void submitText(const std::string &text);
    nlohmann::json getResult();
    void stop();
};

class input_parser {};

class memory_parser {
public:
    std::vector<record> get_relevant_records(std::string input, state State);
    std::vector<ex_record> extract_data_from_records(std::vector<record>, NLPClient);
};

class memory_component {
public:
    memory_parser parser;
    memory_loader loader;
    cache active_mem;
    std::vector<action_log> logs;
    std::vector<record> records;
    AffectiveState affect;

    void load_memory();
    void update_memory();
    void store_context();
};
