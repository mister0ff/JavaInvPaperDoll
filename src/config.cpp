
#include "config.h"
#include <android/log.h>
#include <fstream>
#include <filesystem>

namespace {

constexpr const char* kLogTag = "AutoGGButton";
constexpr const char* kConfigPath = "/sdcard/Android/data/com.mojang.minecraftpe/files/autogg_config.txt";

std::string g_message = "AUTO GG";

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)

}

namespace Config {

void Load() {
    std::ifstream file(kConfigPath);
    if (file.is_open()) {
        std::getline(file, g_message);
        file.close();
        LOGI("Config loaded: %s", g_message.c_str());
    } else {
        g_message = "AUTO GG";
        Save();
    }
}

void Save() {
    std::filesystem::path path(kConfigPath);
    std::filesystem::create_directories(path.parent_path());
    
    std::ofstream file(kConfigPath);
    if (file.is_open()) {
        file << g_message;
        file.close();
        LOGI("Config saved: %s", g_message.c_str());
    }
}

std::string GetMessage() {
    return g_message;
}

void SetMessage(const std::string& msg) {
    g_message = msg;
    Save();
}

} // namespace Config
