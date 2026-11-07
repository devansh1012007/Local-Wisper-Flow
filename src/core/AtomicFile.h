#pragma once

#include <string>

namespace lwf {

bool WriteAtomic(const std::string& path, const std::string& content);

}
