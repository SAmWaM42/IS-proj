#include "memory.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

void AffectiveState::updateFromFeedback(float reward, bool positive_feedback, bool explicit_negative) {
    if (explicit_negative) {
        valence = AffectValence::NEGATIVE;
        stress = std::min(1.0f, stress + 0.35f);
        engagement = std::max(0.0f, engagement - 0.25f);
        affinity = std::max(0.0f, affinity - 0.30f);
        return;
    }

    if (positive_feedback) {
        valence = AffectValence::POSITIVE;
        engagement = std::min(1.0f, engagement + 0.25f);
        affinity = std::min(1.0f, affinity + 0.20f);
    } else {
        valence = AffectValence::NEUTRAL;
    }

    arousal = std::clamp(arousal + reward * 0.1f, 0.0f, 1.0f);
    energy = reward > 0.0f ? AffectEnergy::HIGH : AffectEnergy::MODERATE;
}

void MemoryRecord::update_record() {
    observation_count += 1;
    confidence = std::clamp(confidence + 0.1f, 0.0f, 1.0f);
    relevance = std::clamp(confidence * (generalization_factor + 1.0f), 0.0f, 1.0f);
    recency = std::clamp(recency + 0.1f, 0.0f, 1.0f);
}

record::record() = default;

void record::update_record() {
    observation_count += 1;
    confidence = std::clamp(confidence + 0.1f, 0.0f, 1.0f);
    relevance = std::clamp(confidence * (generalization_factor + 1.0f), 0.0f, 1.0f);
    recency = std::clamp(recency + 0.1f, 0.0f, 1.0f);
}

float calculateMemoryRecordWeight(const record& entry) {
    const float scopeBoost = entry.current_context == record::USER_SPECIFIC ? 0.10f : 0.0f;
    const float normalizedWeight =
        0.45f * std::clamp(entry.confidence, 0.0f, 1.0f) +
        0.30f * std::clamp(entry.relevance, 0.0f, 1.0f) +
        0.15f * std::clamp(entry.recency, 0.0f, 1.0f) +
        0.10f * scopeBoost;

    return std::clamp(normalizedWeight, 0.0f, 1.0f);
}

float calculateRecordSignal(const record& entry) {
    return std::clamp(
        0.5f * std::clamp(entry.confidence, 0.0f, 1.0f) +
            0.5f * std::clamp(entry.relevance, 0.0f, 1.0f),
        0.0f,
        1.0f);
}

std::vector<record> rankRelevantRecords(const std::vector<record>& entries, const std::string& query) {
    std::vector<std::pair<float, record>> ranked;
    ranked.reserve(entries.size());

    for (const auto& entry : entries) {
        float score = calculateMemoryRecordWeight(entry);

        if (!query.empty()) {
            const std::string combined =
                entry.raw_fact.subject + " " + entry.raw_fact.relation + " " + entry.raw_fact.object + " " + query;
            const std::string lower = combined;
            const std::string queryLower = query;
            if (lower.find(queryLower) != std::string::npos) {
                score += 0.15f;
            }
        }

        ranked.emplace_back(score, entry);
    }

    std::sort(ranked.begin(), ranked.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first > rhs.first;
    });

    std::vector<record> ordered;
    ordered.reserve(ranked.size());
    for (const auto& item : ranked) {
        ordered.push_back(item.second);
    }

    return ordered;
}

float summarizeWeightedMemorySignal(const std::vector<record>& entries) {
    if (entries.empty()) {
        return 0.0f;
    }

    float totalWeight = 0.0f;
    float totalSignal = 0.0f;

    for (const auto& entry : rankRelevantRecords(entries, "")) {
        const float weight = calculateMemoryRecordWeight(entry);
        totalWeight += weight;
        totalSignal += weight * calculateRecordSignal(entry);
    }

    if (totalWeight <= 0.0f) {
        return 0.0f;
    }

    return std::clamp(totalSignal / totalWeight, 0.0f, 1.0f);
}

float summarizeMemorySignal(const std::vector<record>& records) {
    return summarizeWeightedMemorySignal(records);
}

