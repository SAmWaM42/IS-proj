#pragma once

#include <cstddef>
#include <map>
#include <memory>
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
    void update(std::size_t state, std::string action, double reward, std::size_t nextState);
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

    float getQValue(const state& stateRef, const std::string& action) const;
    void setQValue(const state& stateRef, const std::string& action, float value);
    float getMaxQValue(const state& stateRef) const;
    std::string getBestAction(const state& stateRef) const;
    std::string getRandomAction(const state& stateRef) const;

public:
    QLearning(float learning_rate, float discount_factor, float epsilon);

    void setActions(const std::vector<std::string>& actions);
    std::string chooseAction(const state& stateRef) const;
    void update(const state& current_state, const std::string& action, float reward, const state& next_state);

    void setEpsilon(float epsilon);
    float getEpsilon() const;

    void loadQTable(const std::string& path);
    void saveQTable(const std::string& path);
    void loadTrainingData(const std::string& path);
};

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

bool runBaselineValidation();
