#include "config.h"
#include <android/log.h>
#include <sys/stat.h>
#include <fstream>
#include <cstring>

namespace {

constexpr const char* kLogTag = "AutoGGButton";
constexpr const char* kConfigPath = "/sdcard/Download/autogg_config.txt";
constexpr const char* kFallbackPath = "/data/local/tmp/autogg_config.txt";

std::string g_message = "AUTO GG";
std::string g_activePath = kConfigPath;

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, kLogTag, __VA_ARGS__)

}

namespace Config {

void Load() {
    g_activePath = kConfigPath;
    
    std::ifstream file(g_activePath.c_str());
    if (file.is_open()) {
        std::getline(file, g_message);
        file.close();
        if (!g_message.empty()) {
            LOGI("Config loaded: %s", g_message.c_str());
            return;
        }
    }
    
    // Fallback
    g_activePath = kFallbackPath;
    std::ifstream fallback(g_activePath.c_str());
    if (fallback.is_open()) {
        std::getline(fallback, g_message);
        fallback.close();
        if (!g_message.empty()) {
            LOGI("Config loaded from fallback: %s", g_message.c_str());
            return;
        }
    }
    
    g_message = "AUTO GG";
    LOGI("Using default: %s", g_message.c_str());
}

void Save() {
    FILE* f = fopen(g_activePath.c_str(), "w");
    if (f) {
        fwrite(g_message.c_str(), 1, g_message.length(), f);
        fclose(f);
        LOGI("Config saved: %s", g_message.c_str());
        return;
    }
    
    // Fallback
    g_activePath = kFallbackPath;
    f = fopen(g_activePath.c_str(), "w");
    if (f) {
        fwrite(g_message.c_str(), 1, g_message.length(), f);
        fclose(f);
        LOGI("Config saved to fallback: %s", g_message.c_str());
        return;
    }
    
    LOGW("Failed to save config!");
}

std::string GetMessage() {
    return g_message;
}

void SetMessage(const std::string& msg) {
    if (!msg.empty()) {
        g_message = msg;
        Save();
    }
}

} // namespace Config
