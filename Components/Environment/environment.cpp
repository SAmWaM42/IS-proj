#include "environment.h"

void tab::updateParameters(std::map<std::string, std::string> parameters_to_update) {
    parameters_to_display.insert(parameters_to_update.begin(), parameters_to_update.end());
}

void window::updateWindow() {
    for (auto& tabEntry : window_tabs) {
        (void)tabEntry;
    }
}

void simulation_environment::initiate() {
    active_window.clear();
    current_user.clear();
}

void simulation_environment::update() {
    clockT += dt;
}

void simulation_environment::fetchRepresentation() {
    // placeholder until richer simulation and rendering are introduced.
}

void simulation_environment::addEntity(std::string entityType) {
    (void)entityType;
}

void simulation_environment::renderObjects() {
    // placeholder for future rendering or visualization work.
}
