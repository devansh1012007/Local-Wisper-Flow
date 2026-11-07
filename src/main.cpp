#include "core/ConfigManager.h"

#include <iostream>
#include <string>

#ifdef _WIN32
#include "audio/MiniaudioCapture.h"
#include "audio/TonePlayer.h"
#include "core/ApiKeyStore.h"
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
#include "core/IEngineSink.h"
#include "core/IInferenceProvider.h"
#include "core/IVadProvider.h"
#include "core/IClock.h"
#include "core/DictationEngine.h"
#include "core/ToneGenerator.h"
#include "core/InjectionException.h"
#include "core/ITextInjector.h"
#include "core/Log.h"
#include "core/NotesStore.h"
#include "core/PersonalDictionary.h"
#include "core/SettingsStore.h"
#include "core/SnippetStore.h"
#include "core/SpokenCommandProcessor.h"
#include "core/StatsStore.h"
#include "core/TextPipeline.h"
#include "core/TransformStore.h"
#include "core/AppVersion.h"
#include "core/ConsentGate.h"
#include "core/CrashReporter.h"
#include "core/DictionaryStore.h"
#include "core/SttRouter.h"
#include "core/UiBus.h"
#include "core/UnavailableProviders.h"
#include "llm/LocalLlamaProvider.h"
#include "llm/OpenRouterProvider.h"
#include "os/ActiveWindow.h"
#include "os/CrashHandler.h"
#include "os/DataDir.h"
#include "os/SingleInstance.h"
#include "os/WindowsHotkeyListener.h"
#include "os/WindowsTextInjector.h"
#include "os/WindowsTray.h"
#include "stt/cloud/ElevenLabsProvider.h"
#include "stt/cloud/GnaniProvider.h"
#include "stt/cloud/LanguageMap.h"
#include "stt/cloud/MuseProvider.h"
#include "stt/cloud/WindowsHttpClient.h"
#include "stt/GrpcInferenceProvider.h"
#include "stt/LocalWhisperProvider.h"
#include "ui/MainWindow.h"
#include "ui/ScratchpadWindow.h"
#include "ui/Overlay.h"
#include "ui/pages/DictionaryPage.h"
#include "ui/pages/InsightsPage.h"
#include "ui/pages/ScratchpadPage.h"
#include "ui/pages/SnippetsPage.h"
#include "ui/pages/TransformsPage.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall ShellExecuteA(void*, const char*, const char*, const char*, const char*, int);
#endif

namespace {

constexpr std::uint32_t kSampleRate = 16000;

// Single-slot attention message written by the engine thread and drained by
// the main loop. Keeps every UI call on the main thread.
class AttentionBox {
public:
    void Set(std::string message) {
        std::lock_guard<std::mutex> lock(mutex_);
        message_ = std::move(message);
        pending_ = true;
    }

    bool Take(std::string& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pending_) {
            return false;
        }
        pending_ = false;
        out = message_;
        return true;
    }

private:
    std::mutex mutex_;
    std::string message_;
    bool pending_ = false;
};

class EngineBridge final : public IEngineSink {
public:
    EngineBridge(AttentionBox& attention, WindowsTray& tray, HistoryStore& history)
        : attention_(attention), tray_(tray), history_(history) {}

    void OnStatus(const std::string& message) override {
        lwf::Log::Info("engine: " + message);
        if (message.find("select some text") != std::string::npos) {
            attention_.Set(message);
        }
        // A cancelled dictation needs no visual event; the badge simply
        // returns to Ready.
    }

    void OnError(ErrorCode code, const std::string& detail) override {
        lwf::Log::Error(std::string(ToString(code)) + ": " + detail);
        attention_.Set(UserMessage(code));
        tray_.NotifyError(code, detail);
    }

    void RecordHistory(const HistoryEntry& entry) { history_.Append(entry); }

private:
    AttentionBox& attention_;
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

        ApiKeyStore api_keys(data_dir + "\\api_keys.json");
        NotesStore notes(data_dir);

        ScratchpadWindow scratchpad_editor(notes, config.Store());
        if (!scratchpad_editor.Init()) {
            lwf::Log::Warning("scratchpad window init failed; continuing without it");
        }

