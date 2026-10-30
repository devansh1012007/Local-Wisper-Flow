#pragma once

#include <cstdint>
#include <string>
#include <vector>

class MultipartBuilder {
public:
    explicit MultipartBuilder(std::string boundary);

    void AddField(const std::string& name, const std::string& value);

    void AddFile(const std::string& name, const std::string& filename,
                 const std::string& content_type, const std::vector<std::uint8_t>& data);

    const std::string& ContentType() const;

    std::vector<std::uint8_t> Build() const;

private:
    struct Part {
        std::string name;
        std::string filename;
        std::string content_type;
        std::vector<std::uint8_t> payload;
        bool is_file = false;
    };

    void ValidateHeaderToken(const std::string& text) const;
    void ValidateValue(const std::string& text) const;

    std::string boundary_;
    std::string content_type_;
    std::vector<Part> parts_;
};
