#include "stt/cloud/MultipartBuilder.h"

#include <utility>

namespace {

const char kCrlf[] = "\r\n";

}  // namespace

MultipartBuilder::MultipartBuilder(std::string boundary) : boundary_(std::move(boundary)) {}

void MultipartBuilder::OpenPart(std::string& body, const std::string& name) const {
    body += "--";
    body += boundary_;
    body += kCrlf;
    body += "Content-Disposition: form-data; name=\"";
    body += name;
    body += "\"";
}

void MultipartBuilder::AddField(const std::string& name, const std::string& value) {
    OpenPart(body_, name);
    body_ += kCrlf;
    body_ += kCrlf;
    body_ += value;
    body_ += kCrlf;
    ++parts_;
}

void MultipartBuilder::AddJsonField(const std::string& name, const std::string& json) {
    OpenPart(body_, name);
    body_ += kCrlf;
    body_ += "Content-Type: application/json";
    body_ += kCrlf;
    body_ += kCrlf;
    body_ += json;
    body_ += kCrlf;
    ++parts_;
}

void MultipartBuilder::AddFile(const std::string& name, const std::string& filename,
                               const std::string& content_type,
                               const std::vector<std::uint8_t>& bytes) {
    OpenPart(body_, name);
    body_ += "; filename=\"";
    body_ += filename;
    body_ += "\"";
    body_ += kCrlf;
    body_ += "Content-Type: ";
    body_ += content_type;
    body_ += kCrlf;
    body_ += kCrlf;
    body_.append(bytes.begin(), bytes.end());
    body_ += kCrlf;
    ++parts_;
}

std::string MultipartBuilder::ContentType() const {
    return "multipart/form-data; boundary=" + boundary_;
}

std::vector<std::uint8_t> MultipartBuilder::Build() const {
    std::string out = body_;
    out += "--";
    out += boundary_;
    out += "--";
    out += kCrlf;
    return std::vector<std::uint8_t>(out.begin(), out.end());
}