        UiBus ui_bus;
        AttentionBox attention;
        Overlay overlay;
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
        IInferenceProvider& local_stack =
            local_stt ? static_cast<IInferenceProvider&>(*local_stt)
                      : static_cast<IInferenceProvider&>(unavailable_stt);
        WindowsTextInjector injector;
        lwf::ui::scratchpad::ScratchpadRouteInjector scratchpad_router(
            injector, scratchpad_editor.DictationBuffer(), scratchpad_editor.DictationActiveFlag());

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
        std::optional<OpenRouterProvider> openrouter_provider;
        WindowsHttpClient openrouter_http;
        ILlmProvider* llm = &passthrough_llm;
        bool cleanup_llm_available = false;
        const std::string llm_provider_name = config.Store().GetString("llm_provider");
        if (llm_provider_name == OpenRouterContract::kApiKeyProvider) {
            std::string openrouter_key = api_keys.Get(OpenRouterContract::kApiKeyProvider);
            if (openrouter_key.empty()) {
                const char* env_key = std::getenv(OpenRouterContract::kApiKeyEnv);
                if (env_key != nullptr) {
                    openrouter_key = env_key;
                }
            }
            const std::string openrouter_model = config.Store().GetString("openrouter_model");
            if (openrouter_key.empty() || openrouter_model.empty()) {
                lwf::Log::Warning("llm_provider=openrouter ignored: api key or openrouter_model "
                                  "is missing; cleanup is disabled");
                startup_errors.emplace_back(ErrorCode::ModelMissing,
                                            "openrouter api key or model is missing; cleanup is disabled");
            } else {
                try {
                    OpenRouterSettings openrouter_settings;
                    openrouter_settings.api_key = openrouter_key;
                    openrouter_settings.model = openrouter_model;
                    openrouter_settings.timeout_ms = OpenRouterContract::kDefaultTimeoutMs;
                    openrouter_provider.emplace(openrouter_http, openrouter_settings);
                    llm = &*openrouter_provider;
                    cleanup_llm_available = true;
                    lwf::Log::Info("llm provider openrouter enabled (model " + openrouter_model +
                                   ")");
                } catch (const std::bad_alloc&) {
                    lwf::Log::Error("openrouter provider setup ran out of memory");
                    startup_errors.emplace_back(
                        ErrorCode::ModelLoadFailed,
                        "not enough memory to set up the openrouter provider; cleanup is disabled");
                } catch (const std::exception& e) {
                    lwf::Log::Warning(std::string("llm disabled: ") + e.what());
                    startup_errors.emplace_back(ErrorCode::ModelMissing,
                                                "language model is missing; cleanup is disabled");
                }
            }
        } else {
            if (!llm_provider_name.empty() && llm_provider_name != "local") {
                lwf::Log::Warning("unknown llm_provider '" + llm_provider_name + "'; using local");
            }
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
        }

        PersonalDictionary dictionary;
        dictionary.Load(data_dir + "\\dictionary.json");
        DictionaryStore dictionary_store(data_dir + "\\dictionary.json");
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

