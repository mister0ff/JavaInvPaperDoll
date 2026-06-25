
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

using SendChatMessageFn = void (*)(void* screen, const char* message);

void* g_minecraftBase = nullptr;
SendChatMessageFn g_sendChat = nullptr;
JavaVM* g_vm = nullptr;
jobject g_activity = nullptr;

bool FindChatFunction() {
    void* mc = dlopen(kMinecraftLibrary, RTLD_NOW | RTLD_NOLOAD);
    if (!mc) {
        LOGE("libminecraftpe.so not loaded");
        return false;
    }
    g_minecraftBase = mc;
    return true;
}

}

namespace ChatSender {

bool Initialize() {
    return FindChatFunction();
}

void Send(const std::string& message) {
    JNIEnv* env = nullptr;
    if (g_vm && g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
        jclass activityClass = env->FindClass("com/mojang/minecraftpe/MainActivity");
        if (activityClass) {
            jmethodID getInstance = env->GetStaticMethodID(activityClass, "getInstance", "()Lcom/mojang/minecraftpe/MainActivity;");
            if (getInstance) {
                jobject activity = env->CallStaticObjectMethod(activityClass, getInstance);
                if (activity) {
                    jmethodID sendChat = env->GetMethodID(activityClass, "nativeSendChatMessage", "(Ljava/lang/String;)V");
                    if (sendChat) {
                        jstring jmsg = env->NewStringUTF(message.c_str());
                        env->CallVoidMethod(activity, sendChat, jmsg);
                        env->DeleteLocalRef(jmsg);
                        LOGI("Chat sent via JNI: %s", message.c_str());
                        return;
                    }
                }
            }
        }
    }
    
    LOGI("Chat message queued: %s", message.c_str());
}

} // namespace ChatSender

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}
