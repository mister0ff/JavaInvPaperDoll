#pragma once

#include <jni.h>      // <-- ADICIONADO (JavaVM precisa disso)
#include <string>

namespace ChatSender {
    void SetJVM(JavaVM* vm);
    bool Initialize();
    void Send(const std::string& message);
}