        const std::string stt_provider_name = config.SttProvider();
        const bool stt_consent = config.SttConsentAccepted();
        std::unique_ptr<ISttProvider> cloud_provider;
        WindowsHttpClient http_client;
        if (IsKnownCloudProvider(stt_provider_name)) {
            const std::string api_key = ResolveApiKey(stt_provider_name, api_keys);
            SttConsentInputs consent_inputs;
            consent_inputs.provider = stt_provider_name;
            consent_inputs.consent_accepted = stt_consent;
            consent_inputs.api_key_present = !api_key.empty();
            const SttConsentVerdict verdict = EvaluateCloudConsent(consent_inputs);
            if (verdict.decision != SttConsentDecision::CloudAllowed) {
                lwf::Log::Warning("stt_provider=" + stt_provider_name + " ignored: " +
                                  verdict.reason);
            } else if (stt_provider_name == "gnani") {
                GnaniProviderSettings gnani_settings;
                gnani_settings.api_key_id = api_key;
                gnani_settings.language = config.SttLanguage();
                gnani_settings.timeout_ms = config.SttTimeoutMs();
                gnani_settings.max_clips = static_cast<std::size_t>(config.SttMaxClips());
                gnani_settings.keyterms = dictionary.Words();
                cloud_provider = std::make_unique<GnaniProvider>(http_client, gnani_settings);
                lwf::Log::Info("cloud provider gnani enabled (language " + config.SttLanguage() +
                               ")");
            } else if (stt_provider_name == "elevenlabs") {
                ElevenLabsProviderSettings elevenlabs_settings;
                elevenlabs_settings.api_key = api_key;
                elevenlabs_settings.language_code =
                    sttlang::ElevenLabsLanguageCode(config.SttLanguage());
                elevenlabs_settings.timeout_ms = config.SttTimeoutMs();
                elevenlabs_settings.max_clips = static_cast<std::size_t>(config.SttMaxClips());
                elevenlabs_settings.keyterms = dictionary.Words();
                cloud_provider =
                    std::make_unique<ElevenLabsProvider>(http_client, elevenlabs_settings);
                lwf::Log::Info("cloud provider elevenlabs enabled");
            } else if (stt_provider_name == "muse") {
                MuseProviderSettings muse_settings;
                muse_settings.api_key = api_key;
                muse_settings.language_bias = sttlang::MuseLanguageBias(config.SttLanguage());
                muse_settings.keywords = dictionary.Words();
                muse_settings.timeout_ms = config.SttTimeoutMs();
                muse_settings.max_clips = static_cast<std::size_t>(config.SttMaxClips());
                cloud_provider = std::make_unique<MuseProvider>(http_client, muse_settings);
                lwf::Log::Info("cloud provider muse enabled");
            }
        } else if (stt_provider_name != "local") {
            lwf::Log::Warning("unknown stt_provider '" + stt_provider_name + "'; using local");
        }
        SttRoutingConfig stt_routing;
        stt_routing.provider = stt_provider_name;
        stt_routing.consent_accepted = stt_consent;
        stt_routing.api_key_source = [stt_provider_name, &api_keys] {
            return ResolveApiKey(stt_provider_name, api_keys);
        };
        auto local_chain = std::make_unique<LocalProviderStack>(local_stack, passthrough_llm);
        SttRouter stt_router(std::move(local_chain), std::move(cloud_provider), stt_routing);

        HistoryStore history(data_dir + "\\history.jsonl",
                             config.Store().GetBool("history_enabled"),
                             config.Store().GetBool("keep_raw_in_history"),
                             config.Store().GetInt("history_max_entries"),
                             config.Store().GetInt("history_max_age_days"));

        StatsStore usage_stats(data_dir + "\\stats.json", config.Store().GetBool("keep_usage_stats"));
        TransformStore transforms;
        transforms.Load(data_dir + "\\transforms.json");

        EngineBridge bridge(attention, tray, history);
        for (const auto& notice : startup_errors) {
            bridge.OnError(notice.first, notice.second);
        }
        const std::string unseen_crash = lwf::CrashReporter::FindUnseenCrash(data_dir + "\\logs");
        if (!unseen_crash.empty()) {
            bridge.OnError(ErrorCode::Unknown, "The app closed unexpectedly last time - see logs");
            lwf::CrashReporter::MarkSeen(unseen_crash);
        }

        ToneGenerator::TonePlayer tone_player;
        WindowsSelectionCapture selection_capture;
        CommandModeController command_controller(selection_capture, llm, injector);
        EngineSettings settings;
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
        IVadProvider* vad = nullptr;
        DictationEngine engine(audio, vad, stt_router, pipeline, scratchpad_router, fsm,
                               clock, bridge, settings);
        engine.SetToneFeedback(&tone_player);
        engine.SetCommandController(&command_controller);
        engine.SetStatsStore(&usage_stats);
        engine.SetActiveWindow(CreateActiveWindow());
        engine.SetAppContextResolver(&app_context);
        engine.SetScratchpadTarget(false);
        engine.SetTransformStore(&transforms);
        engine.SetTransformLlm(llm);
        engine.SetSelectionCapture(&selection_capture);

        std::vector<Binding> hotkey_bindings = LoadBindings(config.Store());
        transforms.SetHotkeyBindings(hotkey_bindings);
        engine.Start();
        HotkeyTracker::Options hotkey_options;
        hotkey_options.avoid_altgr_conflict = config.Store().GetBool("avoid_altgr_conflict");
        hotkey_options.double_tap_window_ms = config.Store().GetInt("double_tap_window_ms");
        WindowsHotkeyListener hotkey(hotkey_bindings, hotkey_options);
        hotkey.Start();

        tray.Start();

