#include "memory.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

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
}

record::record() = default;

void record::update_record() {
    observation_count += 1;
    confidence = std::clamp(confidence + 0.1f, 0.0f, 1.0f);
    relevance = std::clamp(confidence * (generalization_factor + 1.0f), 0.0f, 1.0f);
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
