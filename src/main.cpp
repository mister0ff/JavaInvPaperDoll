#include <android/log.h>
#include <jni.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <cstring>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AutoGG", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AutoGG", __VA_ARGS__)

using GlossInitFn = void (*)(bool);
using GlossHookFn = void* (*)(void*, void*, void**);
using MotionEventFromJavaFn = void (*)(JNIEnv*, jobject, void*);

static GlossInitFn g_glossInit = nullptr;
static GlossHookFn g_glossHook = nullptr;
static MotionEventFromJavaFn g_oldMotion = nullptr;
static void* g_motionHook = nullptr;
static JavaVM* g_vm = nullptr;

// Layout GameActivityMotionEvent
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
// OBTEM MAINACTIVITY VIA mActivities DO ACTIVITYTHREAD
// ============================================================
static jobject GetMainActivity(JNIEnv* env) {
   if (!env) return nullptr;

   jclass activityThreadCls = env->FindClass("android/app/ActivityThread");
   if (!activityThreadCls) {
       LOGE("FindClass ActivityThread failed");
       return nullptr;
   }

   jmethodID currentActivityThread = env->GetStaticMethodID(
       activityThreadCls, "currentActivityThread", "()Landroid/app/ActivityThread;");
   if (!currentActivityThread) {
       LOGE("GetStaticMethodID currentActivityThread failed");
       env->DeleteLocalRef(activityThreadCls);
       return nullptr;
   }

   jobject activityThread = env->CallStaticObjectMethod(activityThreadCls, currentActivityThread);
   if (!activityThread) {
       LOGE("currentActivityThread returned null");
       env->DeleteLocalRef(activityThreadCls);
       return nullptr;
   }

   // Tentar ArrayMap primeiro (API 19+)
   jfieldID mActivitiesField = env->GetFieldID(
       activityThreadCls, "mActivities", "Landroid/util/ArrayMap;");
   
   // Fallback para HashMap (API antiga)
   if (!mActivitiesField) {
       env->ExceptionClear();
       mActivitiesField = env->GetFieldID(
           activityThreadCls, "mActivities", "Ljava/util/HashMap;");
   }

   if (!mActivitiesField) {
       LOGE("mActivities field not found");
       env->ExceptionClear();
       env->DeleteLocalRef(activityThread);
       env->DeleteLocalRef(activityThreadCls);
       return nullptr;
   }

   jobject mActivities = env->GetObjectField(activityThread, mActivitiesField);
   env->DeleteLocalRef(activityThread);
   env->DeleteLocalRef(activityThreadCls);

   if (!mActivities) {
       LOGE("mActivities is null");
       return nullptr;
   }

   // ArrayMap.values() ou HashMap.values()
   jclass mapCls = env->GetObjectClass(mActivities);
   jmethodID values = env->GetMethodID(mapCls, "values", "()Ljava/util/Collection;");
   env->DeleteLocalRef(mapCls);

   if (!values) {
       LOGE("values() method not found");
       env->ExceptionClear();
       env->DeleteLocalRef(mActivities);
       return nullptr;
   }

   jobject collection = env->CallObjectMethod(mActivities, values);
   env->DeleteLocalRef(mActivities);

   if (!collection) {
       LOGE("collection is null");
       return nullptr;
   }

   // Collection.toArray()
   jclass collectionCls = env->FindClass("java/util/Collection");
   jmethodID toArray = env->GetMethodID(collectionCls, "toArray", "()[Ljava/lang/Object;");
   env->DeleteLocalRef(collectionCls);

   if (!toArray) {
       LOGE("toArray method not found");
       env->ExceptionClear();
       env->DeleteLocalRef(collection);
       return nullptr;
   }

   jobjectArray arr = (jobjectArray)env->CallObjectMethod(collection, toArray);
   env->DeleteLocalRef(collection);

   if (!arr) {
       LOGE("toArray returned null");
       return nullptr;
   }

   jsize len = env->GetArrayLength(arr);
   jobject mainActivity = nullptr;

   for (jsize i = 0; i < len && !mainActivity; i++) {
       jobject record = env->GetObjectArrayElement(arr, i);
       if (!record) continue;

       jclass recordCls = env->GetObjectClass(record);
       
       // ActivityClientRecord.activity -> Activity
       jfieldID activityField = env->GetFieldID(
           recordCls, "activity", "Landroid/app/Activity;");
       env->DeleteLocalRef(recordCls);

       if (activityField) {
           jobject activity = env->GetObjectField(record, activityField);
           if (activity) {
               // Verificar se eh MainActivity
               jclass mainCls = env->FindClass("com/mojang/minecraftpe/MainActivity");
               if (mainCls) {
                   if (env->IsInstanceOf(activity, mainCls)) {
                       mainActivity = env->NewGlobalRef(activity); // ou NewLocalRef
                       LOGI("Found MainActivity at index %d", i);
                   }
                   env->DeleteLocalRef(mainCls);
               }
               env->DeleteLocalRef(activity);
           }
       }
       env->DeleteLocalRef(record);
   }

   env->DeleteLocalRef(arr);
   return mainActivity; // Retorna local ref, caller deve deletar
}

