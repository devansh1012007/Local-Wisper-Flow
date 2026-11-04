#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Minimal multipart/form-data writer with CRLF framing and a fixed
// per-vendor boundary (see the *Contract.h headers). Parts are appended in
// order; Build() terminates the body and returns it as raw bytes.
class MultipartBuilder {
public:
    explicit MultipartBuilder(std::string boundary);

    void AddField(const std::string& name, const std::string& value);
    // A field whose part carries Content-Type: application/json.
    void AddJsonField(const std::string& name, const std::string& json);
    void AddFile(const std::string& name, const std::string& filename,
                 const std::string& content_type, const std::vector<std::uint8_t>& bytes);

    std::string ContentType() const;
    std::vector<std::uint8_t> Build() const;

private:
    void OpenPart(std::string& body, const std::string& name) const;

    std::string boundary_;
    std::string body_;
    std::size_t parts_ = 0;
};
