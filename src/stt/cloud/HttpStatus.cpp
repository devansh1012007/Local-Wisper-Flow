#include "stt/cloud/HttpStatus.h"

SttErrorCategory ClassifyHttpStatus(int status) {
    if (status == 401 || status == 403) {
        return SttErrorCategory::Auth;
    }
    if (status == 429) {
        return SttErrorCategory::RateLimited;
    }
    if (status == 408) {
        return SttErrorCategory::Timeout;
    }
    if (status == 413) {
        return SttErrorCategory::PayloadTooLarge;
    }
    if (status >= 500) {
        return SttErrorCategory::Server;
    }
    if (status >= 400) {
        return SttErrorCategory::InvalidRequest;
    }
    return SttErrorCategory::Unknown;
}