        MainWindow full(fsm, audio, config.Store(), ui_bus);
        full.SetDictionaryPage(
            std::make_unique<lwf::ui::pages::DictionaryPage>(dictionary_store, &dictionary));
        full.SetInsightsPage(std::make_unique<lwf::ui::insights::InsightsPage>(usage_stats));
        lwf::ui::EngineDiffSource transforms_diff_source(&engine);
        full.SetTransformsPage(std::make_unique<lwf::ui::TransformsPage>(transforms, &ui_bus,
                                                                          &transforms_diff_source));
        full.SetScratchpadPage(
            std::make_unique<ScratchpadPage>(notes, config.Store(), ui_bus));
        full.SetSnippetsPage(std::make_unique<lwf::ui::pages::SnippetsPage>(snippets));
        {
            MainWindow::DataResetStores reset_stores;
            reset_stores.dictionary = &dictionary_store;
            reset_stores.snippets = &snippets;
            reset_stores.stats = &usage_stats;
            reset_stores.transforms = &transforms;
            reset_stores.notes = &notes;
            reset_stores.api_keys = &api_keys;
            reset_stores.history = &history;
            reset_stores.dictionary_legacy = &dictionary;
            full.SetDataResetStores(reset_stores);
        }
        if (!full.Init()) {
            lwf::Log::Error("full window init failed");
            tray.Stop();
            hotkey.Stop();
            return 1;
        }

        lwf::Log::Info("ready: hold the hotkey and speak");
        // Restore the mini badge position the user last dragged it to
        // (negative sentinel = never moved: default anchor above the tray).
        overlay.SetSavedPosition(config.Store().GetInt("mini_x"),
                                 config.Store().GetInt("mini_y"));
        overlay.SetPositionPersistHandler([&config](int x, int y) {
            config.Store().SetInt("mini_x", x);
            config.Store().SetInt("mini_y", y);
            config.Save();
        });
        if (overlay.Init()) {
            overlay.SetReduceMotion(config.Store().GetBool("reduce_motion"));
            overlay.SetActivateHandler([&ui_bus] {
                ui_bus.PostCommand(UiCommandType::SetEnabled, "1");
            });
            overlay.SetScratchpadVisible(config.Store().GetBool("scratchpad_flow_bar"));
            overlay.SetScratchpadActivateHandler([&ui_bus] {
                ui_bus.PostCommand(UiCommandType::OpenScratchpad);
            });
        } else {
            lwf::Log::Error("mini badge init failed; continuing without it");
        }
        if (full.FirstRunNeeded()) {
            full.SetVisible(true);
        }

