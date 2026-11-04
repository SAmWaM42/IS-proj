#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "Components/State-Action/state-action.h"
#ifndef IS2_PARTICIPANT_BUILD
#include "Components/Training/training.h"
#include "Components/Study/study.h"
#endif
#include "Components/Simulation/companion-ui.h"
#include "Components/Simulation/participant-menu.h"
#include "Components/Simulation/runtime-paths.h"
#include "Components/Simulation/submission-manager.h"
#include "Components/Study/participant-config.h"

// CMake supplies the source-tree path for normal builds. Keep a safe fallback
// for editors or direct compiler invocations that do not load CMake definitions.
#ifndef IS2_SOURCE_DIR
#define IS2_SOURCE_DIR "."
#endif

namespace
{
#ifndef IS2_PARTICIPANT_BUILD
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
#endif
} // namespace

int main(int argc, char **argv)
{
#ifdef IS2_PARTICIPANT_BUILD
    if (argc > 1) {
        std::cerr << "This is the participant application. Start it without command-line options.\n";
        return 2;
    }
    auto appRoot = runtime_paths::executableDirectory(argc > 0 ? argv[0] : nullptr);
    if (!std::filesystem::exists(appRoot / "Data" / "Assets") && std::filesystem::exists(IS2_SOURCE_DIR))
        appRoot = IS2_SOURCE_DIR;
    const auto userDataRoot = runtime_paths::userDataDirectory();
#ifdef _WIN32
    const auto bundledPython = appRoot / "runtime" / "python" / "python.exe";
#else
    const auto bundledPython = appRoot / "runtime" / "python" / "bin" / "python3";
#endif
    std::error_code pathError;
    std::filesystem::create_directories(userDataRoot, pathError);
    if (pathError) {
        std::cerr << "Unable to create application data directory: " << pathError.message() << '\n';
        return 1;
    }
    std::filesystem::current_path(appRoot, pathError);
    if (pathError) {
        std::cerr << "Unable to locate application resources: " << pathError.message() << '\n';
        return 1;
    }
    try {
        SubmissionManager submissions(userDataRoot, appRoot, bundledPython);
        const auto uploadSummary = submissions.uploadPendingSubmissions();
        if (uploadSummary.pending > 0) {
            std::cerr << uploadSummary.pending
                      << " questionnaire submission(s) remain pending upload.\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "Pending questionnaire uploads could not be checked: "
                  << error.what() << '\n';
    }
    const auto packagedPolicies = appRoot / "Policies";
    const std::string policyDirectory =
        (std::filesystem::exists(packagedPolicies) ? packagedPolicies : appRoot / "artifacts" / "training").string();
    const std::string studyOutputDirectory = (userDataRoot / "study-linkage").string();
    const std::string runtimeStatePath = (userDataRoot / "companion-state").string();
    try {
        for (;;) {
            ParticipantMenu menu;
            const auto choice = menu.run();
            if (choice == ParticipantMenu::Choice::Exit) break;
            if (choice == ParticipantMenu::Choice::BelievabilityStudy) {
                try {
                    // Credentials are installed beside the executable in the
                    // root of the extracted application directory.
                    const auto credentialsPath = appRoot / "credentials.json";
                    const auto participantConfig = study::ParticipantConfig::load(credentialsPath);
                    CompanionUI study(policyDirectory, studyOutputDirectory, false, true,
                                      runtimeStatePath, participantConfig);
                    study.run();
                } catch (const std::exception& error) {
                    std::cerr << "Research Session cannot start: " << error.what() << '\n';
                    // Return to the menu; Free Roam remains available without credentials.
                    continue;
                }
                break;
            }
            CompanionUI simulation(policyDirectory, (userDataRoot / "general-interaction").string(),
                                   false, false, runtimeStatePath);
            if (!simulation.run()) break;
        }
    } catch (const std::exception& error) {
        std::cerr << "Unable to start Animal Agent: " << error.what() << '\n';
        return 1;
    }
    return 0;
#else

    auto appRoot = runtime_paths::executableDirectory(argc > 0 ? argv[0] : nullptr);
    if (!std::filesystem::exists(appRoot / "Data" / "Assets") && std::filesystem::exists(IS2_SOURCE_DIR))
        appRoot = IS2_SOURCE_DIR;
    const auto userDataRoot = runtime_paths::userDataDirectory();
    std::error_code pathError;
    std::filesystem::create_directories(userDataRoot, pathError);
    if (pathError) {
        std::cerr << "Unable to create application data directory: " << pathError.message() << '\n';
        return 1;
    }
    std::filesystem::current_path(appRoot, pathError);
    if (pathError) {
        std::cerr << "Unable to locate application resources: " << pathError.message() << '\n';
        return 1;
    }

    std::string mode;
    std::string outputDirectory = (userDataRoot / "training").string();
    std::string studyOutputDirectory = (userDataRoot / "study-linkage").string();
    const auto packagedPolicies = appRoot / "Policies";
    std::string policyDirectory = (std::filesystem::exists(packagedPolicies) ? packagedPolicies : appRoot / "artifacts" / "training").string();
    const std::string runtimeStatePath = (userDataRoot / "companion-state").string();
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
            CompanionUI study(policyDirectory, studyOutputDirectory, false, true, runtimeStatePath);
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
        try
        {
            bool showMenu = true;
            while (showMenu)
            {
                ParticipantMenu menu;
                const auto choice = menu.run();
                if (choice == ParticipantMenu::Choice::Exit)
                {
                    break;
                }

                if (choice == ParticipantMenu::Choice::BelievabilityStudy)
                {
                    CompanionUI study(policyDirectory, studyOutputDirectory, false, true);
                    study.run();
                    // The guided study owns its completion and withdrawal flow.
                    // Returning from it ends this application run.
                    break;
                }

                CompanionUI simulation(policyDirectory, (userDataRoot / "general-interaction").string(), false, false, runtimeStatePath);
                showMenu = simulation.run();
            }
        }
        catch (const std::exception &error)
        {
            std::cerr << "Unable to start Animal Agent: " << error.what() << '\n';
            return 1;
        }
        return 0;
    }

    if (!runTrainingExperiment(mode, episodes, steps, static_cast<unsigned int>(seed), outputDirectory,
                               mode == "resume-training"))
    {
        return 1;
    }

    return 0;
#endif
}
