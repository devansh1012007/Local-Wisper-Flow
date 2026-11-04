#include "core/Log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>
#include <filesystem>
#include <mutex>
#include <vector>

namespace lwf {

struct Log::State {
    std::mutex mutex;
    std::string directory;
    std::string file_path;
    LogLevel level = LogLevel::Info;
    bool transcript_logging = false;
    std::size_t max_bytes = 1024 * 1024;
    int max_files = 5;
    bool initialized = false;
    std::mutex ring_mutex;
    std::deque<std::string> recent_lines;
};

Log::State& Log::S() {
    static State state;
    return state;
}

void Log::Init(const std::string& directory) {
    State& state = S();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.directory = directory;
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    state.file_path = (std::filesystem::path(directory) / "lwf.log").string();
    state.initialized = true;
}

void Log::SetLevel(LogLevel level) {
    State& state = S();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.level = level;
}

void Log::SetTranscriptLogging(bool enabled) {
    State& state = S();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.transcript_logging = enabled;
}

bool Log::TranscriptLogging() {
    return S().transcript_logging;
}

void Log::SetRotationLimits(std::size_t max_bytes, int max_files) {
    State& state = S();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.max_bytes = max_bytes;
    state.max_files = max_files;
}

void Log::RotateIfNeeded(State& state) {
    std::error_code ec;
    if (!std::filesystem::exists(state.file_path, ec)) {
        return;
    }
    if (std::filesystem::file_size(state.file_path, ec) < state.max_bytes || ec) {
        return;
    }
    std::filesystem::remove((std::filesystem::path(state.directory) /
                             ("lwf." + std::to_string(state.max_files) + ".log"))
                                .string(),
                            ec);
    for (int i = state.max_files - 1; i >= 1; --i) {
        const std::string from = (std::filesystem::path(state.directory) /
                                  ("lwf." + std::to_string(i) + ".log"))
                                     .string();
        const std::string to = (std::filesystem::path(state.directory) /
                                ("lwf." + std::to_string(i + 1) + ".log"))
                                   .string();
        std::filesystem::rename(from, to, ec);
    }
    std::filesystem::rename(state.file_path,
                            (std::filesystem::path(state.directory) / "lwf.1.log").string(), ec);
}

void Log::Write(LogLevel level, const std::string& tag, const std::string& message) {
    State& state = S();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (static_cast<int>(level) < static_cast<int>(state.level)) {
        return;
    }
    if (!state.initialized) {
        state.directory = std::filesystem::temp_directory_path().string();
        state.file_path = (std::filesystem::path(state.directory) / "lwf.log").string();
        state.initialized = true;
    }
    RotateIfNeeded(state);

    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm);

    FILE* file = std::fopen(state.file_path.c_str(), "a");
    if (file != nullptr) {
        std::fprintf(file, "%s [%s] %s\n", stamp, tag.c_str(), message.c_str());
        std::fclose(file);
    }
    {
        std::lock_guard<std::mutex> ring_lock(state.ring_mutex);
        state.recent_lines.push_back(std::string(stamp) + " [" + tag + "] " + message);
        while (state.recent_lines.size() > 512) {
            state.recent_lines.pop_front();
        }
    }
}

std::vector<std::string> Log::RecentLines(std::size_t max_lines) {
    State& state = S();
    std::lock_guard<std::mutex> lock(state.ring_mutex);
    const std::size_t start =
        state.recent_lines.size() > max_lines ? state.recent_lines.size() - max_lines : 0;
    return std::vector<std::string>(state.recent_lines.begin() +
                                        static_cast<std::ptrdiff_t>(start),
                                    state.recent_lines.end());
}

std::vector<std::string> Log::RecentLinesTry(std::size_t max_lines) {
    State& state = S();
    std::unique_lock<std::mutex> lock(state.ring_mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return {};
    }
    const std::size_t start =
        state.recent_lines.size() > max_lines ? state.recent_lines.size() - max_lines : 0;
    return std::vector<std::string>(state.recent_lines.begin() +
                                        static_cast<std::ptrdiff_t>(start),
                                    state.recent_lines.end());
}

void Log::Debug(const std::string& message) {
    Write(LogLevel::Debug, "DEBUG", message);
}

void Log::Info(const std::string& message) {
    Write(LogLevel::Info, "INFO", message);
}

void Log::Warning(const std::string& message) {
    Write(LogLevel::Warning, "WARN", message);
}

void Log::Error(const std::string& message) {
    Write(LogLevel::Error, "ERROR", message);
}

void Log::Transcript(const std::string& message) {
    State& state = S();
    if (!state.transcript_logging) {
        return;
    }
    Write(LogLevel::Debug, "TRANSCRIPT", message);
}

}
