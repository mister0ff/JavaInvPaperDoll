#include <android/log.h>
#include <jni.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstring>

#include "floating_button.h"
#include "chat_sender.h"
#include "config.h"

namespace {

constexpr const char* kLogTag = "AutoGGButton";

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)

using GlossInitFn = void (*)(bool);
using GlossHookFn = void* (*)(void*, void*, void**);

GlossInitFn g_glossInit = nullptr;
GlossHookFn g_glossHook = nullptr;

std::atomic<bool> g_initialized{false};

bool ResolvePreloaderApi() {
#ifndef RTLD_NOLOAD
#define RTLD_NOLOAD 0x00004
#endif
    void* preloader = nullptr;
    for (int attempt = 0; attempt < 120 && preloader == nullptr; ++attempt) {
        preloader = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD);
        if (!preloader) {
            usleep(250 * 1000);
        }
    }
    if (!preloader) {
        LOGE("libpreloader.so was not already loaded");
        return false;
    }

    g_glossInit = reinterpret_cast<GlossInitFn>(dlsym(preloader, "GlossInit"));
    g_glossHook = reinterpret_cast<GlossHookFn>(dlsym(preloader, "GlossHook"));
    if (!g_glossInit || !g_glossHook) {
        LOGE("required preloader exports missing");
        return false;
    }
    g_glossInit(true);
    return true;
}

extern "C" JNIEXPORT void JNICALL
Java_com_autogg_AutoGGService_sendChatMessage(JNIEnv* env, jobject /*thiz*/, jstring message) {
    const char* msg = env->GetStringUTFChars(message, nullptr);
    if (msg) {
        ChatSender::Send(msg);
        env->ReleaseStringUTFChars(message, msg);
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_autogg_AutoGGService_getMessage(JNIEnv* env, jobject /*thiz*/) {
    return env->NewStringUTF(Config::GetMessage().c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_autogg_AutoGGService_setMessage(JNIEnv* env, jobject /*thiz*/, jstring message) {
    const char* msg = env->GetStringUTFChars(message, nullptr);
    if (msg) {
        Config::SetMessage(msg);
        env->ReleaseStringUTFChars(message, msg);
    }
}

void* InitThread(void*) {
    LOGI("AutoGG Button mod loaded v1.0");
    
    Config::Load();
    
    if (ResolvePreloaderApi()) {
        LOGI("Preloader API resolved, hooks available");
    }
    
    FloatingButton::Start();
    
    g_initialized.store(true, std::memory_order_release);
    return nullptr;
}

__attribute__((constructor)) void ModConstructor() {
    pthread_t thread{};
    if (pthread_create(&thread, nullptr, &InitThread, nullptr) != 0) {
        LOGE("failed to create initialization thread");
        return;
    }
    pthread_detach(thread);
}

} // namespace
