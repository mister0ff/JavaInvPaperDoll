
#include "floating_button.h"
#include "config.h"
#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <string>

namespace {

constexpr const char* kLogTag = "AutoGGButton";

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)

JavaVM* g_vm = nullptr;
std::atomic<bool> g_running{false};
pthread_t g_buttonThread;

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

void* ButtonThread(void* /*arg*/) {
    JNIEnv* env = nullptr;
    if (g_vm->AttachCurrentThread(&env, nullptr) != 0) {
        LOGE("Failed to attach thread to JVM");
        return nullptr;
    }
    
    jclass activityClass = env->FindClass("com/mojang/minecraftpe/MainActivity");
    if (!activityClass) {
        LOGE("MainActivity not found");
        g_vm->DetachCurrentThread();
        return nullptr;
    }
    
    jmethodID getInstance = env->GetStaticMethodID(activityClass, "getInstance", "()Lcom/mojang/minecraftpe/MainActivity;");
    if (!getInstance) {
        LOGE("getInstance not found");
        g_vm->DetachCurrentThread();
        return nullptr;
    }
    
    jobject activity = env->CallStaticObjectMethod(activityClass, getInstance);
    if (!activity) {
        LOGE("Could not get MainActivity instance");
        g_vm->DetachCurrentThread();
        return nullptr;
    }
    
    LOGI("Attempting to create floating button...");
    
    // Aqui você injetaria o código Java do botão flutuante
    // via DexClassLoader ou reflection na Activity
    
    g_vm->DetachCurrentThread();
    return nullptr;
}

}

namespace FloatingButton {

void Start() {
    if (g_running.load(std::memory_order_acquire)) {
        return;
    }
    g_running.store(true, std::memory_order_release);
    
    usleep(3000 * 1000);
    
    pthread_create(&g_buttonThread, nullptr, ButtonThread, nullptr);
    pthread_detach(g_buttonThread);
}

void Stop() {
    g_running.store(false, std::memory_order_release);
}

} // namespace FloatingButton
