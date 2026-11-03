#include "core/ConfigManager.h"

#include <iostream>
#include <string>

#ifdef _WIN32
#include "audio/MiniaudioCapture.h"
#include "audio/TonePlayer.h"
#include "core/AppContextResolver.h"
#include "core/CommandModeController.h"
#include "core/DictationCleanup.h"
#include "core/DictationFSM.h"
#include "core/EngineTypes.h"
#include "core/EventQueue.h"
#include "core/HistoryStore.h"
#include "core/HotkeyBinding.h"
#include "core/HotkeyTracker.h"
#include "core/ILlmProvider.h"
#include "core/IInferenceProvider.h"
#include "core/InjectionException.h"
#include "core/ITextInjector.h"
#include "core/Log.h"
#include "core/PersonalDictionary.h"
#include "core/SettingsStore.h"
#include "core/SnippetStore.h"
#include "core/SpokenCommandProcessor.h"
#include "core/TextPipeline.h"
#include "core/AppVersion.h"
#include "core/CrashReporter.h"
#include "core/UiBus.h"
#include "core/UnavailableProviders.h"
#include "llm/LocalLlamaProvider.h"
#include "os/ActiveWindow.h"
#include "os/CrashHandler.h"
#include "os/DataDir.h"
#include "os/SingleInstance.h"
#include "os/WindowsHotkeyListener.h"
#include "os/WindowsTextInjector.h"
#include "os/WindowsTray.h"
#include "stt/GrpcInferenceProvider.h"
#include "stt/LocalWhisperProvider.h"
#include "ui/MainWindow.h"
#include "ui/OnboardingWizard.h"
#include "ui/Overlay.h"
#include "ui/SettingsWindow.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kSampleRate = 16000;

class EngineBridge final : public IEngineSink {
public:
    EngineBridge(UiBus& bus, Overlay& overlay, WindowsTray& tray, HistoryStore& history)
        : bus_(bus), overlay_(overlay), tray_(tray), history_(history) {}

    void OnStatus(const std::string& message) override {
        lwf::Log::Info("engine: " + message);
        if (message.find("cancelled") != std::string::npos) {
            overlay_.SetState(OverlayModel::PillState::Cancelled, {});
        } else if (message.find("select some text") != std::string::npos) {
            overlay_.SetState(OverlayModel::PillState::Error, message);
        }
    }

    void OnError(ErrorCode code, const std::string& detail) override {
        lwf::Log::Error(std::string(ToString(code)) + ": " + detail);
        overlay_.SetState(OverlayModel::PillState::Error, UserMessage(code));
        tray_.NotifyError(code, detail);
    }

    void RecordHistory(const HistoryEntry& entry) { history_.Append(entry); }

private:
    UiBus& bus_;
    Overlay& overlay_;
    WindowsTray& tray_;
    HistoryStore& history_;
};

class LastTranscriptBox {
public:
    void Set(std::string value) {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = std::move(value);
    }

    std::string Get() {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_;
    }

private:
    std::mutex mutex_;
    std::string value_;
};

class PassthroughLlm final : public ILlmProvider {
public:
    std::string Process(const std::string& text, const std::string&) override { return text; }
};

class LocalProviderStack final : public IInferenceProvider {
public:
    LocalProviderStack(IInferenceProvider& stt, ILlmProvider& llm) : stt_(stt), llm_(llm) {}

    std::string Transcribe(const std::vector<float>& audio_pcm) override {
        return stt_.Transcribe(audio_pcm);
    }

    std::string ProcessText(const std::string& text, const std::string& system_prompt) override {
        return llm_.Process(text, system_prompt);
    }

private:
    IInferenceProvider& stt_;
    ILlmProvider& llm_;
};

class RouterLlm final : public ILlmProvider {
public:
    explicit RouterLlm(IInferenceProvider& provider) : provider_(provider) {}

