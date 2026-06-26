cmake_minimum_required(VERSION 3.22)
project(AutoGGButton)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(ANDROID_LOG_LIB log)
find_library(ANDROID_DL_LIB dl)

add_library(${PROJECT_NAME} SHARED src/main.cpp)

target_link_libraries(${PROJECT_NAME}
    ${ANDROID_LOG_LIB}
    ${ANDROID_DL_LIB}
)

target_compile_options(${PROJECT_NAME} PRIVATE -O3 -fvisibility=hidden -Wall)
