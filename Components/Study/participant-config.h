#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>

namespace study {

struct ParticipantConfig {
    int participantNumber = 0;
    std::string token;

    // Parse the private credentials document. Throws std::runtime_error when
    // the document is malformed or contains an invalid participant identity.
    static ParticipantConfig parse(std::istream& input);
    static ParticipantConfig load(const std::filesystem::path& path);

    std::string participantCode() const;
    bool conditionAIsProposed() const noexcept;
};

} // namespace study
