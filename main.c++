#include <iostream>

#include "Components/State-Action/state-action.h"

int main() {
    if (!runBaselineValidation()) {
        return 1;
    }

    std::cout << "Baseline learning loop validation passed." << std::endl;
    return 0;
}
