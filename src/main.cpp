#include "audio/MiniaudioCapture.h"
#include "core/ConfigManager.h"
#include "core/DictationFSM.h"
#include "core/ILlmProvider.h"
#include "core/IInferenceProvider.h"
#include "core/ProviderRouter.h"
#include "core/TransformManager.h"
#include "llm/LocalLlamaProvider.h"
#include "stt/GrpcInferenceProvider.h"
#include "stt/LocalWhisperProvider.h"

#ifdef _WIN32
#include "os/WindowsHotkeyListener.h"
#include "os/WindowsTextInjector.h"
#include "ui/MainWindow.h"
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
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
constexpr std::size_t kMinSamples = kSampleRate / 10;

class HotkeyEventQueue {
public:
    void Push(bool is_pressed) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            events_.push_back(is_pressed);
        }
        condition_.notify_one();
    }

    void Close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        condition_.notify_all();
    }

    std::optional<bool> PopFor(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!condition_.wait_for(lock, timeout, [this] { return !events_.empty() || closed_; })) {
            return std::nullopt;
        }
        if (events_.empty()) {
            return std::nullopt;
        }
        const bool is_pressed = events_.front();
        events_.pop_front();
        return is_pressed;
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<bool> events_;
    bool closed_ = false;
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

#ifdef _WIN32

