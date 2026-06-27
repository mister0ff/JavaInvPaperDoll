#include <android/log.h>
#include <jni.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <cstring>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AutoGG", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AutoGG", __VA_ARGS__)

// ============================================================
// TUDO EM UM ARQUIVO - MOD SIMPLES
// Toque na tela = envia mensagem no chat
// ============================================================

using GlossInitFn = void (*)(bool);
using GlossHookFn = void* (*)(void*, void*, void**);
using MotionEventFromJavaFn = void (*)(JNIEnv*, jobject, void*);

static GlossInitFn g_glossInit = nullptr;
static GlossHookFn g_glossHook = nullptr;
static MotionEventFromJavaFn g_oldMotion = nullptr;
static void* g_motionHook = nullptr;
static JavaVM* g_vm = nullptr;

// Layout GameActivityMotionEvent (do seu mod original)
static const int SRC_OFF = 0x04;
static const int ACT_OFF = 0x08;
static const int PTR_OFF = 0x38;
static const int FIRST_PTR = 0x3C;
static const int TOOL_OFF = 0x04;
static const int X_OFF = 0x08;
static const int Y_OFF = 0x0C;

static const int SRC_TOUCH = 0x00001002;
static const int TOOL_FINGER = 1;
static const int ACT_DOWN = 0;

static int g_cooldown = 0;

// ============================================================
// ENVIA MENSAGEM NO CHAT VIA JNI (usando JVM capturada)
// ============================================================
static void SendChat(const char* msg) {
    if (!g_vm) { LOGE("No JVM captured"); return; }
    
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("No JNIEnv"); return;
    }
    
    jclass mc = env->FindClass("com/mojang/minecraftpe/MainActivity");
    if (!mc) { LOGE("No MainActivity class"); return; }
    
    jmethodID getInst = env->GetStaticMethodID(mc, "getInstance", "()Lcom/mojang/minecraftpe/MainActivity;");
    if (!getInst) { LOGE("No getInstance"); return; }
    
    jobject activity = env->CallStaticObjectMethod(mc, getInst);
    if (!activity) { LOGE("No activity"); return; }
    
    // Tenta enviar chat
    jmethodID send = env->GetMethodID(mc, "nativeSendChatMessage", "(Ljava/lang/String;)V");
    if (!send) send = env->GetMethodID(mc, "sendChatMessage", "(Ljava/lang/String;)V");
    if (!send) send = env->GetMethodID(mc, "sendMessage", "(Ljava/lang/String;)V");
    
    if (send) {
        jstring jmsg = env->NewStringUTF(msg);
        env->CallVoidMethod(activity, send, jmsg);
        env->DeleteLocalRef(jmsg);
        LOGI("CHAT SENT: %s", msg);
    } else {
        LOGE("No chat method found");
    }
}

// ============================================================
// HOOK DO TOUCH
// ============================================================
static void HookMotion(JNIEnv* env, jobject ev, void* out) {
    if (g_oldMotion) g_oldMotion(env, ev, out);
    if (!out) return;
    
    char* p = (char*)out;
    int src = *(int*)(p + SRC_OFF);
    int act = *(int*)(p + ACT_OFF) & 0xFF;
    int n = *(int*)(p + PTR_OFF);
    
    if (src != SRC_TOUCH || n < 1) return;
    
    char* ptr = p + FIRST_PTR;
    int tool = *(int*)(ptr + TOOL_OFF);
    if (tool != TOOL_FINGER) return;
    
    float x = *(float*)(ptr + X_OFF);
    float y = *(float*)(ptr + Y_OFF);
    
    // Só envia no ACT_DOWN (quando toca) com cooldown
    if (act == ACT_DOWN && g_cooldown <= 0) {
        g_cooldown = 30; // ~0.5s cooldown
        LOGI("TOUCH at (%.0f, %.0f) -> sending AUTO GG", x, y);
        SendChat("AUTO GG");
    }
    
    if (g_cooldown > 0) g_cooldown--;
}

// ============================================================
// JNI_OnLoad - CAPTURA A JVM AUTOMATICAMENTE
// ============================================================
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    LOGI("JNI_OnLoad: JVM captured");
    return JNI_VERSION_1_6;
}

// ============================================================
// INICIALIZAÇÃO
// ============================================================
static void* InitThread(void*) {
    LOGI("=== AutoGG Simple Mod v1.0 ===");
    LOGI("Touch anywhere on screen to send AUTO GG");
    
    // Espera preloader
    void* pre = nullptr;
    for (int i = 0; i < 120 && !pre; i++) {
        pre = dlopen("libpreloader.so", RTLD_NOW | 0x00004);
        if (!pre) usleep(250000);
    }
    if (!pre) { LOGE("No preloader"); return nullptr; }
    
    g_glossInit = (GlossInitFn)dlsym(pre, "GlossInit");
    g_glossHook = (GlossHookFn)dlsym(pre, "GlossHook");
    if (!g_glossInit || !g_glossHook) { LOGE("No Gloss exports"); return nullptr; }
    g_glossInit(true);
    
    // Hook motion event
    void* mc = dlopen("libminecraftpe.so", RTLD_NOW | 0x00004);
    if (!mc) { LOGE("No libminecraftpe"); return nullptr; }
    
    void* motion = dlsym(mc, "GameActivityMotionEvent_fromJava");
    if (!motion) { LOGE("No motion event"); return nullptr; }
    
    g_motionHook = g_glossHook(motion, (void*)HookMotion, (void**)&g_oldMotion);
    if (!g_motionHook || !g_oldMotion) {
        LOGE("Hook failed"); return nullptr;
    }
    
    LOGI("HOOK OK! Touch screen to send AUTO GG");
    return nullptr;
}

__attribute__((constructor))
void ModConstructor() {
    pthread_t t;
    pthread_create(&t, nullptr, InitThread, nullptr);
    pthread_detach(t);
}