MemorySummary buildMemorySummary(const std::vector<record>& records, const std::string& query) {
    MemorySummary summary;
    const std::vector<record> ranked = rankRelevantRecords(records, query);
    if (ranked.empty()) {
        return summary;
    }

    summary.record_count = static_cast<int>(ranked.size());
    float totalWeight = 0.0f;
    float totalConfidence = 0.0f;
    float totalRecency = 0.0f;
    float totalSignal = 0.0f;

    for (const auto& entry : ranked) {
        const float weight = calculateMemoryRecordWeight(entry);
        totalWeight += weight;
        totalConfidence += weight * std::clamp(entry.confidence, 0.0f, 1.0f);
        totalRecency += weight * std::clamp(entry.recency, 0.0f, 1.0f);
        totalSignal += weight * calculateRecordSignal(entry);

        if (summary.top_tags.size() < 3) {
            const std::string tag = entry.raw_fact.subject + ":" + entry.raw_fact.relation + ":" + entry.raw_fact.object;
            summary.top_tags.push_back(tag);
        }
    }

    if (totalWeight > 0.0f) {
        summary.memory_bias = std::clamp(totalSignal / totalWeight, 0.0f, 1.0f);
        summary.confidence_weight = std::clamp(totalConfidence / totalWeight, 0.0f, 1.0f);
        summary.recency_weight = std::clamp(totalRecency / totalWeight, 0.0f, 1.0f);
        summary.signal_quality = summary.memory_bias;
    }

    return summary;
}

NormalisedObservation normaliseRuntimeObservation(const RuntimeContext& runtime) {
    NormalisedObservation observation;
    observation.user_status = std::clamp(runtime.session_context.user_status, 0, 3);
    observation.target_proximity = std::clamp(runtime.session_context.target_proximity, 0, 3);
    observation.interaction_intent = std::clamp(runtime.session_context.current_intent, 0, 3);
    observation.task_state = std::clamp(runtime.session_context.energy_level + runtime.session_context.stress_level, 0, 3);
    observation.memory_bias = std::clamp(runtime.memory_summary.memory_bias, 0.0f, 1.0f);
    observation.affect_trend = std::clamp(
        static_cast<float>(summarizeAffectTrend(runtime.affective_state, static_cast<Level>(std::clamp(runtime.session_context.energy_level, 0, 2)))),
        0.0f,
        2.0f);
    return observation;
}

bool isRestState(const SessionContext& context, float memory_signal, float affect_trend) {
    const bool idleRequest = context.current_action.empty() || context.current_action == "IDLE";
    const bool lowSignal = memory_signal <= 0.15f;
    const bool lowAffect = affect_trend <= 0.5f;
    const bool lowEnergy = context.energy_level <= 0;
    return idleRequest && (lowSignal || lowAffect || lowEnergy);
}

