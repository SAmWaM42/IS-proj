#pragma once

#include <iostream>
#include <json.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define defaultWidth 1920
#define defaultHeight 1080

class object;

class tab {
public:
    std::string title;
    std::map<std::string, std::string> parameters_to_display;
    bool visible = false;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    void updateParameters(std::map<std::string, std::string> parameters_to_update);
};

class window {
public:
    std::vector<tab> window_tabs;
    void updateWindow();
};

class simulation_environment {
public:
    float clockT = 0.0f;
    float dt = 0.0f;
    std::string active_window;
    std::map<std::string, window> windows;
    std::vector<std::unique_ptr<object>> objectStore;
    std::string current_user;

    void initiate();
    void update();
    void fetchRepresentation();
    void addEntity(std::string entityType);
    void renderObjects();
};
