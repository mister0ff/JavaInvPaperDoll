#include <jni.h>
#include <android/input.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <pthread.h>
#include <unistd.h>
#include <chrono>

#include "pl/Hook.h"
#include "pl/Gloss.h"

#include "ImGui/imgui.h"
#include "ImGui/backends/imgui_impl_opengl3.h"
#include "ImGui/backends/imgui_impl_android.h"

/* =========================
   Globals
   ========================= */
static bool g_Initialized = false;
static int g_Width = 0, g_Height = 0;

static EGLBoolean (*orig_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;

/* =========================
   Stopwatch State
   ========================= */
static bool g_Running = false;
static double g_Elapsed = 0.0;
static std::chrono::steady_clock::time_point g_LastTick;

/* =========================
   Input Hooks
   ========================= */
static void (*orig_Input1)(void*, void*, void*) = nullptr;
static int32_t (*orig_Input2)(void*, void*, bool, long, uint32_t*, AInputEvent**) = nullptr;

static void hook_Input1(void* thiz, void* a1, void* a2) {
    if (orig_Input1)
        orig_Input1(thiz, a1, a2);

    if (thiz && g_Initialized)
        ImGui_ImplAndroid_HandleInputEvent((AInputEvent*)thiz);
}

static int32_t hook_Input2(void* thiz, void* a1, bool a2, long a3,
                          uint32_t* a4, AInputEvent** event) {
    int32_t result = orig_Input2
        ? orig_Input2(thiz, a1, a2, a3, a4, event)
        : 0;

    if (result == 0 && event && *event && g_Initialized)
        ImGui_ImplAndroid_HandleInputEvent(*event);

    return result;
}

/* =========================
   Hook Input
   ========================= */
static void HookInput() {
    GHandle hInput = GlossOpen("libinput.so");
    if (!hInput) return;

    void* sym1 = (void*)GlossSymbol(
        hInput,
        "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE",
        nullptr);

    if (sym1)
        GlossHook(sym1, (void*)hook_Input1, (void**)&orig_Input1);

    void* sym2 = (void*)GlossSymbol(
        hInput,
        "_ZN7android13InputConsumer7consumeEPNS_26InputEventFactoryInterfaceEblPjPPNS_10InputEventE",
        nullptr);

    if (sym2)
        GlossHook(sym2, (void*)hook_Input2, (void**)&orig_Input2);
}

/* =========================
   GL State Backup
   ========================= */
struct GLState {
    GLint prog, tex, aTex, aBuf, eBuf, vao, fbo, vp[4], sc[4], bSrc, bDst;
    GLboolean blend, cull, depth, scissor;
};

static void SaveGL(GLState& s) {
    glGetIntegerv(GL_CURRENT_PROGRAM, &s.prog);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &s.tex);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &s.aTex);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s.aBuf);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &s.eBuf);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &s.vao);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s.fbo);
    glGetIntegerv(GL_VIEWPORT, s.vp);
    glGetIntegerv(GL_SCISSOR_BOX, s.sc);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &s.bSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &s.bDst);
    s.blend = glIsEnabled(GL_BLEND);
    s.cull = glIsEnabled(GL_CULL_FACE);
    s.depth = glIsEnabled(GL_DEPTH_TEST);
    s.scissor = glIsEnabled(GL_SCISSOR_TEST);
}

static void RestoreGL(const GLState& s) {
    glUseProgram(s.prog);
    glActiveTexture(s.aTex);
    glBindTexture(GL_TEXTURE_2D, s.tex);
    glBindBuffer(GL_ARRAY_BUFFER, s.aBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.eBuf);
    glBindVertexArray(s.vao);
    glBindFramebuffer(GL_FRAMEBUFFER, s.fbo);
    glViewport(s.vp[0], s.vp[1], s.vp[2], s.vp[3]);
    glScissor(s.sc[0], s.sc[1], s.sc[2], s.sc[3]);
    glBlendFunc(s.bSrc, s.bDst);
    s.blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    s.cull ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
    s.depth ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    s.scissor ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
}

/* =========================
   Stopwatch UI
   ========================= */
static void DrawMenu() {
    if (g_Running) {
        auto now = std::chrono::steady_clock::now();
        g_Elapsed += std::chrono::duration<double>(now - g_LastTick).count();
        g_LastTick = now;
    }

    int min = (int)(g_Elapsed / 60.0);
    int sec = ((int)g_Elapsed) % 60;
    int ms  = (int)((g_Elapsed - (int)g_Elapsed) * 100.0);

    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("Stopwatch", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // ⏱ TIME 텍스트 제거
    ImGui::Separator();
    ImGui::Text("%02d:%02d:%02d", min, sec, ms);

    ImGui::Spacing();

    if (!g_Running) {
        if (ImGui::Button("Start")) {
            g_Running = true;
            g_LastTick = std::chrono::steady_clock::now();
        }
    } else {
        if (ImGui::Button("Pause")) {
            g_Running = false;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset")) {
        g_Running = false;
        g_Elapsed = 0.0;
    }

    ImGui::End();
}

/* =========================
   ImGui Setup
   ========================= */
static void Setup() {
    if (g_Initialized || g_Width <= 0 || g_Height <= 0) return;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // ✅ 전체 UI 3배
    io.FontGlobalScale = 3.0f;
    ImGui::GetStyle().ScaleAllSizes(3.0f);

    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init("#version 300 es");

    // ✅ 원래 초록색 테마
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]      = ImVec4(0.05f, 0.05f, 0.05f, 0.85f);
    c[ImGuiCol_Button]        = ImVec4(0.1f, 0.6f, 0.1f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.1f, 1.0f, 0.1f, 1.0f);
    c[ImGuiCol_Text]          = ImVec4(0.9f, 1.0f, 0.9f, 1.0f);

    g_Initialized = true;
}

/* =========================
   Render & EGL Hook
   ========================= */
static void Render() {
    GLState s;
    SaveGL(s);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)g_Width, (float)g_Height);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(g_Width, g_Height);
    ImGui::NewFrame();

    DrawMenu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    RestoreGL(s);
}

static EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surf) {
    if (!orig_eglSwapBuffers)
        return EGL_FALSE;

    EGLint w, h;
    eglQuerySurface(dpy, surf, EGL_WIDTH, &w);
    eglQuerySurface(dpy, surf, EGL_HEIGHT, &h);

    if (w < 500 || h < 500)
        return orig_eglSwapBuffers(dpy, surf);

    g_Width = w;
    g_Height = h;

    Setup();
    Render();

    return orig_eglSwapBuffers(dpy, surf);
}

/* =========================
   Thread Init
   ========================= */
static void* MainThread(void*) {
    sleep(3);
    GlossInit(true);

    GHandle hEGL = GlossOpen("libEGL.so");
    if (!hEGL) return nullptr;

    void* swap = (void*)GlossSymbol(hEGL, "eglSwapBuffers", nullptr);
    if (!swap) return nullptr;

    GlossHook(swap, (void*)hook_eglSwapBuffers,
              (void**)&orig_eglSwapBuffers);

    HookInput();
    return nullptr;
}

__attribute__((constructor))
void Init() {
    pthread_t t;
    pthread_create(&t, nullptr, MainThread, nullptr);
}
