#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "Components/Memory/memory.h"

enum class InputType
{
    NONE,
    GREETING,
    PRAISE,
    REQUEST,
    NEGATIVE,
    QUESTION
};

enum class distance
{
    CLOSE,
    MEDIUM,
    FAR,
    UNAVAILABLE
};

enum class Level
{
    LOW,
    MEDIUM,
    HIGH
};

struct EnvironmentConfig {
    int width = 960;
    int height = 600;
    int max_steps = 100;
    int seed = 42;
    std::vector<std::string> actions = {
        "FETCH",
        "WANDER",
        "BARK",
        "ACT_CONFUSED",
        "SLEEP_LAY",
        "NUZZLE",
        "INSIST",
        "GREETING"
    };
    float reward_positive = 1.0f;
    float reward_negative = -1.0f;
    float reward_neutral = 0.0f;
    bool allow_user_stimulus = true;
};

using ActionOutcome = ActionResult;

struct state
{
    std::map<std::string, distance> environmentCondition;
    Level energy = Level::LOW;
    Level stress = Level::LOW;
    Level curiosity = Level::LOW;
    Level attachment = Level::LOW;
    AffectiveState affect;
    float memory = 0.0f;
    float encoded_user_input = 0.0f;
    int memory_bias = 0;
    int affect_trend = 0;

    std::size_t encode() const;
};

class base_action
{
public:
    virtual ~base_action() = default;
    virtual void act() = 0;
};

class complex_action
{
public:
    std::string action_tag;
    std::unique_ptr<base_action> internal_current_action;
    virtual ~complex_action() = default;
    virtual void act(std::vector<base_action> included_actions,
                    std::vector<std::string> action_order,
                    void* mem) = 0;
};

class QTable
{
public:
    std::unordered_map<std::size_t, std::unordered_map<std::string, double>> table;

private:
    double learningRate;
    double discountFactor;

public:
    QTable(double learningRate, double discountFactor);

    double getQValue(std::size_t state, std::string action) const;
    void update(std::size_t state, std::string action, double reward, std::size_t nextState, bool terminal = false);
    std::string chooseAction(std::size_t state);

    void save(const std::string& filename);
    void load(const std::string& filename);
};

class QLearning
{
private:
    QTable q_table;
    float learning_rate;
    float discount_factor;
    float epsilon;
    std::vector<std::string> actions;
    mutable std::mt19937 rng;

    float getQValue(const state& stateRef, const std::string& action) const;
    void setQValue(const state& stateRef, const std::string& action, float value);
    float getMaxQValue(const state& stateRef) const;
    std::string getBestAction(const state& stateRef) const;
    std::string getRandomAction(const state& stateRef) const;

public:
    QLearning(float learning_rate, float discount_factor, float epsilon, unsigned int seed = 42);

    void setActions(const std::vector<std::string>& actions);
    std::string chooseAction(const state& stateRef) const;
    void update(const state& current_state, const std::string& action, float reward, const state& next_state, bool terminal = false);

    void setEpsilon(float epsilon);
    float getEpsilon() const;

    void loadQTable(const std::string& path);
    void saveQTable(const std::string& path);
    void loadTrainingData(const std::string& path);
};

class BaselineAgent {
public:
    explicit BaselineAgent(const EnvironmentConfig& config = EnvironmentConfig{});

    void reset();
    void setExplorationRate(float epsilon);
    void setLearningEnabled(bool enabled);
    std::string selectAction(const EnvironmentObservation& observation);
    void applyOutcome(const EnvironmentObservation& observation,
                      const std::string& action,
                      const ActionOutcome& outcome);

    void loadPersistentState(const std::string& path);
    void savePersistentState(const std::string& path);

    const state& getCurrentState() const;
    const SessionContext& getSessionContext() const;
    const std::vector<InteractionEvent>& getInteractionHistory() const;

private:
    EnvironmentConfig config;
    QLearning q_policy;
    SessionContext session_context;
    std::vector<InteractionEvent> interaction_history;
    state current_state;
};

class ProposedAgent {
public:
    explicit ProposedAgent(const EnvironmentConfig& config = EnvironmentConfig{});

    void reset();
    void setExplorationRate(float epsilon);
    void setLearningEnabled(bool enabled);
    std::string selectAction(const EnvironmentObservation& observation);
    void applyOutcome(const EnvironmentObservation& observation,
                      const std::string& action,
                      const ActionOutcome& outcome);

    void loadPersistentState(const std::string& path);
    void savePersistentState(const std::string& path);

    const state& getCurrentState() const;
    const SessionContext& getSessionContext() const;
    const std::vector<InteractionEvent>& getInteractionHistory() const;
    const std::vector<record>& getMemoryRecords() const;
    const AffectiveState& getAffectiveState() const;

private:
    EnvironmentConfig config;
    QLearning q_policy;
    SessionContext session_context;
    AffectiveState affective_state;
    std::vector<record> memory_records;
    std::vector<InteractionEvent> interaction_history;
    RuntimeContext runtime_context;
    state current_state;
};

struct AgentComparisonRun {
    EnvironmentConfig config;
    BaselineAgent baseline_agent;
    ProposedAgent proposed_agent;
    std::vector<SimulationTick> tick_log;
};

AgentComparisonRun runAgentComparison(const EnvironmentConfig& config, int steps = 10);

state makeState(int intent,
                distance proximity,
                Level energy,
                Level stress,
                Level curiosity,
                Level attachment,
                int userStatus = 0,
                float memory = 0.0f);

state makeStateWithContext(int intent,
                          distance proximity,
                          Level energy,
                          Level stress,
                          Level curiosity,
                          Level attachment,
                          int userStatus,
                          float memory,
                          float confidence,
                          float relevance,
                          float recency,
                          const AffectiveState& affect);

state makeStateFromMemoryRecords(int intent,
                                distance proximity,
                                Level energy,
                                Level stress,
                                Level curiosity,
                                Level attachment,
                                int userStatus,
                                float memory,
                                const std::vector<record>& records,
                                const AffectiveState& affect);

state makeRuntimeState(const RuntimeContext& runtimeContext);
int summarizeMemoryBias(float confidence, float relevance, float recency);
int summarizeAffectTrend(const AffectiveState& affect, Level energy);

bool runBaselineValidation();
