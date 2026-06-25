#pragma once

namespace FloatingButton {
    void Start();
    void Stop();
    bool IsInsideButtonArea(float x, float y);
    void HandleTouchDown(float x, float y);
    void HandleTouchUp(float x, float y);
}
