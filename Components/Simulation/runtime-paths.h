#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace runtime_paths {
inline std::filesystem::path executableDirectory(const char* argv0 = nullptr) {
#ifdef _WIN32
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
        return std::filesystem::path(std::string(buffer, length)).parent_path();
#else
    char buffer[4096] = {};
    const auto length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length > 0)
        return std::filesystem::path(std::string(buffer, static_cast<std::size_t>(length))).parent_path();
#endif
    if (argv0 && *argv0) {
        std::error_code ec;
        auto path = std::filesystem::absolute(argv0, ec);
        if (!ec) return path.parent_path();
    }
    return std::filesystem::current_path();
}

inline std::filesystem::path userDataDirectory() {
#ifdef _WIN32
    if (const char* local = std::getenv("LOCALAPPDATA"); local && *local)
        return std::filesystem::path(local) / "AnimalAgent";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
        return std::filesystem::path(xdg) / "animal-agent";
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / ".local" / "share" / "animal-agent";
#endif
    return std::filesystem::temp_directory_path() / "AnimalAgent";
}
} // namespace runtime_paths
