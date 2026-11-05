#include <filesystem>
#include <iostream>
#include <string>

#include "Components/State-Action/state-action.h"
#include "Components/Simulation/companion-ui.h"
#include "Components/Simulation/participant-menu.h"
#include "Components/Simulation/runtime-paths.h"
#include "Components/Simulation/submission-manager.h"
#include "Components/Study/participant-config.h"

#ifndef IS2_SOURCE_DIR
#define IS2_SOURCE_DIR "."
#endif

int main(int argc, char **argv)
{
    if (argc > 1) {
        std::cerr << "This is the participant application. Start it without command-line options.\n";
        return 2;
    }

    auto appRoot = runtime_paths::executableDirectory(argc > 0 ? argv[0] : nullptr);
    if (!std::filesystem::exists(appRoot / "Data" / "Assets") &&
        std::filesystem::exists(IS2_SOURCE_DIR)) {
        appRoot = IS2_SOURCE_DIR;
    }

    const auto userDataRoot = runtime_paths::userDataDirectory();
    std::error_code pathError;
    std::filesystem::create_directories(userDataRoot, pathError);
    if (pathError) {
        std::cerr << "Unable to create application data directory: "
                  << pathError.message() << '\n';
        return 1;
    }

    std::filesystem::current_path(appRoot, pathError);
    if (pathError) {
        std::cerr << "Unable to locate application resources: "
                  << pathError.message() << '\n';
        return 1;
    }

#ifdef _WIN32
    const auto bundledPython = appRoot / "runtime" / "python" / "python.exe";
#else
    const auto bundledPython = appRoot / "runtime" / "python" / "bin" / "python3";
#endif

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
        (std::filesystem::exists(packagedPolicies)
             ? packagedPolicies
             : appRoot / "artifacts" / "training").string();
    const std::string studyOutputDirectory =
        (userDataRoot / "study-linkage").string();
    const std::string runtimeStatePath =
        (userDataRoot / "companion-state").string();

    try {
        ParticipantMenu menu;
        for (;;) {
            const auto choice = menu.run();
            if (choice == ParticipantMenu::Choice::Exit) {
                break;
            }

            if (choice == ParticipantMenu::Choice::BelievabilityStudy) {
                try {
                    // credentials.json is distributed separately and must sit
                    // beside the executable. Never include it in the common ZIP.
                    const auto credentialsPath = appRoot / "credentials.json";
                    const auto participantConfig =
                        study::ParticipantConfig::load(credentialsPath);
                    CompanionUI study(menu.getWindow(), policyDirectory,
                                      studyOutputDirectory, false, true,
                                      runtimeStatePath, participantConfig);
                    if (!study.run()) {
                        break;
                    }
                } catch (const std::exception& error) {
                    std::cerr << "Research Session cannot start: "
                              << error.what() << '\n';
                }
                continue;
            }

            CompanionUI simulation(
                menu.getWindow(), policyDirectory,
                (userDataRoot / "general-interaction").string(),
                false, false, runtimeStatePath);
            if (!simulation.run()) {
                break;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Unable to start Animal Agent: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
