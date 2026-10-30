#include "stt/cloud/MultipartBuilder.h"

#include <stdexcept>
#include <utility>

namespace {

bool ContainsLineBreak(const std::string& text) {
    return text.find('\r') != std::string::npos || text.find('\n') != std::string::npos;
}

}  // namespace

MultipartBuilder::MultipartBuilder(std::string boundary) : boundary_(std::move(boundary)) {
    if (boundary_.empty() || boundary_.size() > 70) {
        throw std::runtime_error("MultipartBuilder: boundary must be 1-70 characters");
    }
    if (ContainsLineBreak(boundary_)) {
        throw std::runtime_error("MultipartBuilder: boundary must be a single line");
    }
    content_type_ = "multipart/form-data; boundary=" + boundary_;
}

void MultipartBuilder::AddField(const std::string& name, const std::string& value) {
    ValidateHeaderToken(name);
    ValidateValue(value);
    Part part;
    part.name = name;
    part.payload.assign(value.begin(), value.end());
    parts_.push_back(std::move(part));
}

void MultipartBuilder::AddFile(const std::string& name, const std::string& filename,
                               const std::string& content_type,
                               const std::vector<std::uint8_t>& data) {
    ValidateHeaderToken(name);
    ValidateHeaderToken(filename);
    ValidateHeaderToken(content_type);
    Part part;
    part.name = name;
    part.filename = filename;
    part.content_type = content_type;
    part.payload = data;
    part.is_file = true;
    parts_.push_back(std::move(part));
}

const std::string& MultipartBuilder::ContentType() const {
    return content_type_;
}

std::vector<std::uint8_t> MultipartBuilder::Build() const {
    std::vector<std::uint8_t> out;
    const auto append_text = [&out](const std::string& text) {
        out.insert(out.end(), text.begin(), text.end());
    };
    for (const Part& part : parts_) {
        append_text("--" + boundary_ + "\r\n");
        if (part.is_file) {
            append_text("Content-Disposition: form-data; name=\"" + part.name + "\"; filename=\"" +
                        part.filename + "\"\r\n");
            append_text("Content-Type: " + part.content_type + "\r\n");
        } else {
            append_text("Content-Disposition: form-data; name=\"" + part.name + "\"\r\n");
        }
        append_text("\r\n");
        out.insert(out.end(), part.payload.begin(), part.payload.end());
        append_text("\r\n");
    }
    append_text("--" + boundary_ + "--\r\n");
    return out;
}

void MultipartBuilder::ValidateHeaderToken(const std::string& text) const {
    if (ContainsLineBreak(text)) {
        throw std::runtime_error("MultipartBuilder: header tokens must be single-line");
    }
    if (text.find('"') != std::string::npos) {
        throw std::runtime_error("MultipartBuilder: header tokens must not contain quotes");
    }
    if (text.find(boundary_) != std::string::npos) {
        throw std::runtime_error("MultipartBuilder: header tokens must not contain the boundary");
    }
}

void MultipartBuilder::ValidateValue(const std::string& text) const {
    if (ContainsLineBreak(text)) {
        throw std::runtime_error("MultipartBuilder: values must be single-line");
    }
    if (text.find(boundary_) != std::string::npos) {
        throw std::runtime_error("MultipartBuilder: values must not contain the boundary");
    }
}
