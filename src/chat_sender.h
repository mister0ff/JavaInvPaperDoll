#pragma once
#include <string>

namespace ChatSender {
    void SetJVM(JavaVM* vm);
    bool Initialize();
    void Send(const std::string& message);
}