namespace {

nlohmann::json recordToJson(const record& entry) {
    nlohmann::json item = nlohmann::json::object();
    item["subject"] = entry.raw_fact.subject;
    item["relation"] = entry.raw_fact.relation;
    item["object"] = entry.raw_fact.object;
    item["confidence"] = entry.confidence;
    item["generalization_factor"] = entry.generalization_factor;
    item["current_context"] = entry.current_context == record::USER_SPECIFIC ? "USER_SPECIFIC" : "GENERAL";
    item["source"] = static_cast<int>(entry.source);
    item["observation_count"] = entry.observation_count;
    item["last_updated"] = entry.last_updated;
    item["relevance"] = entry.relevance;
    item["recency"] = entry.recency;
    item["relevant_users"] = entry.relevant_users;
    return item;
}

record jsonToRecord(const nlohmann::json& item) {
    record entry;
    entry.raw_fact.subject = item.value("subject", "");
    entry.raw_fact.relation = item.value("relation", "");
    entry.raw_fact.object = item.value("object", "");
    entry.confidence = item.value("confidence", 0.0f);
    entry.generalization_factor = item.value("generalization_factor", 0.0f);
    const std::string context = item.value("current_context", "GENERAL");
    entry.current_context = (context == "USER_SPECIFIC") ? record::USER_SPECIFIC : record::GENERAL;
    entry.source = static_cast<record::source_type>(item.value("source", static_cast<int>(record::OBSERVATION)));
    entry.observation_count = item.value("observation_count", 0);
    entry.last_updated = item.value("last_updated", "");
    entry.relevance = item.value("relevance", 0.0f);
    entry.recency = item.value("recency", 0.0f);
    if (item.contains("relevant_users") && item["relevant_users"].is_array()) {
        entry.relevant_users = item["relevant_users"].get<std::vector<std::string>>();
    }
    return entry;
}

nlohmann::json affectToJson(const AffectiveState& affect) {
    nlohmann::json json = nlohmann::json::object();
    json["valence"] = static_cast<int>(affect.valence);
    json["energy"] = static_cast<int>(affect.energy);
    json["arousal"] = affect.arousal;
    json["engagement"] = affect.engagement;
    json["stress"] = affect.stress;
    json["affinity"] = affect.affinity;
    return json;
}

AffectiveState jsonToAffect(const nlohmann::json& json) {
    AffectiveState affect;
    affect.valence = static_cast<AffectValence>(json.value("valence", static_cast<int>(AffectValence::NEUTRAL)));
    affect.energy = static_cast<AffectEnergy>(json.value("energy", static_cast<int>(AffectEnergy::MODERATE)));
    affect.arousal = json.value("arousal", 0.5f);
    affect.engagement = json.value("engagement", 0.5f);
    affect.stress = json.value("stress", 0.5f);
    affect.affinity = json.value("affinity", 0.5f);
    return affect;
}

nlohmann::json sessionToJson(const SessionContext& context) {
    nlohmann::json json = nlohmann::json::object();
    json["current_intent"] = context.current_intent;
    json["target_proximity"] = context.target_proximity;
    json["user_status"] = context.user_status;
    json["energy_level"] = context.energy_level;
    json["stress_level"] = context.stress_level;
    json["current_action"] = context.current_action;
    json["last_timestamp"] = context.last_timestamp;
    json["last_reward"] = context.last_reward;
    return json;
}

SessionContext jsonToSession(const nlohmann::json& json) {
    SessionContext context;
    context.current_intent = json.value("current_intent", 0);
    context.target_proximity = json.value("target_proximity", 0);
    context.user_status = json.value("user_status", 0);
    context.energy_level = json.value("energy_level", 0);
    context.stress_level = json.value("stress_level", 0);
    context.current_action = json.value("current_action", "");
    context.last_timestamp = json.value("last_timestamp", "");
    context.last_reward = json.value("last_reward", 0.0f);
    return context;
}

nlohmann::json interactionToJson(const InteractionEvent& event) {
    nlohmann::json json = nlohmann::json::object();
    json["timestamp"] = event.timestamp;
    json["action_taken"] = event.action_taken;
    json["summary"] = event.summary;
    json["reward"] = event.reward;
    json["state_summary"] = event.state_summary;
    json["affect_signal"] = event.affect_signal;
    return json;
}

InteractionEvent jsonToInteraction(const nlohmann::json& json) {
    InteractionEvent event;
    event.timestamp = json.value("timestamp", "");
    event.action_taken = json.value("action_taken", "");
    event.summary = json.value("summary", "");
    event.reward = json.value("reward", 0.0f);
    event.state_summary = json.value("state_summary", "");
    event.affect_signal = json.value("affect_signal", 0.0f);
    return event;
}

std::vector<std::string> splitSummary(const std::string& text) {
    std::istringstream stream(text);
    std::vector<std::string> parts;
    std::string token;
    while (stream >> token) {
        parts.push_back(token);
    }
    return parts;
}

} // namespace

bool saveMemoryRecords(const std::string& path, const std::vector<record>& records) {
    std::ofstream output(path);
    if (!output.is_open()) {
        return false;
    }

    nlohmann::json payload = nlohmann::json::array();
    for (const auto& entry : records) {
        payload.push_back(recordToJson(entry));
    }
    output << payload.dump(2);
    return true;
}

std::vector<record> loadMemoryRecords(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return {};
    }

    nlohmann::json payload;
    input >> payload;
    if (!payload.is_array()) {
        return {};
    }

    std::vector<record> records;
    records.reserve(payload.size());
    for (const auto& item : payload) {
        records.push_back(jsonToRecord(item));
    }
    return records;
}

bool saveAffectiveState(const std::string& path, const AffectiveState& affect) {
    std::ofstream output(path);
    if (!output.is_open()) {
        return false;
    }

    output << affectToJson(affect).dump(2);
    return true;
}

AffectiveState loadAffectiveState(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return AffectiveState{};
    }

    nlohmann::json payload;
    input >> payload;
    return jsonToAffect(payload);
}

bool saveSessionContext(const std::string& path, const SessionContext& context) {
    std::ofstream output(path);
    if (!output.is_open()) {
        return false;
    }

    output << sessionToJson(context).dump(2);
    return true;
}

SessionContext loadSessionContext(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return SessionContext{};
    }

    nlohmann::json payload;
    input >> payload;
    return jsonToSession(payload);
}

