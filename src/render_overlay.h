#pragma once

namespace RenderOverlay {
    void Init(int width, int height);
    void Render();
    void SetButtonPressed(bool pressed);
    bool IsInsideButton(float x, float y);
    void SetScreenSize(int w, int h);
}
