#pragma once
#include <jni.h>

namespace FloatingButton {
    void SetJVM(JavaVM* vm);
    void Start();
    void Stop();
}
