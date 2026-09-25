#include <iostream>
#include <math.h>
#include <vector>
#include <Include/json.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
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
class state;
struct record
{
    fact raw_fact;
    float confidence;
    float generalization_factor;
    enum user_context{GENERAL,USER_SPECIFIC};
    user_context current_context;
    std::vector<std::string> relevant_users;
    // How the fact was obtained
    enum source_type{USER_STATEMENT,OBSERVATION,ACTION_HISTORY, INFERENCE};
    source_type source;
    // How often / recently this information has been reinforced
    int observation_count;// tracking to prove confidence value
    std::string last_updated;// How useful this memory is for retrieval
    float relevance; // will be calculation based on a factor of confidence,generalization_factor and update_time( makes fetching faster)
    record();
    void update_record();
};
class action_log
{
    std::string action_taken;
    std::string time;
    float count;
    action_log();
};
class cache
{
    std::vector<ex_record> data_store;
};
class memory_loader
{
    nlohmann::json read_data(std::string path);
    bool save_data(std::string path);
};
class NLPClient
{
private:
    // Python process
    // stdin/stdout pipes
    // Worker thread
    std::thread worker;

    // Communication
    std::queue<std::string> requestQueue;
    std::queue<nlohmann::json> responseQueue;

    // Synchronization
    std::mutex requestMutex;
    std::mutex responseMutex;
    // prevents continous checks to the queues
    std::condition_variable requestCV;
    std::condition_variable responseCV;
    // Lifecycle
    bool running;
    // Internal worker
    void workerLoop();
    // Communication with Python
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
class input_parser{};
class memory_parser
{
    std::vector<record> get_relevant_records(std::string input, state State);
    std::vector<ex_record> extract_data_from_records(std::vector<record>, NLPClient);
};
class memory_component
{
public:
    memory_parser parser;
    memory_loader loader;
    cache active_mem; // store of parsed facts to prevent reparsing already known data too often
    std::vector<action_log> logs;
    std::vector<record> records; // raw data temp_store (can be cleared after parsing)
    void load_memory();
    void update_memory();
    void store_context(); // might be useful to allow compression of the agent to prevent data loss
};