bool saveInteractionHistory(const std::string& path, const std::vector<InteractionEvent>& history) {
    std::ofstream output(path);
    if (!output.is_open()) {
        return false;
    }

    nlohmann::json payload = nlohmann::json::array();
    for (const auto& event : history) {
        payload.push_back(interactionToJson(event));
    }
    output << payload.dump(2);
    return true;
}

std::vector<InteractionEvent> loadInteractionHistory(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return {};
    }

    nlohmann::json payload;
    input >> payload;
    if (!payload.is_array()) {
        return {};
    }

    std::vector<InteractionEvent> history;
    history.reserve(payload.size());
    for (const auto& item : payload) {
        history.push_back(jsonToInteraction(item));
    }
    return history;
}

bool saveAggregateFacts(const std::string& path, const std::vector<record>& facts) {
    return saveMemoryRecords(path, facts);
}

std::vector<record> loadAggregateFacts(const std::string& path) {
    return loadMemoryRecords(path);
}

std::vector<record> aggregateRestStateFacts(const std::vector<InteractionEvent>& history,
                                          const std::vector<record>& existingFacts) {
    std::vector<record> aggregated;
    if (history.empty()) {
        return aggregated;
    }

    std::map<std::string, int> summaryCounts;
    for (const auto& event : history) {
        const auto tokens = splitSummary(event.summary);
        if (tokens.size() >= 2) {
            const std::string subject = tokens.front();
            const std::string object = tokens.back();
            const std::string key = subject + "|" + object;
            summaryCounts[key] += 1;
        }
    }

    for (const auto& [key, count] : summaryCounts) {
        const auto split = key.find('|');
        if (split == std::string::npos) {
            continue;
        }

        record fact;
        fact.raw_fact.subject = key.substr(0, split);
        fact.raw_fact.relation = "mentions";
        fact.raw_fact.object = key.substr(split + 1);
        fact.confidence = std::clamp(0.15f + 0.05f * static_cast<float>(count), 0.0f, 0.35f);
        fact.generalization_factor = 0.2f;
        fact.current_context = record::GENERAL;
        fact.relevance = std::clamp(0.25f + 0.05f * static_cast<float>(count), 0.0f, 0.5f);
        fact.recency = 0.5f;
        fact.observation_count = count;
        fact.last_updated = "rest_state";
        aggregated.push_back(fact);
    }

    if (aggregated.empty()) {
        return existingFacts;
    }

    return aggregated;
}

action_log::action_log() = default;

nlohmann::json memory_loader::read_data(std::string path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return nlohmann::json{};
    }

    nlohmann::json data;
    input >> data;
    return data;
}

bool memory_loader::save_data(std::string path) {
    std::ofstream output(path);
    if (!output.is_open()) {
        return false;
    }

    output << nlohmann::json{};
    return true;
}

NLPClient::NLPClient() = default;

NLPClient::~NLPClient() {
    stop();
}

void NLPClient::start() {
    running = true;
}

void NLPClient::submitText(const std::string &text) {
    std::lock_guard<std::mutex> lock(requestMutex);
    requestQueue.push(text);
    requestCV.notify_one();
}

nlohmann::json NLPClient::getResult() {
    std::unique_lock<std::mutex> lock(responseMutex);
    if (responseQueue.empty()) {
        return nlohmann::json{};
    }

    nlohmann::json response = responseQueue.front();
    responseQueue.pop();
    return response;
}

void NLPClient::stop() {
    running = false;
}

void NLPClient::workerLoop() {
    while (running) {
        std::unique_lock<std::mutex> lock(requestMutex);
        requestCV.wait(lock, [this]() { return !requestQueue.empty() || !running; });
        if (!running && requestQueue.empty()) {
            return;
        }

        if (!requestQueue.empty()) {
            const std::string text = requestQueue.front();
            requestQueue.pop();
            lock.unlock();

            nlohmann::json response = { {"text", text}, {"tokens", nlohmann::json::array()} };
            std::lock_guard<std::mutex> responseLock(responseMutex);
            responseQueue.push(response);
            responseCV.notify_one();
        }
    }
}

void NLPClient::sendRequest(const std::string &text) {
    submitText(text);
}

nlohmann::json NLPClient::receiveResponse() {
    return getResult();
}

std::vector<record> memory_parser::get_relevant_records(std::string input, const state& stateRef) {
    (void)input;
    (void)stateRef;
    return {};
}

std::vector<ex_record> memory_parser::extract_data_from_records(std::vector<record>, NLPClient) {
    return {};
}

void memory_component::load_memory() {}
void memory_component::update_memory() {}
void memory_component::store_context() {}
