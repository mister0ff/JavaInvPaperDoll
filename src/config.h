
#pragma once

#include <string>

namespace Config {
    void Load();
    void Save();
    std::string GetMessage();
    void SetMessage(const std::string& msg);
}
