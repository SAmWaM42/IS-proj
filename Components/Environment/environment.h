#include  <iostream>
#include <math.h>
#include <vector>
#include <json.hpp>
#include <memory.h>
#include <map>
#define defaultWidth 1920
#define defaultHeight 1080
class object;// any object in world will inherit from this
// i think for now we can have an agent representation hence no direct access btwn the agent and its avatar
class tab{
    // if i need an input i can initialize a tab to overlay on another tab 
    //input can be stored as one of the parameters to display and also helps with logging
public:
    std::string title;
    std::map<std::string,std::string> parameters_to_display; // i need to parse this
    bool visible;
    float x;
    float y;
    float width;
    float height;
    void updateParameters(std::map<std::string,std::string> parameters_to_update);

};
class window{
    public:
    std::vector<tab> window_tabs;
    void updateWindow();
};
class simulation_environment{
    public: 
    float clockT;
    float dt;
    std::string active_window;
    std::map<std::string,window> windows;
    std::vector<std::unique_ptr<object>()> objectStore;
    std::string current_user;
    void initiate();
    void update();
    void fetchRepresentation();
    void addEntity(std::string entityType);
    void renderObjects(); // might add a render manager if the graphics get too complex
};