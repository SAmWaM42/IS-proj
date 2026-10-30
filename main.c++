#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "Components/State-Action/state-action.h"
#include "Components/Training/training.h"
#include "Components/Study/study.h"
#include "Components/Simulation/companion-ui.h"

namespace
{
    bool parsePositiveInt(const char *value, int &result)
    {
        if (value == nullptr || *value == '\0')
        {
            return false;
        }
        char *end = nullptr;
        const long parsed = std::strtol(value, &end, 10);
        if (*end != '\0' || parsed < 1 || parsed > 10000000)
        {
            return false;
        }
        result = static_cast<int>(parsed);
        return true;
    }

    void printUsage(const char *executable)
    {
        std::cout
            << "Usage:\n"
            << "  " << executable << " --self-test\n"
            << "  " << executable << " --train [--episodes N] [--steps N] [--seed N] [--out DIR]\n"
            << "  " << executable << " --resume-training [--episodes N] [--steps N] [--seed N] [--out DIR] (loads and continues saved Q-tables)\n"
            << "  " << executable << " --validate [--episodes N] [--steps N] [--seed N] [--out DIR]\n"
            << "  " << executable << " --compare [--episodes N] [--steps N] [--seed N] [--out DIR]\n"
            << "  " << executable << " --diagnose-idle [--policy-dir DIR] [--out DIR] [--seed N]\n"
            << "  " << executable << " --diagnose-temporal [--policy-dir DIR] [--out DIR] [--seed N]\n"
            << "  " << executable << " --study [--policy-dir DIR] [--study-out DIR]\n"
            << "Run --train before --validate, --compare, --study, --diagnose-idle, or --diagnose-temporal. --resume-training continues existing Q-tables. Evaluation disables policy updates.\n";
    }
} // namespace

int main(int argc, char **argv)
{
    std::string mode;
    std::string outputDirectory = "artifacts/training";
    std::string studyOutputDirectory = (std::filesystem::path(IS2_SOURCE_DIR).parent_path() / "companion-study-data").string();
    std::string policyDirectory = (std::filesystem::path(IS2_SOURCE_DIR) / "artifacts/training").string();
    int episodes = 200;
    int steps = 40;
    int seed = 42;
    bool explicitEpisodes = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--sim" || arg == "sim" || arg == "--sim-researcher")
        {
            try
            {
                const bool researcherMode = arg == "--sim-researcher";
                CompanionUI simulation(policyDirectory, "artifacts/simulation", researcherMode);
                simulation.run();
            }
            catch (const std::exception &error)
            {
                std::cerr << "Unable to start companion UI: " << error.what() << '\n';
                return 1;
            }
            return 0;
        }
        if (arg == "--help" || arg == "-h")
        {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "--self-test" || arg == "--train" || arg == "--resume-training" || arg == "--validate" || arg == "--compare" || arg == "--diagnose-idle" || arg == "--diagnose-temporal" || arg == "--study")
        {
            if (!mode.empty())
            {
                std::cerr << "Choose exactly one experiment mode.\n";
                return 2;
            }
            mode = arg == "--self-test" ? "self-test" : arg.substr(2);
            continue;
        }
        if (arg == "--episodes" || arg == "--steps" || arg == "--seed" || arg == "--out" || arg == "--study-out" || arg == "--policy-dir")
        {
            if (i + 1 >= argc)
            {
                std::cerr << arg << " requires a value.\n";
                printUsage(argv[0]);
                return 2;
            }
            const char *value = argv[++i];
            if (arg == "--out")
            {
                outputDirectory = value;
            }
            else if (arg == "--study-out")
            {
                studyOutputDirectory = value;
            }
            else if (arg == "--policy-dir")
            {
                policyDirectory = value;
            }
            else
            {
                int parsed = 0;
                if (!parsePositiveInt(value, parsed))
                {
                    std::cerr << arg << " requires a positive integer.\n";
                    return 2;
                }
                if (arg == "--episodes")
                {
                    episodes = parsed;
                    explicitEpisodes = true;
                }
                else if (arg == "--steps")
                {
                    steps = parsed;
                }
                else
                {
                    seed = parsed;
                }
            }
            continue;
        }

        std::cerr << "Unknown argument: " << arg << '\n';
        printUsage(argv[0]);
        return 2;
    }

    if (mode == "self-test")
    {
        if (!runBaselineValidation() || !runTrainingSelfTests())
        {
            return 1;
        }
        std::cout << "Component validation passed.\n";
        return 0;
    }

    if (mode == "diagnose-idle")
    {
        return runIdlePolicyDiagnostics(policyDirectory, outputDirectory, static_cast<unsigned int>(seed)) ? 0 : 1;
    }

    if (mode == "diagnose-temporal")
    {
        return runTemporalStimulusDiagnostics(policyDirectory, outputDirectory, static_cast<unsigned int>(seed)) ? 0 : 1;
    }

    if (mode == "study")
    {
        try
        {
            CompanionUI study(policyDirectory, studyOutputDirectory, false, true);
            study.run();
        }
        catch (const std::exception &error)
        {
            std::cerr << "Unable to start guided believability study: " << error.what() << '\n';
            return 1;
        }
        return 0;
    }

    if ((mode == "train" || mode == "resume-training") && !explicitEpisodes)
    {
        episodes = 1000;
    }
    if (mode.empty())
    {
        CompanionUI simulation(policyDirectory, "artifacts/simulation", false);
        simulation.run();
        return 0;
    }

    if (!runTrainingExperiment(mode, episodes, steps, static_cast<unsigned int>(seed), outputDirectory,
                               mode == "resume-training"))
    {
        return 1;
    }

    return 0;
}
