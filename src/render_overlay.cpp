#include "render_overlay.h"
#include "imgui_minimal.h"
#include "config.h"
#include "chat_sender.h"
#include <android/log.h>
#include <GLES3/gl3.h>

namespace {
constexpr const char* kLogTag = "AutoGGButton";
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)

// Botão no canto superior direito
constexpr float kBtnX = 0.75f;
constexpr float kBtnY = 0.05f;
constexpr float kBtnW = 0.20f;
constexpr float kBtnH = 0.08f;

int g_ScreenW = 1080, g_ScreenH = 2400;
bool g_ButtonPressed = false;
bool g_WasPressed = false;
}

namespace RenderOverlay {

void SetScreenSize(int w, int h) { g_ScreenW = w; g_ScreenH = h; }

void Init(int width, int height) {
    g_ScreenW = width; g_ScreenH = height;
    MiniUI::Init(width, height);
    LOGI("Render overlay initialized: %dx%d", width, height);
}

bool IsInsideButton(float x, float y) {
    float bx = kBtnX * g_ScreenW;
    float by = kBtnY * g_ScreenH;
    float bw = kBtnW * g_ScreenW;
    float bh = kBtnH * g_ScreenH;
    return (x >= bx && x <= bx + bw && y >= by && y <= by + bh);
}

void SetButtonPressed(bool pressed) { g_ButtonPressed = pressed; }

void Render() {
    if (g_ScreenW <= 0 || g_ScreenH <= 0) return;
    
    // Backup GL state
    GLboolean blend, depth, cull;
    glGetBooleanv(GL_BLEND, &blend);
    glGetBooleanv(GL_DEPTH_TEST, &depth);
    glGetBooleanv(GL_CULL_FACE, &cull);
    GLint prevProg, prevVAO, prevVBO;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    // === BOTÃO AZUL ===
    float bx = kBtnX * g_ScreenW;
    float by = kBtnY * g_ScreenH;
    float bw = kBtnW * g_ScreenW;
    float bh = kBtnH * g_ScreenH;
    
    auto color = g_ButtonPressed 
        ? MiniUI::Color(0.1f, 0.3f, 0.8f, 1.0f)   // Pressionado
        : MiniUI::Color(0.2f, 0.5f, 1.0f, 0.9f);   // Normal
    
    MiniUI::DrawRect(bx, by, bw, bh, color);
    // Borda branca
    MiniUI::DrawRect(bx, by, bw, 3, MiniUI::Color(1,1,1,0.6f));
    MiniUI::DrawRect(bx, by+bh-3, bw, 3, MiniUI::Color(0,0,0,0.4f));
    MiniUI::DrawRect(bx, by, 3, bh, MiniUI::Color(1,1,1,0.6f));
    MiniUI::DrawRect(bx+bw-3, by, 3, bh, MiniUI::Color(0,0,0,0.4f));
    
    // Detecta click (soltou o dedo)
    if (g_WasPressed && !g_ButtonPressed) {
        LOGI("BUTTON CLICKED! Sending: %s", Config::GetMessage().c_str());
        ChatSender::Send(Config::GetMessage());
    }
    g_WasPressed = g_ButtonPressed;
    
    // Restore GL state
    glUseProgram(prevProg);
    glBindVertexArray(prevVAO);
    glBindBuffer(GL_ARRAY_BUFFER, prevVBO);
    if (!blend) glDisable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (cull) glEnable(GL_CULL_FACE);
}

} // namespace RenderOverlay