    std::string Process(const std::string& text, const std::string& system_prompt) override {
        return provider_.ProcessText(text, system_prompt);
    }

private:
    IInferenceProvider& provider_;
};

std::string TrimCopy(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

void RunEngine(MiniaudioCapture& audio, IInferenceProvider& stt, ITextInjector& injector,
               DictationFSM& fsm, EventQueue<EngineEvent>& queue,
               LastTranscriptBox& last_transcript, const std::atomic<bool>& quit) {
    constexpr std::size_t kMinSamples = kSampleRate / 10;
    while (!quit.load()) {
        std::optional<EngineEvent> event = queue.PopFor(200);
        if (!event) {
            continue;
        }
        if (event->type == EngineEventType::PttPress) {
            if (fsm.GetCurrentState() != State::IDLE) {
                continue;
            }
            if (!fsm.TriggerEvent(Trigger::HOTKEY_PRESS)) {
                continue;
            }
            if (!fsm.TriggerEvent(Trigger::VAD_SPEECH)) {
                fsm.TriggerEvent(Trigger::HOTKEY_RELEASE);
                continue;
            }
            audio.Start();
            std::cout << "[mic] Recording... release the hotkey to transcribe" << std::endl;
        }
        if (event->type == EngineEventType::PttRelease) {
            if (fsm.GetCurrentState() != State::CAPTURING) {
                continue;
            }
            audio.Stop();
            const std::vector<float> pcm = audio.GetRecordedAudio();
            fsm.TriggerEvent(Trigger::HOTKEY_RELEASE);

            bool failed = false;
            std::string text;
            if (pcm.size() < kMinSamples) {
                std::cout << "[mic] Tap shorter than 100 ms ignored" << std::endl;
                failed = true;
            } else {
                std::cout << "[stt] Transcribing..." << std::endl;
                try {
                    text = TrimCopy(stt.Transcribe(pcm));
                } catch (const std::exception& e) {
                    std::cerr << "[stt] Error: " << e.what() << '\n';
                    failed = true;
                }
            }

            if (failed) {
                fsm.TriggerEvent(Trigger::SST_FAILED);
                continue;
            }
            if (text.empty()) {
                std::cout << "[stt] Nothing heard" << std::endl;
                fsm.TriggerEvent(Trigger::SST_FAILED);
                continue;
            }

            fsm.TriggerEvent(Trigger::SST_COMPLETE);
            const std::string processed = TrimCopy(stt.ProcessText(text, ""));
            fsm.TriggerEvent(Trigger::SST_COMPLETE);

            try {
                injector.InjectText(processed);
            } catch (const std::exception& e) {
                std::cerr << "[inject] Error: " << e.what() << '\n';
                fsm.TriggerEvent(Trigger::SST_FAILED);
                continue;
            }
            last_transcript.Set(processed);
            fsm.TriggerEvent(Trigger::SST_COMPLETE);
        }
    }
}

}  // namespace

