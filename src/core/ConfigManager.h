#pragma once

#include <mutex>
#include <string>
#include <vector>

class ConfigManager {
public:
    explicit ConfigManager(std::string path = "config.json");

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void Reload();

    int HotkeyCode();
    std::string WhisperModelPath();
    float VadThreshold();
    std::string LlmModelPath();
    bool UseRemote();
    std::string RemoteEndpoint();
    bool CleanupEnabled();
    int CleanupMinWords();
    int CleanupTimeoutMs();
    int MinHoldMs();
    int HandsFreeSilenceMs();
    int MaxRecordingMs();
    int LlmContextSize();
    int LlmMaxPredictTokens();
    int LlmGpuLayers();
    std::string VadModelPath();
    int InjectRestoreDelayMs();
    std::string WhisperLanguage();
    int WhisperThreads();
    int WhisperBeamSize();
    bool WhisperDisableTempFallback();
    bool AvoidAltGr();
    std::string DictionaryPath();
    std::string SttProvider();
    bool SttConsentAccepted();
    std::string SttLanguage();
    int SttTimeoutMs();
    int SttMaxClips();

    const std::vector<std::string>& Warnings() const;

private:
    struct Data {
        int hotkey_code = 0;
        std::string whisper_model_path;
        float vad_threshold = 0.0f;
        std::string llm_model_path;
        bool use_remote = false;
        std::string remote_endpoint = "localhost:50051";
        bool cleanup_enabled = true;
        int cleanup_min_words = 4;
        int cleanup_timeout_ms = 4000;
        int min_hold_ms = 100;
        int hands_free_silence_ms = 0;
        int max_recording_ms = 30000;
        int llm_context_size = 2048;
        int llm_max_predict_tokens = 1024;
        int llm_gpu_layers = 0;
        std::string vad_model_path = "models/silero_vad.onnx";
        int inject_restore_delay_ms = 300;
        std::string whisper_language = "auto";
        int whisper_threads = 4;
        int whisper_beam_size = 1;
        bool whisper_disable_temp_fallback = false;
        bool avoid_altgr = true;
        std::string dictionary_path = "dictionary.json";
        std::string stt_provider = "local";
        bool stt_consent_accepted = false;
        std::string stt_language = "indic-auto";
        int stt_timeout_ms = 15000;
        int stt_max_clips = 40;
    };

    Data ParseFile(const std::string& path);

    std::string path_;
    std::mutex mutex_;
    Data data_;
    std::vector<std::string> warnings_;
};
