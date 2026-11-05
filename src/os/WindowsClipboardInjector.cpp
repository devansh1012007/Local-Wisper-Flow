#include "os/WindowsClipboardInjector.h"

#include "core/Utf8.h"
#include "os/WindowsTextInjector.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

constexpr UINT kClipboardOpenAttempts = 10;
constexpr std::chrono::milliseconds kClipboardRetryDelay{20};
constexpr std::chrono::milliseconds kPasteSettleDelay{30};
constexpr std::size_t kMaxSavedBytes = 64u * 1024 * 1024;

struct SavedFormat {
    UINT format = 0;
    std::vector<std::uint8_t> bytes;
};

std::wstring ToWide(const std::string& text) {
    const std::string safe = Utf8::TrimTrailingPartial(text);
    if (safe.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, safe.data(),
                                           static_cast<int>(safe.size()), nullptr, 0);
    if (length <= 0) {
        throw std::runtime_error("InjectText: UTF-8 to UTF-16 conversion failed");
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, safe.data(), static_cast<int>(safe.size()), &wide[0],
                        length);
    return wide;
}

HGLOBAL CopyBytesToGlobal(const void* data, std::size_t size) {
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, size);
    if (handle == nullptr) {
        return nullptr;
    }
    void* target = GlobalLock(handle);
    if (target == nullptr) {
        GlobalFree(handle);
        return nullptr;
    }
    std::memcpy(target, data, size);
    GlobalUnlock(handle);
    return handle;
}

bool OpenClipboardWithRetry(HWND window) {
    for (UINT attempt = 0; attempt < kClipboardOpenAttempts; ++attempt) {
        if (OpenClipboard(window)) {
            return true;
        }
        std::this_thread::sleep_for(kClipboardRetryDelay);
    }
    return false;
}

std::vector<SavedFormat> SaveClipboardFormats() {
    std::vector<SavedFormat> saved;
    std::size_t total = 0;
    UINT format = EnumClipboardFormats(0);
    while (format != 0) {
        HANDLE handle = GetClipboardData(format);
        if (handle != nullptr) {
            SIZE_T size = GlobalSize(handle);
            const void* data = GlobalLock(handle);
            if (data != nullptr) {
                if (size > 0 && total + size <= kMaxSavedBytes) {
                    SavedFormat entry;
                    entry.format = format;
                    entry.bytes.resize(size);
                    std::memcpy(entry.bytes.data(), data, size);
                    total += size;
                    saved.push_back(std::move(entry));
                }
                GlobalUnlock(handle);
            }
        }
        format = EnumClipboardFormats(format);
    }
    return saved;
}

void SetExclusionFormats(HWND window) {
    const UINT exclude_monitor =
        RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
    if (exclude_monitor != 0) {
        SetClipboardData(exclude_monitor, nullptr);
    }
    const UINT include_history = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    if (include_history != 0) {
        HGLOBAL no = CopyBytesToGlobal("\x00\x00\x00\x00", 4);
        if (no != nullptr) {
            SetClipboardData(include_history, no);
        }
    }
    const UINT upload_cloud = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
    if (upload_cloud != 0) {
        HGLOBAL no = CopyBytesToGlobal("\x00\x00\x00\x00", 4);
        if (no != nullptr) {
            SetClipboardData(upload_cloud, no);
        }
    }
    (void)window;
}

void RestoreClipboardFormats(HWND window, std::vector<SavedFormat>& saved) {
    if (!OpenClipboardWithRetry(window)) {
        return;
    }
    EmptyClipboard();
    for (SavedFormat& entry : saved) {
        if (entry.bytes.empty()) {
            continue;
        }
        HGLOBAL handle = CopyBytesToGlobal(entry.bytes.data(), entry.bytes.size());
        if (handle != nullptr) {
            if (SetClipboardData(entry.format, handle) == nullptr) {
                GlobalFree(handle);
            }
        }
    }
    CloseClipboard();
}

void SendPasteChord() {
    std::vector<INPUT> events;
    INPUT ctrl_down{};
    ctrl_down.type = INPUT_KEYBOARD;
    ctrl_down.ki.wVk = VK_CONTROL;
    events.push_back(ctrl_down);
    INPUT v_down{};
    v_down.type = INPUT_KEYBOARD;
    v_down.ki.wVk = 'V';
    events.push_back(v_down);
    INPUT v_up = v_down;
    v_up.ki.dwFlags = KEYEVENTF_KEYUP;
    events.push_back(v_up);
    INPUT ctrl_up = ctrl_down;
    ctrl_up.ki.dwFlags = KEYEVENTF_KEYUP;
    events.push_back(ctrl_up);
    SendInput(static_cast<UINT>(events.size()), events.data(), sizeof(INPUT));
}

void PumpPendingMessages() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

}  // namespace

WindowsClipboardInjector::WindowsClipboardInjector(int restore_delay_ms)
    : restore_delay_ms_(restore_delay_ms) {
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = L"LocalWisperFlowClipboard";
    RegisterClassW(&window_class);
    window_ = CreateWindowExW(WS_EX_TOOLWINDOW, L"LocalWisperFlowClipboard", L"", WS_OVERLAPPED, 0,
                              0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (window_ == nullptr) {
        throw std::runtime_error("WindowsClipboardInjector: hidden window creation failed");
    }
}

WindowsClipboardInjector::~WindowsClipboardInjector() {
    if (window_ != nullptr) {
        DestroyWindow(static_cast<HWND>(window_));
        window_ = nullptr;
    }
}

void WindowsClipboardInjector::InjectText(const std::string& text) {
    if (text.empty()) {
        return;
    }

    const auto hwnd = static_cast<HWND>(window_);
    const std::string crlf = Utf8::NewlinesToCrlf(Utf8::TrimTrailingPartial(text));
    std::wstring wide_crlf;
    {
        const int length =
            MultiByteToWideChar(CP_UTF8, 0, crlf.data(), static_cast<int>(crlf.size()), nullptr, 0);
        if (length > 0) {
            wide_crlf.resize(static_cast<std::size_t>(length));
            MultiByteToWideChar(CP_UTF8, 0, crlf.data(), static_cast<int>(crlf.size()),
                                &wide_crlf[0], length);
        }
    }
    if (wide_crlf.empty()) {
        return;
    }

    std::vector<SavedFormat> saved;
    const bool clipboard_available = OpenClipboardWithRetry(hwnd);
    if (clipboard_available) {
        saved = SaveClipboardFormats();
        CloseClipboard();
    } else {
        WindowsTextInjector fallback;
        fallback.InjectText(text);
        return;
    }

    if (!OpenClipboardWithRetry(hwnd)) {
        WindowsTextInjector fallback;
        fallback.InjectText(text);
        return;
    }
    EmptyClipboard();
    HGLOBAL text_handle =
        CopyBytesToGlobal(wide_crlf.data(), (wide_crlf.size() + 1) * sizeof(wchar_t));
    if (text_handle == nullptr || SetClipboardData(CF_UNICODETEXT, text_handle) == nullptr) {
        if (text_handle != nullptr) {
            GlobalFree(text_handle);
        }
        CloseClipboard();
        WindowsTextInjector fallback;
        fallback.InjectText(text);
        return;
    }
    SetExclusionFormats(hwnd);
    CloseClipboard();

    std::this_thread::sleep_for(kPasteSettleDelay);
    SendPasteChord();

    std::this_thread::sleep_for(std::chrono::milliseconds(restore_delay_ms_));
    RestoreClipboardFormats(hwnd, saved);
    PumpPendingMessages();
}
