#include  <iostream>
#include <math.h>
#include <vector>
#include <Include/json.hpp>
typedef struct{std::string subject;std::string relation;std::string object;}fact;
typedef struct{std::string subject;float confidence;}ex_record;
class state;//to_be_defined; (is an aggregation of the inputs hence will likely be numerically encoded)
class record
{
    public:
    fact raw_fact;
    float confidence;
    float generalization_factor;
    std::vector<std::string>  relevant_users;//users to include or exclude based on context
    enum user_context{GENERAL,USER_SPECIFIC};
    user_context current_context;
    record();
    void update_record();

};
class action_log{
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
class memory_parser // extracts relevant data from records and ouputs a value passable to the RL function
{
    std::vector<record> get_relevant_records(std::string input,state State); // check if state is null before parsing
    std::vector<ex_record> extract_data_from_records(std::vector<record>);
};
class memory_component
{public:
    memory_parser parser;
    memory_loader loader;
    cache active_mem; // store of parsed facts to prevent reparsing already known data too often
    std::vector<action_log> logs;
    std::vector<record> records; // raw data temp_store (can be cleared after parsing)
    void load_memory();
    void update_memory();
    void store_context(); //might be useful to allow compression of the agent to prevent data loss 


};