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
using MotionEventFromJavaFn = void (*)(JNIEnv*, jobject, std::byte*);

GlossInitFn g_glossInit = nullptr;
GlossHookFn g_glossHook = nullptr;
MotionEventFromJavaFn g_oldMotionEventFromJava = nullptr;
void* g_motionHook = nullptr;

constexpr std::size_t kEventSourceOffset = 0x04;
constexpr std::size_t kEventActionOffset = 0x08;
constexpr std::size_t kEventPointerCountOffset = 0x38;
constexpr std::size_t kFirstPointerOffset = 0x3C;
constexpr std::size_t kPointerStride = 0xD0;
constexpr std::size_t kPointerToolTypeOffset = 0x04;
constexpr std::size_t kPointerAxisXOffset = 0x08;
constexpr std::size_t kPointerAxisYOffset = 0x0C;

constexpr std::int32_t kActionMask = 0xFF;
constexpr std::int32_t kSourceTouchscreen = 0x00001002;
constexpr std::int32_t kToolFinger = 1;
constexpr std::int32_t kActionDown = 0;
constexpr std::int32_t kActionUp = 1;
constexpr std::int32_t kActionCancel = 3;

template <typename T>
T ReadValue(const std::byte* base, std::size_t offset) {
    T value{};
    std::memcpy(&value, base + offset, sizeof(T));
    return value;
}

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

void HookMotionEventFromJava(JNIEnv* env, jobject javaMotionEvent, std::byte* outEvent) {
    if (!g_oldMotionEventFromJava) return;
    g_oldMotionEventFromJava(env, javaMotionEvent, outEvent);
    if (!outEvent) return;

    const std::int32_t source = ReadValue<std::int32_t>(outEvent, kEventSourceOffset);
    const std::int32_t actionRaw = ReadValue<std::int32_t>(outEvent, kEventActionOffset);
    const std::int32_t action = actionRaw & kActionMask;
    const std::int32_t pointerCount = ReadValue<std::int32_t>(outEvent, kEventPointerCountOffset);
    
    if (source != kSourceTouchscreen || pointerCount < 1) return;

    const std::size_t pointer = kFirstPointerOffset;
    const std::int32_t tool = ReadValue<std::int32_t>(outEvent, pointer + kPointerToolTypeOffset);
    if (tool != kToolFinger) return;

    const float x = ReadValue<float>(outEvent, pointer + kPointerAxisXOffset);
    const float y = ReadValue<float>(outEvent, pointer + kPointerAxisYOffset);

    if (action == kActionDown && pointerCount == 1) {
        FloatingButton::HandleTouchDown(x, y);
    } else if ((action == kActionUp || action == kActionCancel) && pointerCount == 1) {
        FloatingButton::HandleTouchUp(x, y);
    }
}

bool InstallHooks() {
#ifndef RTLD_NOLOAD
#define RTLD_NOLOAD 0x00004
#endif
    void* minecraft = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
    if (!minecraft) { LOGE("libminecraftpe.so not loaded"); return false; }

    void* motionTarget = dlsym(minecraft, "GameActivityMotionEvent_fromJava");
    if (!motionTarget) { LOGE("GameActivityMotionEvent_fromJava not found"); return false; }

    g_motionHook = g_glossHook(
        motionTarget,
        reinterpret_cast<void*>(&HookMotionEventFromJava),
        reinterpret_cast<void**>(&g_oldMotionEventFromJava));
    
    if (!g_motionHook || !g_oldMotionEventFromJava) {
        LOGE("Motion event hook failed");
        return false;
    }

    LOGI("AutoGG loaded! Touch top-right corner to send message.");
    return true;
}

void* InitThread(void*) {
    LOGI("AutoGG Button v1.1 (touch area mode)");
    Config::Load();
    FloatingButton::Start();
    
    if (ResolvePreloaderApi()) {
        InstallHooks();
    }
    return nullptr;
}

} // namespace

__attribute__((constructor)) void ModConstructor() {
    pthread_t thread{};
    if (pthread_create(&thread, nullptr, &InitThread, nullptr) != 0) {
        LOGE("failed to create init thread"); return;
    }
    pthread_detach(thread);
}