// ============================================================
// ENVIA MENSAGEM NO CHAT VIA JNI
// ============================================================
static void SendChat(const char* msg) {
   if (!g_vm) { LOGE("No JVM"); return; }

   JNIEnv* env = nullptr;
   jint attachResult = g_vm->GetEnv((void**)&env, JNI_VERSION_1_6);
   bool needsDetach = false;

   if (attachResult == JNI_EDETACHED) {
       JavaVMAttachArgs args = { JNI_VERSION_1_6, "AutoGGThread", nullptr };
       if (g_vm->AttachCurrentThread(&env, &args) != JNI_OK) {
           LOGE("AttachCurrentThread failed");
           return;
       }
       needsDetach = true;
   } else if (attachResult != JNI_OK) {
       LOGE("GetEnv failed: %d", attachResult);
       return;
   }

   if (!env) {
       LOGE("JNIEnv is null");
       return;
   }

   // Limpar qualquer excecao pendente antes de comecar
   if (env->ExceptionCheck()) {
       env->ExceptionClear();
   }

   jobject activity = GetMainActivity(env);
   if (!activity) {
       LOGE("Could not get MainActivity");
       if (needsDetach) g_vm->DetachCurrentThread();
       return;
   }

   jclass mainCls = env->FindClass("com/mojang/minecraftpe/MainActivity");
   if (!mainCls) {
       LOGE("FindClass MainActivity failed");
       env->ExceptionClear();
       env->DeleteLocalRef(activity);
       if (needsDetach) g_vm->DetachCurrentThread();
       return;
   }

   // Verificar se realmente eh instancia
   if (!env->IsInstanceOf(activity, mainCls)) {
       LOGE("Object is not instance of MainActivity");
       env->DeleteLocalRef(mainCls);
       env->DeleteLocalRef(activity);
       if (needsDetach) g_vm->DetachCurrentThread();
       return;
   }

   // Tentar metodos comuns de enviar chat
   jmethodID send = nullptr;
   const char* methodNames[] = {
       "nativeSendChatMessage",
       "sendChatMessage", 
       "sendMessage",
       "sendChat",
       nullptr
   };

   for (int i = 0; methodNames[i] && !send; i++) {
       send = env->GetMethodID(mainCls, methodNames[i], "(Ljava/lang/String;)V");
       if (send) {
           LOGI("Found chat method: %s", methodNames[i]);
       }
   }

   if (!send) {
       LOGE("No chat method found in MainActivity");
       env->ExceptionClear();
       env->DeleteLocalRef(mainCls);
       env->DeleteLocalRef(activity);
       if (needsDetach) g_vm->DetachCurrentThread();
       return;
   }

   jstring jmsg = env->NewStringUTF(msg);
   if (!jmsg) {
       LOGE("NewStringUTF failed");
       env->DeleteLocalRef(mainCls);
       env->DeleteLocalRef(activity);
       if (needsDetach) g_vm->DetachCurrentThread();
       return;
   }

   env->CallVoidMethod(activity, send, jmsg);

   // Verificar excecao apos chamada
   if (env->ExceptionCheck()) {
       LOGE("Exception after CallVoidMethod");
       env->ExceptionDescribe();
       env->ExceptionClear();
   } else {
       LOGI("CHAT SENT: %s", msg);
   }

   env->DeleteLocalRef(jmsg);
   env->DeleteLocalRef(mainCls);
   env->DeleteLocalRef(activity);

   if (needsDetach) {
       g_vm->DetachCurrentThread();
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

   if (act == ACT_DOWN && g_cooldown <= 0) {
       g_cooldown = 30;
       LOGI("TOUCH at (%.0f, %.0f) -> sending AUTO GG", x, y);
       SendChat("AUTO GG");
   }

   if (g_cooldown > 0) g_cooldown--;
}

// ============================================================
// JNI_OnLoad
// ============================================================
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
   g_vm = vm;
   LOGI("JNI_OnLoad: JVM captured");
   return JNI_VERSION_1_6;
}

// ============================================================
// INICIALIZACAO
// ============================================================
static void* InitThread(void*) {
   LOGI("=== AutoGG Simple Mod v1.3 ===");
   LOGI("Touch anywhere on screen to send AUTO GG");

   void* pre = nullptr;
   for (int i = 0; i < 120 && !pre; i++) {
       pre = dlopen("libpreloader.so", RTLD_NOW | RTLD_NOLOAD);
       if (!pre) usleep(250000);
   }
   if (!pre) { LOGE("No preloader"); return nullptr; }

   g_glossInit = (GlossInitFn)dlsym(pre, "GlossInit");
   g_glossHook = (GlossHookFn)dlsym(pre, "GlossHook");
   if (!g_glossInit || !g_glossHook) { LOGE("No Gloss exports"); return nullptr; }
   g_glossInit(true);

   void* mc = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
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
