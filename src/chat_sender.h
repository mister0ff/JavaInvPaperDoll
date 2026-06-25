
#pragma once

#include <cstdint>
#include <string>

namespace ChatSender {
    bool Initialize();
    void Send(const std::string& message);
}