        std::atomic<bool> quit{false};
        State previous_state = State::IDLE;
        auto done_flash_until = std::chrono::steady_clock::time_point{};
        const auto done_flash_duration = std::chrono::milliseconds(600);
        while (!quit.load()) {
            glfwPollEvents();

            UiCommand command;
            while (ui_bus.PollCommand(command)) {
                switch (command.type) {
                    case UiCommandType::OpenSettings:
                        full.SetVisible(true);
                        break;
                    case UiCommandType::OpenHistory:
                    case UiCommandType::OpenLogsFolder:
                        ShellExecuteA(nullptr, "open", (data_dir + "\\logs").c_str(), nullptr,
                                      nullptr, 1);
                        break;
                    case UiCommandType::SetEnabled:
                        config.Store().SetBool("dictation_enabled", command.payload == "1");
                        break;
                    case UiCommandType::StartLearnHotkey: {
                        const std::string target = UiBus::ParseStartLearnTarget(command.payload);
                        if (HotkeyBinding::IsConfigHotkeyKey(target)) {
                            hotkey.BeginLearnMode(target);
                        } else {
                            lwf::Log::Warning("learn: unknown target ignored");
                        }
                        break;
                    }
                    case UiCommandType::ApplySettings:
                        config.Save();
                        {
                            const std::vector<Binding> bindings = LoadBindings(config.Store());
                            hotkey.UpdateBindings(bindings);
                            transforms.SetHotkeyBindings(bindings);
                        }
                        settings.min_hold_ms = config.Store().GetInt("min_hold_ms");
                        settings.quiet_speech_boost = config.Store().GetBool("quiet_speech_boost");
                        settings.sound_enabled = config.Store().GetBool("sound_enabled");
                        settings.streaming_stt = config.Store().GetBool("streaming_stt");
                        settings.streaming_min_segment_ms = config.Store().GetInt("streaming_min_segment_ms");
                        settings.streaming_pause_ms = config.Store().GetInt("streaming_pause_ms");
                        engine.SetSettings(settings);
                        spoken.SetAggressive(config.Store().GetBool("spoken_commands_aggressive"));
                        snippets.SetMode(config.Store().GetString("snippet_mode"));
                        overlay.SetReduceMotion(config.Store().GetBool("reduce_motion"));
                        usage_stats.SetUsageStatsEnabled(config.Store().GetBool("keep_usage_stats"));
                        overlay.SetScratchpadVisible(config.Store().GetBool("scratchpad_flow_bar"));
                        break;
                    case UiCommandType::ToggleAutostart:
                        Autostart::SetEnabled(!Autostart::IsEnabled());
                        break;
                    case UiCommandType::Quit:
                        quit.store(true);
                        break;
                    case UiCommandType::StartTransform: {
                        int slot = 0;
                        try {
                            if (!command.payload.empty()) {
                                slot = std::stoi(command.payload);
                            }
                        } catch (const std::exception&) {
                            slot = 0;
                        }
                        engine.RequestTransform(slot);
                        break;
                    }
                    case UiCommandType::OpenScratchpad:
                        lwf::Log::Info("scratchpad open requested");
                        if (command.payload.empty()) {
                            scratchpad_editor.OpenNewNote();
                        } else {
                            scratchpad_editor.OpenNote(command.payload);
                        }
                        break;
                }
            }

            std::string learned;
            std::string target;
            if (hotkey.TryTakeLearnedBinding(learned, target)) {
                if (!HotkeyBinding::IsConfigHotkeyKey(target)) {
                    lwf::Log::Warning("learn: unknown target ignored");
                } else {
                    const auto role = HotkeyBinding::RoleForConfigKey(target);
                    const std::vector<Binding> registry = LoadBindings(config.Store());
                    const std::string error =
                        HotkeyBinding::ValidateRecorderChord(learned, registry, *role);
                    if (!error.empty()) {
                        full.SetRecorderError(target, error);
                    } else {
                        config.Store().SetString(target, learned);
                        config.Save();
                        const std::vector<Binding> bindings = LoadBindings(config.Store());
                        hotkey.UpdateBindings(bindings);
                        transforms.SetHotkeyBindings(bindings);
                        full.SetLearnedHotkey(target, learned);
                    }
                }
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

            // Mini badge state, derived here so the engine thread never
            // touches the UI. Attention beats the done flash, which beats
            // the live FSM state; the badge itself handles the dwell times.
            std::string attention_message;
            if (attention.Take(attention_message)) {
                overlay.SetState(OverlayModel::PillState::Attention, attention_message);
            } else if (state == State::IDLE && previous_state == State::EMITTING) {
                done_flash_until = std::chrono::steady_clock::now() + done_flash_duration;
                overlay.SetState(OverlayModel::PillState::Done, {});
            } else if (state != State::IDLE ||
                       std::chrono::steady_clock::now() >= done_flash_until) {
                OverlayModel::PillState mini_state;
                switch (state) {
                    case State::CAPTURING:
                        mini_state = OverlayModel::PillState::Listening;
                        break;
                    case State::TRANSCRIBING:
                    case State::POSTPROCESSING:
                    case State::EMITTING:
                        mini_state = OverlayModel::PillState::Thinking;
                        break;
                    default:
                        mini_state = config.Store().GetBool("dictation_enabled")
                                         ? OverlayModel::PillState::Ready
                                         : OverlayModel::PillState::Paused;
                        break;
                }
                overlay.SetState(mini_state, {});
            }
            previous_state = state;

            full.SetLastTranscript(engine.LastText());
            full.RunOnce(1.0f / 60.0f);
            full.TakeCloseRequest();  // close hides; the tray owns Quit
            overlay.RenderFrame(1.0f / 60.0f);
            engine.SetScratchpadTarget(scratchpad_editor.IsDictationActive());
            scratchpad_editor.RenderFrame(1.0f / 60.0f);
            scratchpad_editor.TakeCloseRequest();
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }

        engine.Stop();
        hotkey.Stop();
        tray.Stop();
        tone_player.Shutdown();
        overlay.Shutdown();
        scratchpad_editor.Shutdown();
        full.Shutdown();
        glfwTerminate();
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
                  << " model=" << config.WhisperModelPath()
                  << " stt_provider=" << config.SttProvider()
                  << " stt_consent=" << (config.SttConsentAccepted() ? "true" : "false")
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
}

#endif
