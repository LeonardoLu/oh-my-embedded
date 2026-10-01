#pragma once

#include <map>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string.h>

namespace settingstest {
extern std::map<std::string, bool> bools;
extern std::map<std::string, uint8_t> bytes;
extern std::map<std::string, uint16_t> ushorts;
extern std::map<std::string, std::string> strings;
}

class String {
public:
    String(const char* value) : _value(value) {}
    String(const std::string& value) : _value(value) {}

    void toCharArray(char* out, size_t size) const {
        if (!size) return;
        strncpy(out, _value.c_str(), size);
        out[size - 1] = 0;
    }

private:
    std::string _value;
};

class Preferences {
public:
    bool begin(const char*, bool) { return true; }
    void end() {}

    String getString(const char* key, const char* fallback) const {
        const auto it = settingstest::strings.find(key);
        return it == settingstest::strings.end() ? String(fallback) : String(it->second);
    }
    bool getBool(const char* key, bool fallback) const {
        const auto it = settingstest::bools.find(key);
        return it == settingstest::bools.end() ? fallback : it->second;
    }
    uint8_t getUChar(const char* key, uint8_t fallback) const {
        const auto it = settingstest::bytes.find(key);
        return it == settingstest::bytes.end() ? fallback : it->second;
    }
    uint16_t getUShort(const char* key, uint16_t fallback) const {
        const auto it = settingstest::ushorts.find(key);
        return it == settingstest::ushorts.end() ? fallback : it->second;
    }

    size_t putString(const char* key, const char* value) {
        settingstest::strings[key] = value;
        return strlen(value);
    }
    size_t putBool(const char* key, bool value) {
        settingstest::bools[key] = value;
        return 1;
    }
    size_t putUChar(const char* key, uint8_t value) {
        settingstest::bytes[key] = value;
        return 1;
    }
    size_t putUShort(const char* key, uint16_t value) {
        settingstest::ushorts[key] = value;
        return 1;
    }
};
