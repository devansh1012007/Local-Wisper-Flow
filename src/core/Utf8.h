#pragma once

#include <cstddef>
#include <string>

namespace Utf8 {

std::string TrimTrailingPartial(const std::string& text);

std::string NormalizeNewlines(const std::string& text);

std::string NewlinesToCrlf(const std::string& text);

std::size_t TrailingPartialLength(const std::string& text);

}  // namespace Utf8
