#include "participant-config.h"

#include <json.hpp>

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace study {
namespace {
using nlohmann::json;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}
}

ParticipantConfig ParticipantConfig::parse(std::istream& input) {
    json document;
    try {
        input >> document;
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("credentials.json is not valid JSON: ") + error.what());
    }

    if (!document.is_object())
        throw std::runtime_error("credentials.json must contain a JSON object.");

    if (!document.contains("participant_number") ||
        !document.at("participant_number").is_number_integer())
        throw std::runtime_error("credentials.json must contain an integer participant_number.");

    const auto& numberValue = document.at("participant_number");
    std::int64_t number = 0;
    if (numberValue.is_number_unsigned()) {
        const auto unsignedNumber = numberValue.get<std::uint64_t>();
        if (unsignedNumber > 9999)
            throw std::runtime_error("participant_number must be between 1 and 9999.");
        number = static_cast<std::int64_t>(unsignedNumber);
    } else {
        number = numberValue.get<std::int64_t>();
    }
    if (number < 1 || number > 9999)
        throw std::runtime_error("participant_number must be between 1 and 9999.");

    if (!document.contains("token") || !document.at("token").is_string())
        throw std::runtime_error("credentials.json must contain a string token.");

    const std::string tokenValue = trim(document.at("token").get<std::string>());
    if (tokenValue.empty())
        throw std::runtime_error("The participant upload token in credentials.json is empty.");

    return ParticipantConfig{static_cast<int>(number), tokenValue};
}

ParticipantConfig ParticipantConfig::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Could not open " + path.string() +
                                 ". Place the participant's credentials.json beside AnimalAgent.");
    return parse(input);
}

std::string ParticipantConfig::participantCode() const {
    std::ostringstream output;
    output << 'P' << std::setfill('0') << std::setw(2) << participantNumber;
    return output.str();
}

bool ParticipantConfig::conditionAIsProposed() const noexcept {
    return participantNumber % 2 == 0;
}

} // namespace study
