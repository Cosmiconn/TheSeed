#include "config.h"
#include <fstream>

namespace seed::util {

Config::Config(const std::string& filePath) : path(filePath) {}

bool Config::load() {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    try {
        file >> root;
        return true;
    } catch (...) {
        return false;
    }
}

bool Config::reload() {
    return load();
}

int Config::getInt(const std::string& key, int defaultVal) const {
    try {
        return root.at(key).get<int>();
    } catch (...) {
        return defaultVal;
    }
}

float Config::getFloat(const std::string& key, float defaultVal) const {
    try {
        return root.at(key).get<float>();
    } catch (...) {
        return defaultVal;
    }
}

std::string Config::getString(const std::string& key, const std::string& defaultVal) const {
    try {
        return root.at(key).get<std::string>();
    } catch (...) {
        return defaultVal;
    }
}

} // namespace seed::util