void RunEngine(MiniaudioCapture& audio, IInferenceProvider& stt, ITextInjector& injector,
               DictationFSM& fsm, HotkeyEventQueue& queue, LastTranscriptBox& last_transcript,
               const std::atomic<bool>& quit) {
    while (!quit.load()) {
        std::optional<bool> event = queue.PopFor(std::chrono::milliseconds(200));
        if (!event) {
            continue;
        }
        if (*event) {
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
        } else {
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

#else

int RunFsmSmokeTest() {
    DictationFSM fsm;
    fsm.SetTransitionListener([](State previous, Trigger trigger, State current) {
        std::cout << "[fsm] " << ToString(previous) << " -> " << ToString(current) << " ["
                  << ToString(trigger) << "]" << std::endl;
    });

    fsm.TriggerEvent(Trigger::HOTKEY_PRESS);
    fsm.TriggerEvent(Trigger::VAD_SPEECH);
    fsm.TriggerEvent(Trigger::HOTKEY_RELEASE);
    fsm.TriggerEvent(Trigger::SST_COMPLETE);
    fsm.TriggerEvent(Trigger::SST_COMPLETE);
    fsm.TriggerEvent(Trigger::SST_COMPLETE);

    if (fsm.TriggerEvent(Trigger::VAD_SPEECH)) {
        std::cerr << "[fsm] Unexpected accepted transition" << '\n';
        return 1;
    }
    std::cout << "[fsm] Invalid transition correctly rejected" << std::endl;

    return fsm.GetCurrentState() == State::IDLE ? 0 : 1;
}

int RunRouterSmokeTest() {
    class Scripted final : public IInferenceProvider {
    public:
        Scripted(std::string tag, bool fail) : tag_(std::move(tag)), fail_(fail) {}

        std::string Transcribe(const std::vector<float>&) override {
            if (fail_) {
                throw std::runtime_error("scripted failure");
            }
            return "T:" + tag_;
        }

        std::string ProcessText(const std::string& text, const std::string&) override {
            if (fail_) {
                throw std::runtime_error("scripted failure");
            }
            return "P:" + text + ":" + tag_;
        }

    private:
        std::string tag_;
        bool fail_;
    };

    {
        ProviderRouter router(std::make_unique<Scripted>("local", false),
                              std::make_unique<Scripted>("remote", true), true);
        const std::vector<float> pcm(16, 0.0f);
        const std::string transcript = router.Transcribe(pcm);
        const std::string processed = router.ProcessText("hi", "");
        if (transcript != "T:local" || processed != "P:hi:local") {
            std::cerr << "[router-smoke] fallback routing mismatch: '" << transcript << "' / '"
                      << processed << "'" << '\n';
            return 1;
        }
    }
    {
        ProviderRouter router(std::make_unique<Scripted>("local", false),
                              std::make_unique<Scripted>("remote", false), false);
        if (router.ProcessText("hi", "") != "P:hi:local") {
            std::cerr << "[router-smoke] use_remote=false must use local" << '\n';
            return 1;
        }
    }

    std::cout << "[router-smoke] PASS" << std::endl;
    return 0;
}

int RunLlmSmokeTest() {
    class LocalPassthrough final : public ILlmProvider {
    public:
        std::string Process(const std::string& text, const std::string&) override {
            return TrimCopy(text);
        }
    };

    LocalPassthrough provider;
    TransformManager manager(provider, "transforms.json");

    std::atomic<bool> done{false};
    bool result_ok = false;
    manager.ExecuteTransform("Fix Grammar", "  helo wrld  ", [&](std::string result) {
        std::cout << "[llm-smoke] callback fired on background thread, result: '" << result
                  << "'" << '\n';
        result_ok = (result == "helo wrld");
        done.store(true);
    });

    for (int i = 0; i < 200 && !done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!done.load()) {
        std::cerr << "[llm-smoke] callback never fired" << '\n';
        return 1;
    }
    if (!result_ok) {
        std::cerr << "[llm-smoke] unexpected transform result" << '\n';
        return 1;
    }

    std::atomic<bool> unknown_done{false};
    manager.ExecuteTransform("Does Not Exist", "abc", [&](std::string result) {
        unknown_done.store(true);
        if (!result.empty()) {
            result_ok = false;
        }
    });
    for (int i = 0; i < 100 && !unknown_done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!unknown_done.load() || !result_ok) {
        std::cerr << "[llm-smoke] unknown-transform handling failed" << '\n';
        return 1;
    }

    std::cout << "[llm-smoke] PASS" << std::endl;
    return 0;
}

#endif

}

int main() {
#ifdef _WIN32
    try {
        std::cout << "Local Wisper Flow starting..." << std::endl;

        ConfigManager config;
        const std::uint32_t hotkey_code = static_cast<std::uint32_t>(config.HotkeyCode());
        std::cout << "[cfg] hotkey=0x" << std::hex << hotkey_code << std::dec
                  << " model=" << config.WhisperModelPath()
                  << " vad_threshold=" << config.VadThreshold()
                  << " llm_model=" << config.LlmModelPath()
                  << " use_remote=" << (config.UseRemote() ? "true" : "false")
                  << " remote_endpoint=" << config.RemoteEndpoint() << std::endl;

        MiniaudioCapture audio(kSampleRate, 1);
        auto local_stt = std::make_unique<LocalWhisperProvider>(config.WhisperModelPath());
        WindowsTextInjector injector;
        LastTranscriptBox last_transcript;

        PassthroughLlm passthrough_llm;
        std::optional<LocalLlamaProvider> llama_provider;
        ILlmProvider* llm = &passthrough_llm;
        const std::string llm_model_path = config.LlmModelPath();
        if (!llm_model_path.empty()) {
            try {
                llama_provider.emplace(llm_model_path);
                llm = &*llama_provider;
                std::cout << "[llm] model loaded: " << llm_model_path << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[llm] disabled (" << e.what() << ")" << std::endl;
            }
        } else {
            std::cout << "[llm] disabled (no llm_model_path in config.json)" << std::endl;
        }

        LocalProviderStack local_stack(*local_stt, *llm);
        auto remote_provider = std::make_unique<GrpcInferenceProvider>(config.RemoteEndpoint());
        ProviderRouter router(std::move(local_stt), std::move(remote_provider),
                              config.UseRemote());
        RouterLlm router_llm(router);

        TransformManager transforms(router_llm, "transforms.json");

        HotkeyEventQueue queue;
        DictationFSM fsm;
        fsm.SetTransitionListener([](State previous, Trigger trigger, State current) {
            std::cout << "[fsm] " << ToString(previous) << " -> " << ToString(current) << " ["
                      << ToString(trigger) << "]" << std::endl;
        });
        WindowsHotkeyListener hotkey(hotkey_code);
        std::atomic<bool> quit{false};

        std::thread engine([&] {
            try {
                RunEngine(audio, router, injector, fsm, queue, last_transcript, quit);
            } catch (const std::exception& e) {
                std::cerr << "[engine] Error: " << e.what() << '\n';
                quit.store(true);
            }
        });

        hotkey.StartListening([&queue](bool is_pressed) { queue.Push(is_pressed); });

        MainWindow ui(fsm, audio);
        if (!ui.Init()) {
            std::cerr << "[ui] initialization failed" << '\n';
            quit.store(true);
            queue.Close();
            engine.join();
            return 1;
        }

        std::cout << "Hold the hotkey and speak; release to inject the transcript." << std::endl;
        ui.Run();

        quit.store(true);
        queue.Close();
        engine.join();

        std::cout << "Shutdown complete." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
#else
    try {
        ConfigManager config;
        std::cout << "[cfg] hotkey=" << config.HotkeyCode()
                  << " model=" << config.WhisperModelPath()
                  << " vad_threshold=" << config.VadThreshold()
                  << " llm_model=" << config.LlmModelPath()
                  << " use_remote=" << (config.UseRemote() ? "true" : "false")
                  << " remote_endpoint=" << config.RemoteEndpoint() << std::endl;
        std::cout << "Local Wisper Flow: hotkey/text-injection layer requires a Windows build."
                  << std::endl;
        const int fsm_rc = RunFsmSmokeTest();
        const int llm_rc = RunLlmSmokeTest();
        const int router_rc = RunRouterSmokeTest();
        return (fsm_rc != 0 || llm_rc != 0 || router_rc != 0) ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
#endif
}
