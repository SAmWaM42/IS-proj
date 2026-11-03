#include "submission-manager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
std::string submissionId(int participantNumber, int sessionIndex) {
    std::ostringstream id;
    id << 'P' << std::setw(2) << std::setfill('0') << participantNumber
       << "-S" << sessionIndex;
    return id.str();
}

#ifdef _WIN32
std::string quoteWindowsArgument(const std::string& value) {
    std::string quoted = "\"" ;
    std::size_t backslashes = 0;
    for (char ch : value) {
        if (ch == '\\') {
            ++backslashes;
        } else if (ch == '"') {
            quoted.append(backslashes * 2 + 1, '\\');
            quoted.push_back('"');
            backslashes = 0;
        } else {
            quoted.append(backslashes, '\\');
            backslashes = 0;
            quoted.push_back(ch);
        }
    }
    quoted.append(backslashes * 2, '\\');
    quoted.push_back('"');
    return quoted;
}
#endif
} // namespace

SubmissionManager::SubmissionManager(std::filesystem::path userDataDirectory,
                                     std::filesystem::path applicationDirectory,
                                     std::filesystem::path pythonExecutable)
    : userDataDirectory_(std::move(userDataDirectory)),
      applicationDirectory_(std::move(applicationDirectory)),
      pythonExecutable_(std::move(pythonExecutable)),
      pendingDirectory_(userDataDirectory_ / "pending-submissions") {
    std::filesystem::create_directories(pendingDirectory_);
}

std::filesystem::path SubmissionManager::pendingDirectory() const {
    return pendingDirectory_;
}

bool SubmissionManager::writeAtomically(const std::filesystem::path& destination,
                                        const std::string& contents) {
    const auto temporary = destination.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.flush();
        if (!output.good()) return false;
    }

    std::error_code error;
#ifdef _WIN32
    // std::filesystem::rename does not replace an existing destination on Windows.
    std::filesystem::remove(destination, error);
    error.clear();
#endif
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary);
        return false;
    }
    return true;
}

bool SubmissionManager::queueSubmission(int participantNumber, int sessionIndex,
                                        const nlohmann::json& responses) {
    if (participantNumber < 1 || participantNumber > 9999 ||
        (sessionIndex != 1 && sessionIndex != 2) || !responses.is_object()) {
        return false;
    }

    const std::string id = submissionId(participantNumber, sessionIndex);
    nlohmann::json record = responses;
    record["schema_version"] = 1;
    record["submission_id"] = id;
    record["participant_number"] = participantNumber;
    record["session_index"] = sessionIndex;

    const auto destination = pendingDirectory_ / (id + ".json");
    // A repeated submit for the same participant/session must not silently
    // overwrite the original response or create a second queued record.
    if (std::filesystem::exists(destination)) return false;

    if (!writeAtomically(destination, record.dump(2) + "\n")) return false;

    // Best effort immediate delivery. If it fails, the durable queue entry stays.
    uploadOne(destination);
    return true;
}

bool SubmissionManager::uploadOne(const std::filesystem::path& responseFile) const {
    const auto uploader = applicationDirectory_ / "Components" / "NLP" / "upload_submission.py";
    const auto config = applicationDirectory_ / "Data" / "Study" / "upload-config.json";
    if (!std::filesystem::exists(uploader) || !std::filesystem::exists(config) ||
        !std::filesystem::exists(pythonExecutable_)) {
        return false;
    }

#ifdef _WIN32
    std::string command = quoteWindowsArgument(pythonExecutable_.string()) + " " +
                          quoteWindowsArgument(uploader.string()) + " " +
                          quoteWindowsArgument(config.string()) + " " +
                          quoteWindowsArgument(responseFile.string());
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');
    if (!CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, applicationDirectory_.string().c_str(),
                        &startup, &process)) {
        return false;
    }
    CloseHandle(process.hThread);
    const DWORD waitResult = WaitForSingleObject(process.hProcess, 20000);
    DWORD exitCode = 1;
    if (waitResult == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &exitCode);
    else TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hProcess);
    return waitResult == WAIT_OBJECT_0 && exitCode == 0;
#else
    const pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        chdir(applicationDirectory_.c_str());
        execl(pythonExecutable_.c_str(), pythonExecutable_.c_str(),
              uploader.c_str(), config.c_str(), responseFile.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

SubmissionUploadSummary SubmissionManager::uploadPendingSubmissions() {
    SubmissionUploadSummary summary;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(pendingDirectory_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        ++summary.attempted;
        if (uploadOne(file)) {
            std::error_code error;
            std::filesystem::remove(file, error);
            if (!error) ++summary.uploaded;
            else ++summary.pending;
        } else {
            ++summary.pending;
        }
    }
    return summary;
}
