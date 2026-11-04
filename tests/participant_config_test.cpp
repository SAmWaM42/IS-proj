#include "Components/Study/participant-config.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
study::ParticipantConfig parse(const std::string& source) {
    std::istringstream input(source);
    return study::ParticipantConfig::parse(input);
}

void validParticipantConfiguration() {
    const auto odd = parse(R"({"participant_number":1,"token":"token-one"})");
    assert(odd.participantNumber == 1);
    assert(odd.token == "token-one");
    assert(odd.participantCode() == "P01");
    assert(!odd.conditionAIsProposed());

    const auto even = parse(R"({"participant_number":2,"token":"token-two"})");
    assert(even.participantCode() == "P02");
    assert(even.conditionAIsProposed());

    const auto larger = parse(R"({"participant_number":100,"token":"token-100"})");
    assert(larger.participantCode() == "P100");
    assert(!larger.conditionAIsProposed());
}

void invalidCredentialsAreRejected() {
    const char* invalid[] = {
        "not json",
        "[]",
        R"({"token":"x"})",
        R"({"participant_number":"1","token":"x"})",
        R"({"participant_number":0,"token":"x"})",
        R"({"participant_number":10000,"token":"x"})",
        R"({"participant_number":1,"token":""})",
        R"({"participant_number":1,"token":"   "})",
        R"({"participant_number":1,"token":42})"
    };

    for (const char* document : invalid) {
        bool rejected = false;
        try {
            (void)parse(document);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
}
}

int main() {
    validParticipantConfiguration();
    invalidCredentialsAreRejected();
    std::cout << "[PASS] participant credentials parsing and A/B allocation\n";
}
