#pragma once

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

    // Persists the response first, then immediately attempts to upload it.
    // The submission key is deterministic: P{participantNumber}-S{sessionIndex}.
    bool queueSubmission(int participantNumber, int sessionIndex,
                         const nlohmann::json& responses);

    // Retries every pending JSON response. Successfully acknowledged files
    // are deleted; failed or ambiguous uploads remain queued.
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
