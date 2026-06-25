#include <android/log.h>
#include <jni.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>
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
JavaVM* g_vm = nullptr;

bool ResolvePreloaderApi() {
#ifndef RTLD_NOLOAD
#define RTLD_NOLOAD 0x00004
#endif
    void* preloader = nullptr;
    for (int attempt = 0; attempt < 120 && preloader == nullptr; ++attempt) {
        preloader = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD);
        if (!preloader) usleep(250 * 1000);
    }
    if (!preloader) { LOGE("libpreloader.so not loaded"); return false; }

    g_glossInit = reinterpret_cast<GlossInitFn>(dlsym(preloader, "GlossInit"));
    g_glossHook = reinterpret_cast<GlossHookFn>(dlsym(preloader, "GlossHook"));
    if (!g_glossInit || !g_glossHook) { LOGE("preloader exports missing"); return false; }
    g_glossInit(true);
    return true;
}

void* InitThread(void*) {
    LOGI("AutoGG Button mod loaded v1.0");
    Config::Load();
    if (ResolvePreloaderApi()) LOGI("Preloader API resolved");
    
    if (g_vm) {
        FloatingButton::SetJVM(g_vm);
        ChatSender::SetJVM(g_vm);
    }
    FloatingButton::Start();
    g_initialized.store(true);
    return nullptr;
}
}

void OnJVMReady(JavaVM* vm) { g_vm = vm; }

__attribute__((constructor)) void ModConstructor() {
    pthread_t thread{};
    if (pthread_create(&thread, nullptr, &InitThread, nullptr) != 0) {
        LOGE("failed to create init thread"); return;
    }
    pthread_detach(thread);
}
