#include <iostream>
#include <string>

#include "Components/Environment/environment.h"
#include "Components/State-Action/state-action.h"

int main(int argc, char** argv) {
    if (argc > 1) {
        const std::string mode = argv[1];
        if (mode == "--sim" || mode == "sim") {
            BackyardSimulation simulation;
            simulation.run();
            return 0;
        }
        if (mode == "--sim-history" || mode == "sim-history" || mode == "--history") {
            BackyardSimulation simulation;
            simulation.runScenarioHistory();
            return 0;
        }
        if (mode == "--compare" || mode == "compare") {
            EnvironmentConfig config;
            config.max_steps = 10;
            const auto comparison = runAgentComparison(config, config.max_steps);
            std::cout << "Baseline and proposed agents compared over " << comparison.tick_log.size() << " steps." << std::endl;
            return 0;
        }
    }

    if (!runBaselineValidation()) {
        return 1;
    }

    std::cout << "Baseline learning loop validation passed." << std::endl;
    return 0;
}
