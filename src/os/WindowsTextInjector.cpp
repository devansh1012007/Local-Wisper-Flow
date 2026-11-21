#include "os/WindowsTextInjector.h"

#include "core/ErrorCode.h"
#include "core/InjectionException.h"
#include "core/Log.h"

#include <windows.h>

#include <optional>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kClipboardOpenRetries = 8;

struct SavedClipboardFormat {
    UINT format;
    std::vector<BYTE> data;
};

bool IsDelayedRenderOrHandleFormat(UINT format) {
    if (format == CF_BITMAP || format == CF_PALETTE || format == CF_METAFILEPICT ||
        format == CF_ENHMETAFILE || format == CF_DSPBITMAP || format == CF_DSPPALETTE ||
        format == CF_DSPMETAFILEPICT || format == CF_DSPENHMETAFILE) {
        return true;
    }
    return format >= 0x0300 && format <= 0x0308;
}

std::vector<SavedClipboardFormat> CaptureClipboardFormats() {
    std::vector<SavedClipboardFormat> saved;
    UINT format = 0;
    for (;;) {
        format = EnumClipboardFormats(format);
        if (format == 0) {
            break;
        }
        if (IsDelayedRenderOrHandleFormat(format)) {
            continue;
        }
        HANDLE handle = GetClipboardData(format);
        if (handle == nullptr) {
            continue;
        }
        SIZE_T size = GlobalSize(handle);
        if (size == 0) {
            continue;
        }
        const void* data = GlobalLock(handle);
        if (data == nullptr) {
            continue;
        }
        SavedClipboardFormat entry;
        entry.format = format;
        entry.data.assign(static_cast<const BYTE*>(data),
                          static_cast<const BYTE*>(data) + size);
        GlobalUnlock(handle);
        saved.push_back(std::move(entry));
    }
    return saved;
}

void RestoreClipboardFormats(const std::vector<SavedClipboardFormat>& saved) {
    if (saved.empty()) {
        return;
    }
    if (!OpenClipboard(nullptr)) {
        Log::Warning("clipboard restore failed: could not reopen clipboard");
        return;
    }
    EmptyClipboard();
    std::size_t restored = 0;
    for (const SavedClipboardFormat& entry : saved) {
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, entry.data.size());
        if (handle == nullptr) {
            continue;
        }
        void* target = GlobalLock(handle);
        if (target == nullptr) {
            GlobalFree(handle);
            continue;
        }
        std::memcpy(target, entry.data.data(), entry.data.size());
        GlobalUnlock(handle);
        if (SetClipboardData(entry.format, handle) != nullptr) {
            ++restored;
        } else {
            GlobalFree(handle);
        }
    }
    CloseClipboard();
    if (restored != saved.size()) {
        Log::Warning("clipboard restore incomplete: " + std::to_string(restored) + "/" +
                     std::to_string(saved.size()));
    }
}

void SetClipboardExclusionFormats() {
    const UINT exclude_monitor = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
    const UINT include_history = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    const UINT upload_cloud = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
    const DWORD zero_value = 0;
    if (exclude_monitor != 0) {
        HANDLE handle = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
        if (handle != nullptr) {
            void* target = GlobalLock(handle);
            if (target != nullptr) {
                *static_cast<DWORD*>(target) = zero_value;
                GlobalUnlock(handle);
                if (SetClipboardData(exclude_monitor, handle) == nullptr) {
                    GlobalFree(handle);
                }
            } else {
                GlobalFree(handle);
            }
        }
    }
    for (const UINT format : {include_history, upload_cloud}) {
        if (format == 0) {
            continue;
        }
        HANDLE handle = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
        if (handle != nullptr) {
            void* target = GlobalLock(handle);
            if (target != nullptr) {
                *static_cast<DWORD*>(target) = zero_value;
                GlobalUnlock(handle);
                if (SetClipboardData(format, handle) == nullptr) {
                    GlobalFree(handle);
                }
            } else {
                GlobalFree(handle);
            }
        }
    }
}

std::string Lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

}

void WindowsTextInjector::SetPasteChordProvider(std::function<std::string()> provider) {
    paste_chord_provider_ = std::move(provider);
}

void WindowsTextInjector::SetInjectDelayMs(int delay_ms) {
    inject_delay_ms_ = delay_ms;
}

bool WindowsTextInjector::ForegroundIsElevated() const {
    HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) {
        return false;
    }
    DWORD process_id = 0;
    GetWindowThreadProcessId(foreground, &process_id);
    if (process_id == 0) {
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr) {
        return false;
    }
    HANDLE token = nullptr;
    const bool opened = OpenProcessToken(process, TOKEN_QUERY, &token) != 0;
    bool elevated = false;
    if (opened && token != nullptr) {
        DWORD return_length = 0;
        GetTokenInformation(token, TokenElevation, &elevated, sizeof(elevated), &return_length);
        CloseHandle(token);
    }
    CloseHandle(process);

    bool we_are_elevated = false;
    HANDLE our_token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &our_token) && our_token != nullptr) {
        DWORD return_length = 0;
        GetTokenInformation(our_token, TokenElevation, &we_are_elevated,
                            sizeof(we_are_elevated), &return_length);
        CloseHandle(our_token);
    }
    return elevated && !we_are_elevated;
}

