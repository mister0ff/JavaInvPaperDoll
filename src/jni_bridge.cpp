#include <jni.h>
#include <android/log.h>
#include "config.h"
#include "chat_sender.h"

namespace {
constexpr const char* kLogTag = "AutoGGButton";
JavaVM* g_vm = nullptr;
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
}

JavaVM* GetJVM() { return g_vm; }

extern "C" JNIEXPORT void JNICALL
Java_com_autogg_AutoGGService_sendChatMessage(JNIEnv* env, jobject, jstring message) {
    const char* msg = env->GetStringUTFChars(message, nullptr);
    if (msg) {
        ChatSender::Send(msg);
        env->ReleaseStringUTFChars(message, msg);
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_autogg_AutoGGService_getMessage(JNIEnv* env, jobject) {
    return env->NewStringUTF(Config::GetMessage().c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_autogg_AutoGGService_setMessage(JNIEnv* env, jobject, jstring message) {
    const char* msg = env->GetStringUTFChars(message, nullptr);
    if (msg) {
        Config::SetMessage(msg);
        env->ReleaseStringUTFChars(message, msg);
    }
}

// ============ ÚNICO JNI_OnLoad DO PROJETO ============
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    LOGI("JNI_OnLoad called, JVM captured");
    
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
        ChatSender::SetJVM(vm);
    }
    return JNI_VERSION_1_6;
}

