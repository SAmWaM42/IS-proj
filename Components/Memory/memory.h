#pragma once

#include <condition_variable>
#include <iostream>
#include <json.hpp>
#include <math.h>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

enum class Level;

class state;

class NLPClient;

typedef struct
{
    std::string subject;
    std::string relation;
    std::string object;
} fact;

typedef struct
{
    std::string subject;
    float confidence;
} ex_record;

enum class AffectValence
{
    NEUTRAL,
    POSITIVE,
    NEGATIVE
};

enum class AffectEnergy
{
    LOW,
    MODERATE,
    HIGH
};

struct AffectiveState
{
    AffectValence valence = AffectValence::NEUTRAL;
    AffectEnergy energy = AffectEnergy::MODERATE;
    float arousal = 0.5f;
    float engagement = 0.5f;
    float stress = 0.5f;
    float affinity = 0.5f;

    void updateFromFeedback(float reward, bool positive_feedback, bool explicit_negative);
    // Advance transient affect toward resting baselines; energy is environment-owned.
    void decay(float elapsedSeconds);
};

enum class MemorySource
{
    USER_STATEMENT,
    OBSERVATION,
    ACTION_HISTORY,
    INFERENCE
};

enum class MemoryScope
{
    GENERAL,
    USER_SPECIFIC
};

struct MemoryRecord
{
    fact raw_fact;
    float confidence = 0.0f;
    float generalization_factor = 0.0f;
    MemoryScope scope = MemoryScope::GENERAL;
    std::vector<std::string> relevant_users;
    MemorySource source = MemorySource::OBSERVATION;
    int observation_count = 0;
    std::string last_updated;
    float relevance = 0.0f;
    float recency = 0.0f;

    void update_record();
};

struct record
{
    fact raw_fact;
    float confidence = 0.0f;
    float generalization_factor = 0.0f;
    enum user_context
    {
        GENERAL,
        USER_SPECIFIC
    };
    user_context current_context = GENERAL;
    std::vector<std::string> relevant_users;
    enum source_type
    {
        USER_STATEMENT,
        OBSERVATION,
        ACTION_HISTORY,
        INFERENCE
    };
    source_type source = OBSERVATION;
    int observation_count = 0;
    std::string last_updated;
    float relevance = 0.0f;
    float recency = 0.0f;

    record();
    void update_record();
};

struct SessionContext
{
    int current_intent = 0;
    int target_proximity = 0;
    int user_status = 0;
    int energy_level = 0;
    int stress_level = 0;
    std::string current_action;
    std::string last_timestamp;
    float last_reward = 0.0f;
};


struct InteractionEvent
{
    std::string timestamp;
    std::string action_taken;
    std::string summary;
    float reward = 0.0f;
    std::string state_summary;
    float affect_signal = 0.0f;
};

struct MemorySummary
{
    float memory_bias = 0.0f;
    float signal_quality = 0.0f;
    float recency_weight = 0.0f;
    float confidence_weight = 0.0f;
    int record_count = 0;
    std::vector<std::string> top_tags;
};

struct NormalisedObservation
{
    int user_status = 0;
    int target_proximity = 0;
    int task_state = 0;
    int interaction_intent = 0;
    float memory_bias = 0.0f;
    float affect_trend = 0.0f;
};

struct RuntimeContext
{
    InteractionEvent latest_event;
    SessionContext session_context;
    AffectiveState affective_state;
    MemorySummary memory_summary;
    std::vector<std::string> recent_actions;
    bool rest_state = false;
};

struct EnvironmentObservation
{
    int goal_state = 0;
    int target_proximity = 0;
    int user_status = 0;
    int task_status = 0;
    float signal_strength = 0.0f;
    // Explicit observable affordances used by the shared action mask.
    bool food_available = false;
    bool play_available = false;
};

struct ActionResult
{
    int action_id = 0;
    float reward = 0.0f;
    bool terminal = false;
    std::string outcome_tag;
    // Environment-owned energy level after this transition (0..1). Negative means unspecified.
    // Baseline ignores this field; the proposed agent uses it as an affective-state input.
    float energy_after = -1.0f;
};

float summarizeMemorySignal(const std::vector<record> &records);
float calculateMemoryRecordWeight(const record &entry);
float calculateRecordSignal(const record &entry);
std::vector<record> rankRelevantRecords(const std::vector<record> &entries, const std::string &query);
float summarizeWeightedMemorySignal(const std::vector<record> &entries);
MemorySummary buildMemorySummary(const std::vector<record> &records, const std::string &query = "");
NormalisedObservation normaliseRuntimeObservation(const RuntimeContext &runtime);
bool isRestState(const SessionContext &context, float memory_signal = 0.0f, float affect_trend = 0.0f);
int summarizeAffectTrend(const AffectiveState &affect, Level energy);
bool saveMemoryRecords(const std::string &path, const std::vector<record> &records);
std::vector<record> loadMemoryRecords(const std::string &path);
bool saveAffectiveState(const std::string &path, const AffectiveState &affect);
AffectiveState loadAffectiveState(const std::string &path);
bool saveSessionContext(const std::string &path, const SessionContext &context);
SessionContext loadSessionContext(const std::string &path);
bool saveInteractionHistory(const std::string &path, const std::vector<InteractionEvent> &history);
std::vector<InteractionEvent> loadInteractionHistory(const std::string &path);
bool saveAggregateFacts(const std::string &path, const std::vector<record> &facts);
std::vector<record> loadAggregateFacts(const std::string &path);
std::vector<record> aggregateRestStateFacts(const std::vector<InteractionEvent> &history,  const std::vector<record> &existingFacts);

class action_log
{
public:
    std::string action_taken;
    std::string time;
    float count = 0.0f;

    action_log();
};

class cache
{
public:
    std::vector<ex_record> data_store;
};

class memory_loader
{
public:
    nlohmann::json read_data(std::string path);
    bool save_data(std::string path);
};

class NLPClient
{
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

class input_parser
{
};

class memory_parser
{
public:
    std::vector<record> get_relevant_records(std::string input, const state &stateRef);
    std::vector<ex_record> extract_data_from_records(std::vector<record>, NLPClient);
};

class memory_component
{
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
