#pragma once

#include <string>

bool runTrainingExperiment(const std::string& mode,
                          int episodes,
                          int steps,
                          unsigned int seed,
                          const std::string& outputDirectory);
