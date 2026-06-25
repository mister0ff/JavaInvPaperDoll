#pragma once

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <android/log.h>
#include <cmath>
#include <cstring>

namespace MiniUI {

struct Vec2 { float x, y; };
struct Vec4 { float r, g, b, a; };

inline Vec4 Color(float r, float g, float b, float a) {
    return {r, g, b, a};
}

static GLuint g_Program = 0;
static GLuint g_VAO = 0;
static GLuint g_VBO = 0;
static int g_ScreenW = 0, g_ScreenH = 0;

static const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
out vec4 vColor;
uniform vec2 uResolution;
void main() {
    vec2 clip = (aPos / uResolution) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
    vColor = aColor;
})";

static const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
})";

static GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, "AutoGGButton", "Shader compile error: %s", log);
    }
    return shader;
}

static void Init(int w, int h) {
    if (g_Program != 0) return;
    g_ScreenW = w; g_ScreenH = h;
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    g_Program = glCreateProgram();
    glAttachShader(g_Program, vs);
    glAttachShader(g_Program, fs);
    glLinkProgram(g_Program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glGenVertexArrays(1, &g_VAO);
    glGenBuffers(1, &g_VBO);
    glBindVertexArray(g_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 6, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

static void DrawRect(float x, float y, float w, float h, Vec4 color) {
    float vertices[] = {
        x,     y,     color.r, color.g, color.b, color.a,
        x + w, y,     color.r, color.g, color.b, color.a,
        x,     y + h, color.r, color.g, color.b, color.a,
        x + w, y,     color.r, color.g, color.b, color.a,
        x + w, y + h, color.r, color.g, color.b, color.a,
        x,     y + h, color.r, color.g, color.b, color.a,
    };
    glUseProgram(g_Program);
    glUniform2f(glGetUniformLocation(g_Program, "uResolution"), (float)g_ScreenW, (float)g_ScreenH);
    glBindVertexArray(g_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

} // namespace MiniUI

