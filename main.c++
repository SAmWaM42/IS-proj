#include <cstdlib>
#include <iostream>
#include <string>

#include "Components/Environment/environment.h"
#include "Components/State-Action/state-action.h"
#include "Components/Training/training.h"
#include "Components/Study/study.h"

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
        << "  " << executable << " --self-test\n"
        << "  " << executable << " --train [--episodes N] [--steps N] [--seed N] [--out DIR]\n"
        << "  " << executable << " --validate [--episodes N] [--steps N] [--seed N] [--out DIR]\n"
        << "  " << executable << " --compare [--episodes N] [--steps N] [--seed N] [--out DIR]\n"
        << "  " << executable << " --study [--policy-dir DIR] [--study-out DIR]\n"
        << "  " << executable << " --sim | --sim-history\n\n"
        << "Run --train before --validate, --compare, or --study. Evaluation disables policy updates.\n";
}
} // namespace

int main(int argc, char** argv) {
    std::string mode;
    std::string outputDirectory = "artifacts/training";
    std::string studyOutputDirectory = "artifacts/study";
    std::string policyDirectory = "artifacts/training";
    int episodes = 200;
    int steps = 40;
    int seed = 42;
    bool explicitEpisodes = false;

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
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "--self-test" || arg == "--train" || arg == "--validate" || arg == "--compare" || arg == "--study") {
            if (!mode.empty()) {
                std::cerr << "Choose exactly one experiment mode.\n";
                return 2;
            }
            mode = arg == "--self-test" ? "self-test" : arg.substr(2);
            continue;
        }
        if (arg == "--episodes" || arg == "--steps" || arg == "--seed" || arg == "--out" || arg == "--study-out" || arg == "--policy-dir") {
            if (i + 1 >= argc) {
                std::cerr << arg << " requires a value.\n";
                printUsage(argv[0]);
                return 2;
            }
            const char* value = argv[++i];
            if (arg == "--out") {
                outputDirectory = value;
            } else if (arg == "--study-out") {
                studyOutputDirectory = value;
            } else if (arg == "--policy-dir") {
                policyDirectory = value;
            } else {
                int parsed = 0;
                if (!parsePositiveInt(value, parsed)) {
                    std::cerr << arg << " requires a positive integer.\n";
                    return 2;
                }
                if (arg == "--episodes") {
                    episodes = parsed;
                    explicitEpisodes = true;
                } else if (arg == "--steps") {
                    steps = parsed;
                } else {
                    seed = parsed;
                }
            }
            continue;
        }

        std::cerr << "Unknown argument: " << arg << '\n';
        printUsage(argv[0]);
        return 2;
    }

    if (mode.empty() || mode == "self-test") {
        if (!runBaselineValidation()) {
            return 1;
        }
        std::cout << "Component validation passed.\n";
        return 0;
    }

    if (mode == "study") {
        return runParticipantStudy(policyDirectory, studyOutputDirectory) ? 0 : 1;
    }

    if (mode == "train" && !explicitEpisodes) {
        episodes = 1000;
    }

    if (!runTrainingExperiment(mode, episodes, steps, static_cast<unsigned int>(seed), outputDirectory)) {
        return 1;
    }
    return 0;
}
