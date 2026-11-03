#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include <json.hpp>

struct SubmissionUploadSummary {
    std::size_t attempted = 0;
    std::size_t uploaded = 0;
    std::size_t pending = 0;
};

class SubmissionManager {
public:
    SubmissionManager(std::filesystem::path userDataDirectory,
                      std::filesystem::path applicationDirectory,
                      std::filesystem::path pythonExecutable);

    // Persists questionnaire responses and the participant's upload token locally,
    // then attempts delivery. The token is stripped before the HTTP payload is sent.
    bool queueSubmission(int participantNumber, int sessionIndex, char condition,
                         const std::string& participantToken,
                         const nlohmann::json& responses);

    SubmissionUploadSummary uploadPendingSubmissions();
    std::filesystem::path pendingDirectory() const;

private:
    std::filesystem::path userDataDirectory_;
    std::filesystem::path applicationDirectory_;
    std::filesystem::path pythonExecutable_;
    std::filesystem::path pendingDirectory_;

    bool uploadOne(const std::filesystem::path& responseFile) const;
    static bool writeAtomically(const std::filesystem::path& destination,
                                const std::string& contents);
};
