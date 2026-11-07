#pragma once

#include <map>
#include <mutex>
#include <string>

class ApiKeyStore {
public:
    explicit ApiKeyStore(std::string path);

    void Reload();
    std::string Get(const std::string& provider) const;
    bool Has(const std::string& provider) const;
    bool Set(const std::string& provider, const std::string& key);
    bool Remove(const std::string& provider);

private:
    void LoadLocked();
    bool PersistLocked();

    mutable std::mutex mutex_;
    std::string path_;
    std::map<std::string, std::string> keys_;
};