bool WindowsTextInjector::InjectViaTyping(const std::string& text) {
    if (text.size() > 4000) {
        return false;
    }
    if (inject_delay_ms_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(inject_delay_ms_));
    }
    const int wide_length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                                static_cast<int>(text.size()), nullptr, 0);
    if (wide_length <= 0) {
        return false;
    }
    std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &wide[0],
                        wide_length);

    std::vector<INPUT> inputs;
    inputs.reserve(wide.size() * 2);
    for (const wchar_t c : wide) {
        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = static_cast<WORD>(c);
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);
        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }
    if (inputs.empty()) {
        return true;
    }
    const UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(),
                                static_cast<int>(sizeof(INPUT)));
    return sent == inputs.size();
}

void WindowsTextInjector::PasteChord(const std::string& paste_chord) {
    const std::string chord = Lower(paste_chord.empty() ? std::string("ctrl+v") : paste_chord);
    const bool ctrl = chord.find("ctrl") != std::string::npos;
    const bool shift = chord.find("shift") != std::string::npos;
    const bool insert_key = chord.find("insert") != std::string::npos;

    std::vector<INPUT> inputs;
    const WORD v_key = insert_key ? VK_INSERT : static_cast<WORD>('V');
    INPUT modifier{};
    modifier.type = INPUT_KEYBOARD;
    modifier.ki.wVk = VK_CONTROL;
    if (ctrl) {
        inputs.push_back(modifier);
    }
    if (shift) {
        modifier.ki.wVk = VK_SHIFT;
        inputs.push_back(modifier);
    }
    INPUT key{};
    key.type = INPUT_KEYBOARD;
    key.ki.wVk = v_key;
    inputs.push_back(key);
    INPUT key_up = key;
    key_up.ki.dwFlags = KEYEVENTF_KEYUP;
    inputs.push_back(key_up);
    INPUT modifier_up{};
    modifier_up.type = INPUT_KEYBOARD;
    modifier_up.ki.dwFlags = KEYEVENTF_KEYUP;
    if (shift) {
        modifier_up.ki.wVk = VK_SHIFT;
        inputs.push_back(modifier_up);
    }
    if (ctrl) {
        modifier_up.ki.wVk = VK_CONTROL;
        inputs.push_back(modifier_up);
    }
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(),
              static_cast<int>(sizeof(INPUT)));
}

void WindowsTextInjector::InjectViaClipboard(const std::string& text,
                                             const std::string& paste_chord) {
    const UINT cf_unicode = CF_UNICODETEXT;
    const int wide_length =
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (wide_length <= 0) {
        throw lwf::InjectionException(lwf::ErrorCode::InjectFailed, "utf8 conversion failed");
    }

    bool opened = false;
    for (int attempt = 0; attempt < kClipboardOpenRetries && !opened; ++attempt) {
        opened = OpenClipboard(nullptr) != 0;
        if (!opened) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25 << attempt));
        }
    }
    if (!opened) {
        throw lwf::InjectionException(lwf::ErrorCode::ClipboardBusy,
                                      "clipboard could not be opened after retries");
    }

    std::vector<SavedClipboardFormat> saved = CaptureClipboardFormats();
    EmptyClipboard();

    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wide_length) * sizeof(wchar_t));
    if (handle == nullptr) {
        RestoreClipboardFormats(saved);
        CloseClipboard();
        throw lwf::InjectionException(lwf::ErrorCode::InjectFailed, "clipboard alloc failed");
    }
    void* target = GlobalLock(handle);
    if (target == nullptr) {
        GlobalFree(handle);
        RestoreClipboardFormats(saved);
        CloseClipboard();
        throw lwf::InjectionException(lwf::ErrorCode::InjectFailed, "clipboard lock failed");
    }
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        static_cast<wchar_t*>(target), wide_length);
    GlobalUnlock(handle);

    if (SetClipboardData(cf_unicode, handle) == nullptr) {
        GlobalFree(handle);
        RestoreClipboardFormats(saved);
        CloseClipboard();
        throw lwf::InjectionException(lwf::ErrorCode::InjectFailed, "clipboard set failed");
    }
    SetClipboardExclusionFormats();
    CloseClipboard();

    if (inject_delay_ms_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(inject_delay_ms_));
    }
    PasteChord(paste_chord);

    std::thread([saved = std::move(saved)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        if (OpenClipboard(nullptr)) {
            const bool ours_still =
                GetClipboardData(CF_UNICODETEXT) != nullptr;
            CloseClipboard();
            if (ours_still) {
                RestoreClipboardFormats(saved);
            }
        }
    }).detach();
}

