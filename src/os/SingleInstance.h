#pragma once

#include <string>

class SingleInstance {
public:
    explicit SingleInstance(const std::string& mutex_name);
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    bool IsFirstInstance() const;
    void SignalOpenSettings();

private:
    void* handle_ = nullptr;
};
