#include "floating_button.h"
#include "config.h"
#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>

namespace {
constexpr const char* kLogTag = "AutoGGButton";
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)
JavaVM* g_vm = nullptr;
std::atomic<bool> g_running{false};
pthread_t g_buttonThread;
}

namespace FloatingButton {
void SetJVM(JavaVM* vm) { g_vm = vm; }

void* ButtonThread(void*) {
    if (!g_vm) { LOGE("JVM not set"); return nullptr; }
    
    JNIEnv* env = nullptr;
    if (g_vm->AttachCurrentThread(&env, nullptr) != 0) {
        LOGE("Failed to attach thread"); return nullptr;
    }
    
    jclass activityClass = env->FindClass("com/mojang/minecraftpe/MainActivity");
    if (!activityClass) { LOGE("MainActivity not found"); g_vm->DetachCurrentThread(); return nullptr; }
    
    jmethodID getInstance = env->GetStaticMethodID(activityClass, "getInstance", "()Lcom/mojang/minecraftpe/MainActivity;");
    if (!getInstance) { LOGE("getInstance not found"); g_vm->DetachCurrentThread(); return nullptr; }
    
    jobject activity = env->CallStaticObjectMethod(activityClass, getInstance);
    if (!activity) { LOGE("No activity"); g_vm->DetachCurrentThread(); return nullptr; }
    
    LOGI("Floating button ready");
    
    // Teste: mostra Toast
    jclass toastClass = env->FindClass("android/widget/Toast");
    jmethodID makeText = env->GetStaticMethodID(toastClass, "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
    jstring msg = env->NewStringUTF("AutoGG Button Loaded!");
    jobject toast = env->CallStaticObjectMethod(toastClass, makeText, activity, msg, 1);
    jmethodID show = env->GetMethodID(toastClass, "show", "()V");
    env->CallVoidMethod(toast, show);
    env->DeleteLocalRef(msg);
    
    g_vm->DetachCurrentThread();
    return nullptr;
}

void Start() {
    if (g_running.load()) return;
    g_running.store(true);
    usleep(5000 * 1000);
    pthread_create(&g_buttonThread, nullptr, ButtonThread, nullptr);
    pthread_detach(g_buttonThread);
}

void Stop() { g_running.store(false); }
}