void WindowsTextInjector::InjectText(const std::string& text) {
    if (text.empty()) {
        return;
    }
    if (ForegroundIsElevated()) {
        throw lwf::InjectionException(lwf::ErrorCode::InjectBlockedElevated,
                                      "target window is elevated; run the app as admin to paste there");
    }
    std::string paste_chord = "ctrl+v";
    if (paste_chord_provider_) {
        paste_chord = paste_chord_provider_();
    }
    try {
        InjectViaClipboard(text, paste_chord);
        return;
    } catch (const lwf::InjectionException& e) {
        if (e.Code() != lwf::ErrorCode::ClipboardBusy) {
            throw;
        }
        Log::Warning("clipboard busy, falling back to SendInput typing");
    }
    if (!InjectViaTyping(text)) {
        throw lwf::InjectionException(lwf::ErrorCode::InjectFailed,
                                      "SendInput typing fallback failed");
    }
}

std::optional<std::string> WindowsSelectionCapture::CaptureSelection() {
    if (!OpenClipboard(nullptr)) {
        return std::nullopt;
    }
    saved_clipboard_.clear();
    if (const HANDLE handle = GetClipboardData(CF_UNICODETEXT); handle != nullptr) {
        if (const void* data = GlobalLock(handle); data != nullptr) {
            const int size = WideCharToMultiByte(CP_UTF8, 0, static_cast<const wchar_t*>(data), -1,
                                                 nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string utf8(static_cast<std::size_t>(size), '\0');
                WideCharToMultiByte(CP_UTF8, 0, static_cast<const wchar_t*>(data), -1, &utf8[0],
                                    size, nullptr, nullptr);
                utf8.pop_back();
                saved_clipboard_ = std::move(utf8);
            }
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    clipboard_saved_ = true;

    std::vector<INPUT> chord;
    INPUT down{};
    down.type = INPUT_KEYBOARD;
    down.ki.wVk = VK_CONTROL;
    chord.push_back(down);
    INPUT key = down;
    key.ki.wVk = static_cast<WORD>('C');
    chord.push_back(key);
    INPUT key_up = key;
    key_up.ki.dwFlags = KEYEVENTF_KEYUP;
    chord.push_back(key_up);
    INPUT ctrl_up = down;
    ctrl_up.ki.dwFlags = KEYEVENTF_KEYUP;
    chord.push_back(ctrl_up);
    SendInput(static_cast<UINT>(chord.size()), chord.data(), sizeof(INPUT));

    const auto start = std::chrono::steady_clock::now();
    std::string selected;
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start)
                .count() > 300) {
            RestoreClipboard();
            clipboard_saved_ = false;
            return std::nullopt;
        }
        if (OpenClipboard(nullptr)) {
            if (const HANDLE handle = GetClipboardData(CF_UNICODETEXT); handle != nullptr) {
                if (const void* data = GlobalLock(handle); data != nullptr) {
                    const int size = WideCharToMultiByte(CP_UTF8, 0,
                                                         static_cast<const wchar_t*>(data), -1,
                                                         nullptr, 0, nullptr, nullptr);
                    if (size > 0) {
                        std::string utf8(static_cast<std::size_t>(size), '\0');
                        WideCharToMultiByte(CP_UTF8, 0, static_cast<const wchar_t*>(data), -1,
                                            &utf8[0], size, nullptr, nullptr);
                        utf8.pop_back();
                        selected = std::move(utf8);
                    }
                    GlobalUnlock(handle);
                }
            }
            CloseClipboard();
            if (!selected.empty() && selected != saved_clipboard_) {
                return selected;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void WindowsSelectionCapture::RestoreClipboard() {
    if (!clipboard_saved_) {
        return;
    }
    clipboard_saved_ = false;
    if (!OpenClipboard(nullptr)) {
        return;
    }
    EmptyClipboard();
    const int wide_size = MultiByteToWideChar(CP_UTF8, 0, saved_clipboard_.c_str(),
                                              static_cast<int>(saved_clipboard_.size()), nullptr,
                                              0);
    if (wide_size > 0) {
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE,
                                     static_cast<SIZE_T>(wide_size) * sizeof(wchar_t));
        if (handle != nullptr) {
            void* target = GlobalLock(handle);
            if (target != nullptr) {
                MultiByteToWideChar(CP_UTF8, 0, saved_clipboard_.c_str(),
                                    static_cast<int>(saved_clipboard_.size()),
                                    static_cast<wchar_t*>(target), wide_size);
                GlobalUnlock(handle);
                if (SetClipboardData(CF_UNICODETEXT, handle) == nullptr) {
                    GlobalFree(handle);
                }
            } else {
                GlobalFree(handle);
            }
        }
    }
    CloseClipboard();
}
