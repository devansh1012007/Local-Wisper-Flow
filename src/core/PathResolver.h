#pragma once

#include <string>

std::string ResolveDataPath(const std::string& path, const std::string& data_dir,
                            const std::string& exe_dir);
