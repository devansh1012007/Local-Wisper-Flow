#pragma once

#include <string>
#include <vector>

namespace lwf {

enum class LogLevel { Debug, Info, Warning, Error };

class Log {
public:
    Log() = delete;

    static void Init(const std::string& directory);
    static void SetLevel(LogLevel level);
    static void SetTranscriptLogging(bool enabled);
    static bool TranscriptLogging();
    static void SetRotationLimits(std::size_t max_bytes, int max_files);

    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
    static void Transcript(const std::string& message);

    static std::vector<std::string> RecentLines(std::size_t max_lines);
    static std::vector<std::string> RecentLinesTry(std::size_t max_lines);

private:
    struct State;
    static State& S();
    static void Write(LogLevel level, const std::string& tag, const std::string& message);
    static void RotateIfNeeded(State& state);
};

}
