#pragma once

#include <string>

bool runTrainingSelfTests();

bool runIdlePolicyDiagnostics(const std::string& policyDirectory,
                              const std::string& outputDirectory,
                              unsigned int seed);

bool runTrainingExperiment(const std::string& mode,
                          int episodes,
                          int steps,
                          unsigned int seed,
                          const std::string& outputDirectory,
                          bool resumeTraining = false);
