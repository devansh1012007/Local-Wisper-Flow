#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lwf_sha256 {

std::string HexDigest(const std::string& input);
std::string HexDigestOfFile(const std::string& path, bool& ok);

}
