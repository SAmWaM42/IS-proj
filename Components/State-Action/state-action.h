// this is the general actions with the specific actions being subclasses of the above
#include <iostream>
#include <math.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
class memory_component;
// defentitions for state-action implementation

//state definition
enum class InputType // will be used elsewhere
{
    NONE,
    GREETING,
    PRAISE,
    REQUEST,
    NEGATIVE,
    QUESTION
};
enum class distance{
CLOSE,MEDIUM,FAR,UNAVAILABLE
};

enum class Level
{
    LOW,
    MEDIUM,
    HIGH
};
struct state
{
    // What just happened
    // Relevant environmental information
    std::map<std::string,distance> environmentCondition;
    // Agent's internal state
    Level energy;
    Level stress;
    Level curiosity;
    Level attachment;
    // Relevant memory information
    float memory; // fine for npw but may expand if system allows
    float encoded_user_input;// default to one if none;
    // Convert the state into something the Q-table can use
     std::size_t encode() const;
};

// ACTION DEFINITION
class base_action
{
public:
    virtual void act() = 0;
    virtual ~base_action() = default;
};

class complex_action
{
public:
    std::string action_tag;
    std::unique_ptr<base_action> internal_current_action;
    virtual void act(std::vector<base_action> included_actions, std::vector<std::string> action_order, memory_component *mem) = 0;
};
// Qtable
class QTable
{
private:
    // state -> action -> Q value
    std::unordered_map<std::size_t, std::unordered_map<std::string, double>> table;

    double learningRate;
    double discountFactor;

public:
    QTable(double learningRate, double discountFactor);

    double getQValue(
        std::size_t state,
        std::string action) const;

    void update(
        std::size_t state,
        std::string action,
        double reward,
        std::size_t nextState);

    std::string chooseAction(std::size_t state);

    void save(const std::string &filename);
    void load(const std::string &filename);
};

// specific action space (will be the same for both agents but will only be called)
class move : base_action{};
class grab : base_action{};
class lay : base_action{};
class shake_head : base_action{};
class idle : base_action{}; // can be used under continous invalid or unprocessibke output or state transition.
class bark : base_action{};
class get_attention : complex_action{};
class follow : complex_action{};
class get_confused : complex_action{};
class snuggle : complex_action{};
class fetch : complex_action{};
class wander : complex_action{};
class respond_positive:complex_action{};
class respond_negative:complex_action{};

// qLearning class structure
class QLearning
{
private:
    // ---- Core Q-learning data ----
    // QTable q_table; not implemented yet
    float learning_rate;
    float discount_factor;
    float epsilon;

    // ---- Action space ----

    std::vector<complex_action> actions;

    // ---- Internal helpers ----

    float getQValue(const state &state, const complex_action &action);

    void setQValue(
        const state &state,
        const complex_action &action,
        float value);

    float getMaxQValue(const state &state);

    complex_action getBestAction(const state &state);

    complex_action getRandomAction(const state &state);

public:
    // ---- Construction / configuration ----

    QLearning(
        float learning_rate,
        float discount_factor,
        float epsilon);

    void setActions(const std::vector<complex_action> &actions);

    // ---- Decision making ----

    complex_action chooseAction(const state &state);

    // ---- Learning ----

    void update(
        const state &current_state,
        const complex_action &action,
        float reward,
        const state &next_state);

    // ---- Exploration ----

    void setEpsilon(float epsilon);

    float getEpsilon() const;

    // ---- Persistence / pretraining ----

    void loadQTable(const std::string &path);
    void saveQTable(const std::string &path);
    void loadTrainingData(const std::string &path);
};