int main() {
    try {
        SingleInstance instance("LwfSingleInstanceMutex");
        if (!instance.IsFirstInstance()) {
            instance.SignalOpenSettings();
            std::cout << "Another instance is already running." << std::endl;
            return 0;
        }

        const std::string data_dir = GetDataDir();
        lwf::Log::Init(data_dir + "\\logs");
        lwf::Log::Info(std::string("Local Wisper Flow ") + lwf::AppVersion() +
                       " starting, data dir " + data_dir);

        static std::atomic<int> g_fsm_diag{0};
        lwf::CrashHandler::Install(data_dir + "\\logs", &g_fsm_diag);

        const std::string config_path = data_dir + "\\config.json";
        if (!std::filesystem::exists(config_path)) {
            const std::string exe_side = GetExeDir() + "\\config.json";
            std::error_code import_ec;
            if (std::filesystem::exists(exe_side)) {
                std::filesystem::copy_file(exe_side, config_path,
                                           std::filesystem::copy_options::overwrite_existing,
                                           import_ec);
            }
        }
        ConfigManager config(config_path);
        for (const std::string& clamped : config.Store().ClampedOnLastLoad()) {
            lwf::Log::Warning(std::string("config value clamped: ") + clamped);
        }
        config.Store().SetBool("first_run", config.Store().GetBool("first_run"));

        UiBus ui_bus;
        Overlay overlay;
        SettingsWindow settings_window(config.Store(), ui_bus);
        OnboardingWizard wizard(config.Store());
        WindowsTray tray(ui_bus, config.Store());

        std::vector<std::pair<ErrorCode, std::string>> startup_errors;
        if (config.Store().CorruptOnLastLoad()) {
            lwf::Log::Error("config was corrupt; defaults loaded");
            startup_errors.emplace_back(
                ErrorCode::ConfigCorrupt,
                "config.json was corrupt and has been moved aside; defaults loaded");
        }

        MiniaudioCapture audio(kSampleRate, 1);
        std::unique_ptr<LocalWhisperProvider> local_stt;
        try {
            local_stt = std::make_unique<LocalWhisperProvider>(
                ResolveModelPath(config.WhisperModelPath()));
        } catch (const std::bad_alloc&) {
            lwf::Log::Error("whisper model load ran out of memory");
            startup_errors.emplace_back(
                ErrorCode::ModelLoadFailed,
                "not enough memory to load the speech model; pick a smaller model in Settings");
        } catch (const std::exception& e) {
            lwf::Log::Error(std::string("whisper model failed to load: ") + e.what());
            startup_errors.emplace_back(ErrorCode::ModelMissing,
                                        "speech model is missing; open Settings > Speech");
        }
        UnavailableStt unavailable_stt;
        IInferenceProvider& stt = local_stt ? static_cast<IInferenceProvider&>(*local_stt)
                                            : static_cast<IInferenceProvider&>(unavailable_stt);
        WindowsTextInjector injector;

        AppContextResolver app_context;
        if (!app_context.Load(data_dir + "\\app_profiles.json")) {
            app_context.UseBuiltInProfiles();
        }
        injector.SetPasteChordProvider([&app_context] {
            IActiveWindow* active = CreateActiveWindow();
            const ActiveWindowInfo info = active->Get();
            return app_context.Resolve(info.process_name, info.window_title).paste_chord;
        });
        injector.SetInjectDelayMs(config.Store().GetInt("inject_delay_ms"));

        PassthroughLlm passthrough_llm;
        std::optional<LocalLlamaProvider> llama_provider;
        ILlmProvider* llm = &passthrough_llm;
        bool cleanup_llm_available = false;
        const std::string llm_model_path = ResolveModelPath(config.LlmModelPath());
        if (!llm_model_path.empty()) {
            try {
                llama_provider.emplace(llm_model_path);
                llm = &*llama_provider;
                cleanup_llm_available = true;
                lwf::Log::Info("llm model loaded: " + llm_model_path);
            } catch (const std::bad_alloc&) {
                lwf::Log::Error("llm model load ran out of memory");
                startup_errors.emplace_back(
                    ErrorCode::ModelLoadFailed,
                    "not enough memory to load the language model; pick a smaller model in Settings");
            } catch (const std::exception& e) {
                lwf::Log::Warning(std::string("llm disabled: ") + e.what());
                startup_errors.emplace_back(ErrorCode::ModelMissing,
                                            "language model is missing; cleanup is disabled");
            }
        }

        PersonalDictionary dictionary;
        dictionary.Load(data_dir + "\\dictionary.json");
        SnippetStore snippets;
        if (!snippets.Load(data_dir + "\\snippets.json")) {
            snippets.SetEntries({});
        }
        snippets.SetMode(config.Store().GetString("snippet_mode"));
        SpokenCommandProcessor spoken;
        spoken.SetAggressive(config.Store().GetBool("spoken_commands_aggressive"));

        DictationCleanup cleanup(llm);
        if (!cleanup_llm_available) {
            cleanup.SetEnabled(false);
        } else {
            cleanup.SetEnabled(config.Store().GetBool("cleanup_enabled"));
        }
        cleanup.SetMinWords(config.Store().GetInt("cleanup_min_words"));
        cleanup.SetTimeoutMs(config.Store().GetInt("cleanup_timeout_ms"));
        TextPipeline pipeline(cleanup);
        pipeline.SetSpokenCommands(&spoken);
        pipeline.SetDictionary(&dictionary);
        pipeline.SetSnippets(&snippets);

        HistoryStore history(data_dir + "\\history.jsonl",
                             config.Store().GetBool("history_enabled"),
                             config.Store().GetBool("keep_raw_in_history"),
                             config.Store().GetInt("history_max_entries"),
                             config.Store().GetInt("history_max_age_days"));

        EngineBridge bridge(ui_bus, overlay, tray, history);
        for (const auto& notice : startup_errors) {
            bridge.OnError(notice.first, notice.second);
        }
        const std::string unseen_crash = lwf::CrashReporter::FindUnseenCrash(data_dir + "\\logs");
        if (!unseen_crash.empty()) {
            bridge.OnError(ErrorCode::Unknown, "The app closed unexpectedly last time - see logs");
            lwf::CrashReporter::MarkSeen(unseen_crash);
        }

        TonePlayer tone_player;
        WindowsSelectionCapture selection_capture;
        CommandModeController command_controller(selection_capture, llm, injector);
        lwf::EngineSettings settings;
        settings.min_hold_ms = config.Store().GetInt("min_hold_ms");
        settings.max_recording_ms = config.Store().GetInt("max_recording_ms");
        settings.hands_free_silence_ms = config.Store().GetInt("hands_free_silence_ms");
        settings.quiet_speech_boost = config.Store().GetBool("quiet_speech_boost");
        settings.sound_enabled = config.Store().GetBool("sound_enabled");
        settings.sound_volume = config.Store().GetDouble("sound_volume");
        settings.streaming_stt = config.Store().GetBool("streaming_stt");
        settings.streaming_min_segment_ms = config.Store().GetInt("streaming_min_segment_ms");
        settings.streaming_pause_ms = config.Store().GetInt("streaming_pause_ms");

        DictationFSM fsm;
        fsm.SetTransitionListener([](State, Trigger, State current) {
            g_fsm_diag.store(static_cast<int>(current), std::memory_order_relaxed);
        });
        RealClock clock;
        std::unique_ptr<SileroVadProvider> vad_provider;
        try {
            vad_provider = std::make_unique<SileroVadProvider>(
                data_dir + "\\models\\silero_vad.onnx",
                static_cast<float>(config.VadThreshold()));
        } catch (const std::exception& e) {
            lwf::Log::Error(std::string("vad model failed to load; running without silence gating: ") +
                            e.what());
            bridge.OnError(ErrorCode::ModelMissing,
                           "voice activity model is missing; running without silence gating");
        }
        DictationEngine engine(audio, vad_provider.get(), stt, pipeline, injector, fsm, clock,
                               bridge, settings);
        engine.SetToneFeedback(&tone_player);
        engine.SetCommandController(&command_controller);

        std::vector<Binding> hotkey_bindings = LoadBindings(config.Store());
        HotkeyTracker::Options hotkey_options;
        hotkey_options.avoid_altgr_conflict = config.Store().GetBool("avoid_altgr_conflict");
        hotkey_options.double_tap_window_ms = config.Store().GetInt("double_tap_window_ms");
        WindowsHotkeyListener hotkey(hotkey_bindings, hotkey_options);
        hotkey.Start();

        tray.Start();

        MainWindow dashboard(fsm, audio);
        if (!dashboard.Init()) {
            lwf::Log::Error("dashboard init failed");
            tray.Stop();
            hotkey.Stop();
            return 1;
        }

        lwf::Log::Info("ready: hold the hotkey and speak");
        overlay.Init();
        settings_window.Init();
        if (wizard.IsNeeded()) {
            wizard.Open();
            settings_window.Open();
        }

        std::atomic<bool> quit{false};
        while (!quit.load() && !dashboard.ShouldClose()) {
            UiCommand command;
            while (ui_bus.PollCommand(command)) {
                switch (command.type) {
                    case UiCommandType::OpenSettings:
                        settings_window.Open();
                        break;
                    case UiCommandType::OpenHistory:
                    case UiCommandType::OpenLogsFolder:
                        ShellExecuteA(nullptr, "open", (data_dir + "\\logs").c_str(), nullptr,
                                      nullptr, SW_SHOWNORMAL);
                        break;
                    case UiCommandType::SetEnabled:
                        config.Store().SetBool("dictation_enabled", command.payload == "1");
                        break;
                    case UiCommandType::StartLearnHotkey:
                        hotkey.BeginLearnMode();
                        break;
                    case UiCommandType::ApplySettings:
                        config.Save();
                        hotkey.UpdateBindings(LoadBindings(config.Store()));
                        settings.min_hold_ms = config.Store().GetInt("min_hold_ms");
                        settings.quiet_speech_boost = config.Store().GetBool("quiet_speech_boost");
                        settings.sound_enabled = config.Store().GetBool("sound_enabled");
                        settings.streaming_stt = config.Store().GetBool("streaming_stt");
                        settings.streaming_min_segment_ms = config.Store().GetInt("streaming_min_segment_ms");
                        settings.streaming_pause_ms = config.Store().GetInt("streaming_pause_ms");
                        spoken.SetAggressive(config.Store().GetBool("spoken_commands_aggressive"));
                        snippets.SetMode(config.Store().GetString("snippet_mode"));
                        break;
                    case UiCommandType::ToggleAutostart:
                        Autostart::SetEnabled(!Autostart::IsEnabled());
                        break;
                    case UiCommandType::Quit:
                        quit.store(true);
                        break;
                }
            }

            std::string learned;
            if (hotkey.TryTakeLearnedBinding(learned)) {
                config.Store().SetString("hotkey_ptt", learned);
                config.Save();
                hotkey.UpdateBindings(LoadBindings(config.Store()));
                wizard.SetDetectedHotkey(learned);
            }

            EngineEvent event;
            while (hotkey.PollEvent(event)) {
                engine.PostEvent(event.type);
            }

            const State state = fsm.GetCurrentState();
            hotkey.SetCancelArmed(state == State::CAPTURING || state == State::TRANSCRIBING);

            StatusSnapshotValue status;
            status.state = static_cast<int>(state);
            status.audio_level = audio.AudioLevel();
            status.mode = engine.LastText().empty() ? 0 : 1;
            status.enabled = config.Store().GetBool("dictation_enabled");
            ui_bus.PublishStatus(status);
            overlay.PushLevel(audio.AudioLevel());
            wizard.SetMicLevel(audio.AudioLevel());

            dashboard.RunOnce();
            overlay.RenderFrame(1.0f / 60.0f);
            settings_window.RenderFrame();
            wizard.RenderFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }

        engine.Stop();
        hotkey.Stop();
        tray.Stop();
        tone_player.Shutdown();
        overlay.Shutdown();
        settings_window.Shutdown();
        lwf::Log::Info("shutdown complete");
        return 0;
    } catch (const std::exception& e) {
        lwf::Log::Error(std::string("fatal: ") + e.what());
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
}

#else

int main() {
    try {
        ConfigManager config("config.json");
        std::cout << "Local Wisper Flow core build: hotkey=" << config.HotkeyCode()
                  << " model=" << config.WhisperModelPath() << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
}

#endif
