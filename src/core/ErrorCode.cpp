#include "core/ErrorCode.h"

const char* ToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::MicUnavailable: return "MicUnavailable";
        case ErrorCode::MicLost: return "MicLost";
        case ErrorCode::ModelMissing: return "ModelMissing";
        case ErrorCode::ModelLoadFailed: return "ModelLoadFailed";
        case ErrorCode::SttFailed: return "SttFailed";
        case ErrorCode::LlmFailed: return "LlmFailed";
        case ErrorCode::LlmTimeout: return "LlmTimeout";
        case ErrorCode::InjectFailed: return "InjectFailed";
        case ErrorCode::InjectBlockedElevated: return "InjectBlockedElevated";
        case ErrorCode::ClipboardBusy: return "ClipboardBusy";
        case ErrorCode::HookInstallFailed: return "HookInstallFailed";
        case ErrorCode::ConfigCorrupt: return "ConfigCorrupt";
        case ErrorCode::DownloadFailed: return "DownloadFailed";
        case ErrorCode::ChecksumMismatch: return "ChecksumMismatch";
        case ErrorCode::Unknown: return "Unknown";
    }
    return "Unknown";
}

const char* UserMessage(ErrorCode code) {
    switch (code) {
        case ErrorCode::MicUnavailable: return "Microphone unavailable";
        case ErrorCode::MicLost: return "Microphone was disconnected";
        case ErrorCode::ModelMissing: return "Model file is missing";
        case ErrorCode::ModelLoadFailed: return "Model failed to load";
        case ErrorCode::SttFailed: return "Transcription failed";
        case ErrorCode::LlmFailed: return "Text cleanup failed";
        case ErrorCode::LlmTimeout: return "Text cleanup timed out";
        case ErrorCode::InjectFailed: return "Could not paste the text";
        case ErrorCode::InjectBlockedElevated: return "Target app runs elevated - run the app as admin to paste there";
        case ErrorCode::ClipboardBusy: return "Clipboard is busy, try again";
        case ErrorCode::HookInstallFailed: return "Keyboard hook failed to install";
        case ErrorCode::ConfigCorrupt: return "Config file was corrupt, defaults loaded";
        case ErrorCode::DownloadFailed: return "Download failed";
        case ErrorCode::ChecksumMismatch: return "Downloaded file failed verification";
        case ErrorCode::Unknown: return "Unexpected error";
    }
    return "Unexpected error";
}
