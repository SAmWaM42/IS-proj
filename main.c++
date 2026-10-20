#include <cstdlib>
#include <iostream>
#include <string>

#include "Components/Environment/environment.h"
#include "Components/State-Action/state-action.h"

namespace {
bool parsePositiveInt(const char* value, int& result) {
    if (value == nullptr || *value == '\0') {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (*end != '\0' || parsed < 1 || parsed > 10000000) {
        return false;
    }
    result = static_cast<int>(parsed);
    return true;
}

void printUsage(const char* executable) {
    std::cout
        << "Usage:\n"
        << "  " << executable << " [--self-test]\n"
        << "  " << executable << " --sim\n"
        << "  " << executable << " --sim-history\n"
        << "  " << executable << " --compare [--steps N]\n\n"
        << "Note: --compare currently runs the existing shared-observation harness; "
        << "it is not yet the full multi-episode training/evaluation pipeline.\n";
}
} // namespace

int main(int argc, char** argv) {
    int comparisonSteps = 10;
    bool compare = false;
    bool selfTest = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sim" || arg == "sim") {
            BackyardSimulation simulation;
            simulation.run();
            return 0;
        }
        if (arg == "--sim-history" || arg == "sim-history" || arg == "--history") {
            BackyardSimulation simulation;
            simulation.runScenarioHistory();
            return 0;
        }
        if (arg == "--self-test" || arg == "--validate") {
            selfTest = true;
            continue;
        }
        if (arg == "--compare" || arg == "compare") {
            compare = true;
            continue;
        }
        if (arg == "--steps") {
            if (i + 1 >= argc || !parsePositiveInt(argv[++i], comparisonSteps)) {
                std::cerr << "--steps requires a positive integer.\n";
                printUsage(argv[0]);
                return 2;
            }
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        printUsage(argv[0]);
        return 2;
    }

    if (selfTest && compare) {
        std::cerr << "--self-test and --compare cannot be used together.\n";
        return 2;
    }

    if (compare) {
        EnvironmentConfig config;
        config.max_steps = comparisonSteps;
        const auto comparison = runAgentComparison(config, comparisonSteps);
        std::cout << "Baseline and proposed agents compared over "
                  << comparison.tick_log.size() << " shared observations.\n";
        std::cout << "Note: the current harness logs baseline outcomes only and uses a "
                     "fixed observation sequence; interpret this as a component smoke test, "
                     "not as comparative performance evidence.\n";
        return 0;
    }

    if (!runBaselineValidation()) {
        return 1;
    }

    std::cout << "Baseline learning loop validation passed.\n";
    return 0;
}
