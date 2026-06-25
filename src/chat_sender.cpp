#include "chat_sender.h"
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <cstring>

namespace {
constexpr const char* kLogTag = "AutoGGButton";
constexpr const char* kMinecraftLibrary = "libminecraftpe.so";
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)
JavaVM* g_vm = nullptr;
}

namespace ChatSender {
void SetJVM(JavaVM* vm) { g_vm = vm; }

bool Initialize() {
    void* mc = dlopen(kMinecraftLibrary, RTLD_NOW | RTLD_NOLOAD);
    if (!mc) {
        LOGE("libminecraftpe.so not loaded");
        return false;
    }
    return true;
}

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
    if (!activity) { LOGE("Could not get MainActivity"); return; }
    
    jmethodID sendChat = env->GetMethodID(activityClass, "nativeSendChatMessage", "(Ljava/lang/String;)V");
    if (sendChat) {
        jstring jmsg = env->NewStringUTF(message.c_str());
        env->CallVoidMethod(activity, sendChat, jmsg);
        env->DeleteLocalRef(jmsg);
        LOGI("Chat sent: %s", message.c_str());
        return;
    }
    
    sendChat = env->GetMethodID(activityClass, "sendChatMessage", "(Ljava/lang/String;)V");
    if (sendChat) {
        jstring jmsg = env->NewStringUTF(message.c_str());
        env->CallVoidMethod(activity, sendChat, jmsg);
        env->DeleteLocalRef(jmsg);
        LOGI("Chat sent (fallback): %s", message.c_str());
        return;
    }
    
    LOGE("No chat send method found");
}
}
