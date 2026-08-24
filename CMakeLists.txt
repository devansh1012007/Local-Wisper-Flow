cmake_minimum_required(VERSION 3.16)

project(LocalWisperFlow VERSION 0.1.0 LANGUAGES C CXX)

if(WIN32)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.25
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniaudio)
if(NOT TARGET miniaudio)
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE "${miniaudio_SOURCE_DIR}")
endif()

set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG        v1.9.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(whisper)

add_executable(local-wisper-flow
    src/main.cpp
    src/audio/miniaudio_impl.cpp
    src/audio/MiniaudioCapture.cpp
    src/stt/LocalWhisperProvider.cpp
)

target_include_directories(local-wisper-flow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(local-wisper-flow PRIVATE miniaudio whisper)

if(WIN32)
    target_sources(local-wisper-flow PRIVATE
        src/os/WindowsHotkeyListener.cpp
        src/os/WindowsTextInjector.cpp
    )
    target_link_libraries(local-wisper-flow PRIVATE user32)
endif()
