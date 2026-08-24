#include "audio/MiniaudioCapture.h"
#include "core/IInferenceProvider.h"
#include "stt/LocalWhisperProvider.h"

#ifdef _WIN32
#include "os/WindowsHotkeyListener.h"
#include "os/WindowsTextInjector.h"
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <cstddef>
#include <string>
#include <thread>
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
               HotkeyEventQueue& queue, std::atomic<bool>& transcribing,
               const std::atomic<bool>& quit) {
    bool recording = false;
    while (!quit.load()) {
        std::optional<bool> event = queue.PopFor(std::chrono::milliseconds(200));
        if (!event) {
            continue;
        }
        if (*event) {
            if (recording || transcribing.load()) {
                continue;
            }
            audio.Start();
            recording = true;
            std::cout << "[mic] Recording... release Right Alt to transcribe" << std::endl;
        } else if (recording) {
            recording = false;
            audio.Stop();
            std::vector<float> pcm = audio.GetRecordedAudio();
            if (pcm.size() < kMinSamples) {
                std::cout << "[mic] Tap shorter than 100 ms ignored" << std::endl;
                continue;
            }
            transcribing.store(true);
            std::cout << "[stt] Transcribing..." << std::endl;
            try {
                const std::string text = TrimCopy(stt.Transcribe(pcm));
                if (text.empty()) {
                    std::cout << "[stt] Nothing heard" << std::endl;
                } else {
                    injector.InjectText(text);
                }
            } catch (const std::exception& e) {
                std::cerr << "[stt] Error: " << e.what() << '\n';
            }
            transcribing.store(false);
        }
    }
    if (recording) {
        audio.Stop();
    }
}

int RunSelfTest(MiniaudioCapture& audio, IInferenceProvider& stt) {
    std::cout << "[selftest] Recording..." << std::endl;
    audio.Start();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    audio.Stop();
    std::cout << "[selftest] Transcribing..." << std::endl;
    const std::string text = TrimCopy(stt.Transcribe(audio.GetRecordedAudio()));
    std::cout << "[selftest] Result: " << (text.empty() ? "(nothing heard)" : text) << std::endl;
    return 0;
}
#endif

}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    try {
        std::cout << "Local Wisper Flow starting..." << std::endl;

        MiniaudioCapture audio(kSampleRate, 1);
        LocalWhisperProvider stt("ggml-base.en.bin");

        if (argc > 1 && std::string(argv[1]) == "--selftest") {
            return RunSelfTest(audio, stt);
        }

        WindowsTextInjector injector;
        HotkeyEventQueue queue;
        WindowsHotkeyListener hotkey;
        std::atomic<bool> transcribing{false};
        std::atomic<bool> quit{false};

        std::thread engine([&] {
            try {
                RunEngine(audio, stt, injector, queue, transcribing, quit);
            } catch (const std::exception& e) {
                std::cerr << "[engine] Error: " << e.what() << '\n';
                quit.store(true);
            }
        });

        hotkey.StartListening([&queue](bool is_pressed) { queue.Push(is_pressed); });

        std::cout << "Hold Right Alt and speak; release to type the transcript."
                  << " Press Enter here to quit." << std::endl;
        std::cout << "Tip: run with --selftest for a fixed 3-second microphone test."
                  << std::endl;

        std::cin.get();

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
    std::cout << "Local Wisper Flow: hotkey/text-injection layer requires a Windows build."
              << std::endl;
    return 0;
#endif
}
