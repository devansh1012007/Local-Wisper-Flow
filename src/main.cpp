#include "core/ConfigManager.h"
#include "core/DictationCleanup.h"
#include "core/DictationEngine.h"
#include "core/DictationFSM.h"
#include "core/EngineEvent.h"
#include "core/IInferenceProvider.h"
#include "core/ISttProvider.h"
#include "core/ProviderRouter.h"
#include "core/SttRouter.h"
#include "llm/LocalLlamaProvider.h"
#include "os/AppPaths.h"
#include "stt/LocalWhisperProvider.h"
#include "stt/cloud/GnaniContract.h"
#include "stt/cloud/GnaniProvider.h"
#ifdef LWF_HAVE_GRPC
#include "stt/GrpcInferenceProvider.h"
#endif
#ifdef LWF_HAVE_ONNXRUNTIME
#include "vad/SileroVadProvider.h"
#endif

#ifdef _WIN32
#include "audio/MiniaudioCapture.h"
#include "os/WindowsClipboardInjector.h"
#include "os/WindowsHotkeyListener.h"
#include "os/WindowsHttpClient.h"
#include "ui/MainWindow.h"
#endif

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kSampleRate = 16000;

class PassthroughLlm final : public ILlmProvider {
public:
    std::string Process(const std::string& text, const std::string&) override { return text; }
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

#ifdef _WIN32

std::string DescribeEngineError(EngineErrorCode code) {
    switch (code) {
        case EngineErrorCode::MicUnavailable:
            return "Microphone unavailable";
        case EngineErrorCode::MicLost:
            return "Microphone lost";
        case EngineErrorCode::SttFailed:
            return "Transcription failed";
        case EngineErrorCode::InjectFailed:
            return "Text injection failed";
        case EngineErrorCode::None:
            break;
    }
    return "Unknown error";
}

int RunWindowsApp() {
    try {
        std::cout << "Local Wisper Flow starting..." << std::endl;

        const std::string config_path = AppPaths::ResolveConfigPath();
        ConfigManager config(config_path);
        const std::string whisper_model = AppPaths::ResolveAsset(config.WhisperModelPath());
        const std::string llm_model = AppPaths::ResolveAsset(config.LlmModelPath());
        const std::string vad_model = AppPaths::ResolveAsset(config.VadModelPath());
        const std::uint32_t hotkey_code = static_cast<std::uint32_t>(config.HotkeyCode());
        std::cout << "[cfg] config=" << config_path << " hotkey=0x" << std::hex << hotkey_code
                  << std::dec << " model=" << whisper_model
                  << " vad_threshold=" << config.VadThreshold()
                  << " llm_model=" << llm_model
                  << " use_remote=" << (config.UseRemote() ? "true" : "false")
                  << " remote_endpoint=" << config.RemoteEndpoint() << std::endl;

        MiniaudioCapture audio(kSampleRate, 1);
        WhisperSettings whisper_settings;
        whisper_settings.language = config.WhisperLanguage();
        whisper_settings.threads = config.WhisperThreads();
        whisper_settings.beam_size = config.WhisperBeamSize();
        whisper_settings.disable_temperature_fallback = config.WhisperDisableTempFallback();
        auto local_stt = std::make_unique<LocalWhisperProvider>(whisper_model, whisper_settings);
        PersonalDictionary dictionary;
        dictionary.Load(AppPaths::ResolveAsset(config.DictionaryPath()));
        local_stt->SetInitialPrompt(dictionary.PromptFragment());
        WindowsClipboardInjector injector(config.InjectRestoreDelayMs());
        EngineStatusSink status_sink;

        PassthroughLlm passthrough_llm;
        std::optional<LocalLlamaProvider> llama_provider;
        ILlmProvider* llm = &passthrough_llm;
        if (!llm_model.empty()) {
            LocalLlamaProvider::Options llama_options;
            llama_options.context_size = static_cast<std::size_t>(config.LlmContextSize());
            llama_options.max_predict_tokens =
                static_cast<std::size_t>(config.LlmMaxPredictTokens());
            llama_options.gpu_layers = config.LlmGpuLayers();
            try {
                llama_provider.emplace(llm_model, llama_options);
                llm = &*llama_provider;
                std::cout << "[llm] model loaded: " << llm_model << std::endl;
                if (llama_provider->WarmUp()) {
                    std::cout << "[llm] warm-up complete" << std::endl;
                } else {
                    std::cerr << "[llm] warm-up failed, first dictation may be slow" << '\n';
                }
            } catch (const std::exception& e) {
                std::cerr << "[llm] disabled (" << e.what() << ")" << std::endl;
            }
        } else {
            std::cout << "[llm] disabled (no llm_model_path in config.json)" << std::endl;
        }
        local_stt->WarmUp();

        std::unique_ptr<IInferenceProvider> remote_provider;
        bool use_remote = config.UseRemote();
        if (use_remote) {
#ifdef LWF_HAVE_GRPC
            remote_provider = std::make_unique<GrpcInferenceProvider>(config.RemoteEndpoint());
#else
            std::cerr << "[router] use_remote=true ignored: built without gRPC support" << '\n';
            use_remote = false;
#endif
        }
        auto local_chain = std::make_unique<ProviderRouter>(std::move(local_stt),
                                                            std::move(remote_provider), use_remote);

#ifdef _WIN32
        WindowsHttpClient http_client;
#endif
        std::unique_ptr<ISttProvider> cloud_provider;
        const std::string stt_provider_name = config.SttProvider();
        const bool stt_consent = config.SttConsentAccepted();
        if (stt_provider_name != "local" && stt_provider_name != "gnani") {
            std::cerr << "[stt] unknown stt_provider '" << stt_provider_name
                      << "', using local" << '\n';
        }
        if (stt_provider_name == "gnani" && !stt_consent) {
            std::cerr << "[stt] stt_provider=gnani ignored: stt_consent_accepted is false"
                      << '\n';
        }
        const char* gnani_key_env = std::getenv(GnaniContract::kApiKeyEnv);
        const std::string gnani_key = gnani_key_env != nullptr ? gnani_key_env : "";
        if (stt_provider_name == "gnani" && stt_consent && gnani_key.empty()) {
            std::cerr << "[stt] stt_provider=gnani ignored: " << GnaniContract::kApiKeyEnv
                      << " is not set" << '\n';
        }
#ifdef _WIN32
        if (stt_provider_name == "gnani" && stt_consent && !gnani_key.empty()) {
            GnaniProviderSettings gnani_settings;
            gnani_settings.api_key_id = gnani_key;
            gnani_settings.language = config.SttLanguage();
            gnani_settings.timeout_ms = config.SttTimeoutMs();
            gnani_settings.max_clips = static_cast<std::size_t>(config.SttMaxClips());
            gnani_settings.keyterms = dictionary.Words();
            cloud_provider = std::make_unique<GnaniProvider>(http_client, gnani_settings);
            std::cout << "[stt] cloud provider gnani enabled (language "
                      << config.SttLanguage() << ")" << std::endl;
        }
#else
        if (stt_provider_name == "gnani" && stt_consent && !gnani_key.empty()) {
            std::cerr << "[stt] stt_provider=gnani ignored: cloud providers require a Windows build"
                      << '\n';
        }
#endif

        SttRoutingConfig stt_routing;
        stt_routing.provider = stt_provider_name;
        stt_routing.consent_accepted = stt_consent;
        stt_routing.api_key_source = [] {
            const char* key = std::getenv(GnaniContract::kApiKeyEnv);
            return key != nullptr ? std::string(key) : std::string();
        };
        SttRouter router(std::move(local_chain), std::move(cloud_provider), stt_routing);
        RouterLlm router_llm(router);

        CleanupSettings cleanup_settings;
        cleanup_settings.enabled = config.CleanupEnabled();
        cleanup_settings.min_words = static_cast<std::size_t>(config.CleanupMinWords());
        cleanup_settings.timeout = std::chrono::milliseconds(config.CleanupTimeoutMs());
        DictationCleanup cleanup(router_llm, cleanup_settings,
                                 [&status_sink](const std::string& note) {
                                     status_sink.OnStatus(note);
                                 });

        std::unique_ptr<IVadProvider> vad;
#ifdef LWF_HAVE_ONNXRUNTIME
        const std::string vad_model_path = config.VadModelPath();
        if (!vad_model_path.empty()) {
            try {
                vad = std::make_unique<SileroVadProvider>(vad_model_path, config.VadThreshold());
                std::cout << "[vad] silero loaded (threshold "
                          << config.VadThreshold() << ")" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[vad] disabled (" << e.what() << ")" << std::endl;
            }
        }
#endif

        EngineEventQueue queue;
        DictationFSM fsm;
        fsm.SetTransitionListener([](State previous, Trigger trigger, State current) {
            std::cout << "[fsm] " << ToString(previous) << " -> " << ToString(current) << " ["
                      << ToString(trigger) << "]" << std::endl;
        });

        EngineSettings engine_settings;
        engine_settings.sample_rate = kSampleRate;
        engine_settings.min_hold = std::chrono::milliseconds(config.MinHoldMs());
        engine_settings.hands_free_silence_ms = config.HandsFreeSilenceMs();
        engine_settings.max_recording = std::chrono::milliseconds(config.MaxRecordingMs());
        engine_settings.dictionary = &dictionary;
        DictationEngine engine(audio, router, cleanup, injector, fsm, vad.get(), engine_settings,
                               &status_sink);
        audio.SetChunkSink(engine.Sink());

        WindowsHotkeyListener hotkey(hotkey_code, config.AvoidAltGr());
        std::atomic<bool> quit{false};
        std::atomic<bool> engine_alive{true};
        router.SetCancelCheck([&quit] { return quit.load(); });

        std::thread engine_thread([&] {
            try {
                engine.Run(queue, quit);
            } catch (const std::exception& e) {
                std::cerr << "[engine] fatal: " << e.what() << '\n';
                status_sink.OnError(EngineErrorCode::None,
                                    std::string("engine stopped: ") + e.what());
            }
            engine_alive.store(false);
            hotkey.SetSwallowEnabled(false);
        });

        hotkey.StartListening([&queue](bool is_pressed) {
            queue.Push(EngineEvent{is_pressed ? EngineEventType::PttPress
                                              : EngineEventType::PttRelease,
                                    std::chrono::steady_clock::now()});
        });

        MainWindow ui(fsm, audio, status_sink, engine_alive, quit);
        if (!ui.Init()) {
            std::cerr << "[ui] initialization failed" << '\n';
            quit.store(true);
            queue.Close();
            engine_thread.join();
            return 1;
        }
        if (!hotkey.HookInstalled()) {
            status_sink.OnError(EngineErrorCode::None,
                                "hotkey hook failed to install; dictation is disabled");
        }

        std::cout << "Hold the hotkey and speak; release to inject the transcript." << std::endl;
        ui.Run();

        quit.store(true);
        queue.Close();
        engine_thread.join();

        std::cout << "Shutdown complete." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
}

#endif

}

int main() {
#ifdef _WIN32
    return RunWindowsApp();
#else
    try {
        ConfigManager config;
        std::cout << "[cfg] hotkey=" << config.HotkeyCode()
                  << " model=" << config.WhisperModelPath()
                  << " vad_threshold=" << config.VadThreshold()
                  << " llm_model=" << config.LlmModelPath()
                  << " use_remote=" << (config.UseRemote() ? "true" : "false")
                  << " remote_endpoint=" << config.RemoteEndpoint()
                  << " stt_provider=" << config.SttProvider()
                  << " stt_consent=" << (config.SttConsentAccepted() ? "true" : "false")
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "Local Wisper Flow: hotkey/text-injection layer requires a Windows build."
              << std::endl;
    return 0;
#endif
}
