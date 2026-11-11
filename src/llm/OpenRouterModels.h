#pragma once

#include "stt/cloud/IHttpTransport.h"

#include <string>
#include <vector>

namespace OpenRouterModels {

enum class FetchStatus { Ok, Network, NonOk, BadJson, Empty };

struct ModelList {
    FetchStatus status = FetchStatus::Network;
    std::vector<std::string> modelIds;
    std::string errorDetail;
};

ModelList FetchOpenRouterModels(IHttpTransport& transport, const std::string& apiKey);

}  // namespace OpenRouterModels
