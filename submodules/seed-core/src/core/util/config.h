#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace seed::util {

class Config {
    nlohmann::json root;
    std::string path;

public:
    explicit Config(const std::string& filePath);
    bool load();
    bool reload();

    int getInt(const std::string& key, int defaultVal) const;
    float getFloat(const std::string& key, float defaultVal) const;
    std::string getString(const std::string& key, const std::string& defaultVal) const;
};

} // namespace seed::util
