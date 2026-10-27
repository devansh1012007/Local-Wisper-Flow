#pragma once

#include <string>

namespace WhisperText {

std::string FilterHallucinations(const std::string& transcript);

bool IsOnlyHallucination(const std::string& transcript);

}  // namespace WhisperText
