#pragma once

#include <string>
#include <vector>

bool runTrainingSelfTests();

bool runTemporalStimulusDiagnostics(const std::string& policyDirectory,
                                    const std::string& outputDirectory,
                                    unsigned int seed);

bool runIdlePolicyDiagnostics(const std::string& policyDirectory,
                              const std::string& outputDirectory,
                              unsigned int seed);

bool runTrainingExperiment(const std::string& mode,
                          int episodes,
                          int steps,
                          unsigned int seed,
                          const std::string& outputDirectory,
                          bool resumeTraining = false,
                          const std::vector<std::string>& commandArguments = {});
