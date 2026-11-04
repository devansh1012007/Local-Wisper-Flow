#pragma once

#ifndef LWF_APP_VERSION
#define LWF_APP_VERSION "0.0.0-dev"
#endif

namespace lwf {

inline const char* AppVersion() { return LWF_APP_VERSION; }

}
