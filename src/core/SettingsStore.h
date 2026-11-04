#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class SettingsStore {
public:
    enum class Type { Int, Double, Bool, String };

    struct SchemaEntry {
        const char* key;
        Type type;
        double default_value;
        const char* default_string;
        double min_value;
        double max_value;
        bool hot_apply;
    };

    using Listener = std::function<void(const std::string& key)>;

    explicit SettingsStore(std::string path);

    void Load();
    void Save();

    int GetInt(const std::string& key) const;
    double GetDouble(const std::string& key) const;
    bool GetBool(const std::string& key) const;
    std::string GetString(const std::string& key) const;

    void SetInt(const std::string& key, int value);
    void SetDouble(const std::string& key, double value);
    void SetBool(const std::string& key, bool value);
    void SetString(const std::string& key, const std::string& value);

    bool HasKey(const std::string& key) const;
    bool IsHotApply(const std::string& key) const;
    static const std::vector<SchemaEntry>& Schema();

    std::vector<std::string> ClampedOnLastLoad() const;
    bool CorruptOnLastLoad() const;
    bool LoadedExistingFile() const;

    void AddListener(const std::string& key, Listener listener);

private:
    void LoadLocked();

    const SchemaEntry* FindSchema(const std::string& key) const;
    double GetNumber(const std::string& key) const;
    void SetNumber(const std::string& key, double value);
    void Notify(const std::string& key);

    mutable std::mutex mutex_;
    std::string path_;
    std::string raw_unknown_;
    std::unordered_map<std::string, double> numbers_;
    std::unordered_map<std::string, std::string> strings_;
    std::unordered_map<std::string, bool> bools_;
    std::unordered_map<std::string, std::vector<Listener>> listeners_;
    std::vector<std::string> clamped_;
    bool corrupt_ = false;
    bool loaded_existing_ = false;
};
