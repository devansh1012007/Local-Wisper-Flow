#pragma once

enum class ErrorCode {
    MicUnavailable,
    MicLost,
    ModelMissing,
    ModelLoadFailed,
    SttFailed,
    LlmFailed,
    LlmTimeout,
    InjectFailed,
    InjectBlockedElevated,
    ClipboardBusy,
    HookInstallFailed,
    ConfigCorrupt,
    DownloadFailed,
    ChecksumMismatch,
    Unknown,
};

const char* ToString(ErrorCode code);
const char* UserMessage(ErrorCode code);
