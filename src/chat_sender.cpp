#include "chat_sender.h"
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>

namespace {
constexpr const char* kLogTag = "AutoGGButton";
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)
JavaVM* g_vm = nullptr;
}

namespace ChatSender {
void SetJVM(JavaVM* vm) { g_vm = vm; }

void Send(const std::string& message) {
    JNIEnv* env = nullptr;
    if (!g_vm) { LOGE("JVM not available"); return; }
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Failed to get JNIEnv"); return;
    }
    
    jclass activityClass = env->FindClass("com/mojang/minecraftpe/MainActivity");
    if (!activityClass) { LOGE("MainActivity not found"); return; }
    
    jmethodID getInstance = env->GetStaticMethodID(activityClass, "getInstance", "()Lcom/mojang/minecraftpe/MainActivity;");
    if (!getInstance) { LOGE("getInstance not found"); return; }
    
    jobject activity = env->CallStaticObjectMethod(activityClass, getInstance);
    if (!activity) { LOGE("No activity instance"); return; }
    
    // Tenta vários métodos de envio de chat
    const char* methods[] = {
        "nativeSendChatMessage",
        "sendChatMessage", 
        "sendMessage",
        nullptr
    };
    
    for (int i = 0; methods[i]; ++i) {
        jmethodID sendChat = env->GetMethodID(activityClass, methods[i], "(Ljava/lang/String;)V");
        if (sendChat) {
            jstring jmsg = env->NewStringUTF(message.c_str());
            env->CallVoidMethod(activity, sendChat, jmsg);
            env->DeleteLocalRef(jmsg);
            LOGI("Chat sent: %s", message.c_str());
            return;
        }
    }
    
    LOGE("No chat send method found");
}
} // namespace ChatSender
