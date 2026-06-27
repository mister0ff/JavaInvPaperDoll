cmake_minimum_required(VERSION 3.18.1)
project("stopwatch-hook")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# Configurações do Android / NDK
# ============================================
set(ANDROID_NATIVE_API_LEVEL 21)
set(CMAKE_ANDROID_ARCH_ABI armeabi-v7a)

# ============================================
# Flags de compilação
# ============================================
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -O3 -s -fno-exceptions -fno-rtti -fvisibility=hidden")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -O3 -s -fvisibility=hidden")

# ============================================
# Diretórios de include
# ============================================
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ImGui
    ${CMAKE_SOURCE_DIR}/ImGui/backends
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# ============================================
# Arquivos fonte
# ============================================
set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/ImGui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/ImGui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/ImGui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/ImGui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/ImGui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/ImGui/backends/imgui_impl_android.cpp
)

set(PL_SOURCES
    ${CMAKE_SOURCE_DIR}/pl/Hook.cpp
    ${CMAKE_SOURCE_DIR}/pl/Gloss.cpp
)

set(MAIN_SOURCES
    ${CMAKE_SOURCE_DIR}/main.cpp
)

# ============================================
# Biblioteca compartilhada
# ============================================
add_library(
    stopwatch
    SHARED
    ${MAIN_SOURCES}
    ${IMGUI_SOURCES}
    ${PL_SOURCES}
)

# ============================================
# Link de bibliotecas do sistema
# ============================================
target_link_libraries(
    stopwatch
    EGL
    GLESv3
    android
    log
    dl
)

# ============================================
# Propriedades
# ============================================
set_target_properties(stopwatch PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
