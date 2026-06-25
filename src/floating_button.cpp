#include "floating_button.h"
#include "config.h"
#include "chat_sender.h"
#include <android/log.h>
#include <atomic>
#include <chrono>

namespace {

constexpr const char* kLogTag = "AutoGGButton";
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)

// Área do "botão virtual": canto superior direito
// Ajuste conforme sua tela! (valores em % da tela)
constexpr float kButtonX = 0.80f;      // 80% da largura
constexpr float kButtonY = 0.02f;      // 2% da altura (topo)
constexpr float kButtonWidth = 0.18f;   // 18% da largura
constexpr float kButtonHeight = 0.10f;  // 10% da altura

constexpr int64_t kLongPressMs = 600;

std::atomic<bool> g_touchingButton{false};
std::atomic<int64_t> g_touchStartTime{0};

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

namespace FloatingButton {

bool IsInsideButtonArea(float x, float y) {
    // Assumindo tela 1080x2400 como base - ajuste se necessário
    float screenW = 1080.0f;
    float screenH = 2400.0f;
    
    float btnLeft = kButtonX * screenW;
    float btnRight = (kButtonX + kButtonWidth) * screenW;
    float btnTop = kButtonY * screenH;
    float btnBottom = (kButtonY + kButtonHeight) * screenH;
    
    return (x >= btnLeft && x <= btnRight && y >= btnTop && y <= btnBottom);
}

void HandleTouchDown(float x, float y) {
    if (IsInsideButtonArea(x, y)) {
        g_touchingButton.store(true);
        g_touchStartTime.store(NowMs());
        LOGI("Touch START in button area (%.0f, %.0f)", x, y);
    }
}

void HandleTouchUp(float x, float y) {
    if (!g_touchingButton.load()) return;
    
    int64_t duration = NowMs() - g_touchStartTime.load();
    g_touchingButton.store(false);
    
    if (duration >= kLongPressMs) {
        LOGI("LONG PRESS (%lld ms) - Config mode", duration);
        // TODO: abrir dialog de config
        ChatSender::Send("/say [AutoGG] Config: " + Config::GetMessage());
    } else {
        LOGI("CLICK (%lld ms) - Sending message!", duration);
        ChatSender::Send(Config::GetMessage());
    }
}

void Start() {
    LOGI("AutoGG touch area active!");
    LOGI("Touch the TOP-RIGHT corner of screen to send message");
    LOGI("Area: X=%.0f%%-%.0f%%, Y=%.0f%%-%.0f%%", 
         kButtonX*100, (kButtonX+kButtonWidth)*100,
         kButtonY*100, (kButtonY+kButtonHeight)*100);
}

void Stop() {
    g_touchingButton.store(false);
}

} // namespace FloatingButton
