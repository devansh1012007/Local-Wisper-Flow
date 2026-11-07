cmake_minimum_required(VERSION 3.16)

project(LocalWisperFlow VERSION 0.1.0 LANGUAGES C CXX)

if(WIN32)
    add_compile_definitions(_USE_MATH_DEFINES)
    # windows.h defines global min/max macros unless NOMINMAX is defined,
    # which breaks std::min/std::max in any translation unit that pulls in
    # windows.h (e.g. glfw3native.h -> windows.h). Define it project-wide,
    # the same guard already used per-file in src/os/*.cpp.
    add_compile_definitions(NOMINMAX)
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING
    "Compat shim: vendored deps declare pre-3.5 minimums which CMake >= 4 rejects")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LWF_BUILD_FULL "Build the full app with whisper/llama/grpc/onnx providers" OFF)
if(WIN32)
    set(LWF_BUILD_FULL ON CACHE BOOL "" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)

# TonePlayer.cpp is part of lwf_core (always built) and #includes <miniaudio.h>,
# so miniaudio is a genuine core dependency: declare it unconditionally here,
# next to the other always-fetched deps. (It previously lived under
# if(LWF_BUILD_FULL), which broke non-FULL Linux builds lacking the
# gitignored _deps-src/ checkout.)
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

set(LWF_CORE_SOURCES
    src/core/AppContextResolver.cpp
    src/core/ApiKeyStore.cpp
    src/core/AtomicFile.cpp
    src/core/AudioGain.cpp
    src/core/CommandModeController.cpp
    src/core/ConfigManager.cpp
    src/core/ConsentGate.cpp
    src/core/SttRouter.cpp
    src/stt/cloud/ClipSplitter.cpp
    src/stt/cloud/WavEncoder.cpp
    src/stt/cloud/MultipartBuilder.cpp
    src/stt/cloud/GnaniRequest.cpp
    src/stt/cloud/GnaniResponse.cpp
    src/stt/cloud/GnaniProvider.cpp
    src/stt/cloud/HttpStatus.cpp
    src/stt/cloud/LanguageMap.cpp
    src/stt/cloud/ElevenLabsRequest.cpp
    src/stt/cloud/ElevenLabsResponse.cpp
    src/stt/cloud/ElevenLabsProvider.cpp
    src/stt/cloud/MuseRequest.cpp
    src/stt/cloud/MuseResponse.cpp
    src/stt/cloud/MuseProvider.cpp
    src/core/DictationCleanup.cpp
    src/core/DictationEngine.cpp
    src/core/CrashReporter.cpp
    src/core/DictationFSM.cpp
    src/core/DictionaryStore.cpp
    src/core/ErrorCode.cpp
    src/core/HotkeyBinding.cpp
    src/core/HotkeyTracker.cpp
    src/core/HistoryStore.cpp
    src/core/IClock.cpp
    src/core/InjectorDecorators.cpp
    src/core/InsightsModel.cpp
    src/core/Languages.cpp
    src/core/Log.cpp
    src/core/NotesStore.cpp
    src/core/PersonalDictionary.cpp
    src/core/OverlayModel.cpp
    src/core/PathResolver.cpp
    src/core/ProviderRouter.cpp
    src/core/RateLimiter.cpp
    src/core/SpokenCommandProcessor.cpp
    src/core/StatsStore.cpp
    src/core/Onboarding.cpp
    src/core/StreamingDictation.cpp
    src/core/SettingsStore.cpp
    src/core/SettingsUiModel.cpp
    src/core/Sha256.cpp
    src/core/SnippetStore.cpp
    src/core/TextDiff.cpp
    src/core/TextPipeline.cpp
    src/core/ToneGenerator.cpp
    src/core/TransformManager.cpp
    src/core/TransformStore.cpp
    src/core/UiBus.cpp
    src/audio/TonePlayer.cpp
)

add_library(lwf_core STATIC ${LWF_CORE_SOURCES})
# WindowsHttpClient.cpp unconditionally #includes <windows.h>/<winhttp.h>; like
# the src/os/* adapters it must stay out of the non-Windows build. Its only
# consumer (main.cpp) is inside #ifdef _WIN32, so gating here is behavior-neutral.
if(WIN32)
    target_sources(lwf_core PRIVATE src/stt/cloud/WindowsHttpClient.cpp)
endif()
target_include_directories(lwf_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(lwf_core PUBLIC LWF_APP_VERSION="${PROJECT_VERSION}")
target_link_libraries(lwf_core PUBLIC nlohmann_json::nlohmann_json miniaudio)

add_executable(local-wisper-flow src/main.cpp src/os/ActiveWindow.cpp src/os/CrashHandler.cpp src/os/DataDir.cpp src/os/WindowStyles.cpp)
target_link_libraries(local-wisper-flow PRIVATE lwf_core)

enable_testing()

add_executable(lwf_tests
    src/os/ActiveWindow.cpp
    src/os/CrashHandler.cpp
    src/os/DataDir.cpp
    src/os/WindowStyles.cpp
    tests/main.cpp
    tests/test_cleanup.cpp
    tests/test_appcontext.cpp
    tests/test_atomicfile.cpp
    tests/test_config_compat.cpp
    tests/consent_test.cpp
    tests/secrets_redaction_test.cpp
    tests/gnani_provider_test.cpp
    tests/stt_router_test.cpp
    tests/elevenlabs_provider_test.cpp
    tests/language_map_test.cpp
    tests/muse_provider_test.cpp
    tests/test_crash.cpp
    tests/test_engine.cpp
    tests/test_worker.cpp
    tests/test_fsm.cpp
    tests/test_gain.cpp
    tests/test_history.cpp
    tests/test_hotkey.cpp
    tests/test_insights.cpp
    tests/test_languages.cpp
    tests/test_errors.cpp
    tests/test_log.cpp
    tests/test_dictionary.cpp
    tests/test_dictionarystore.cpp
    tests/test_notesstore.cpp
    tests/test_onboarding.cpp
    tests/test_overlay_settings_ui.cpp
    tests/test_resilience.cpp
    tests/test_privacy.cpp
    tests/test_router.cpp
    tests/test_spoken.cpp
    tests/test_statsstore.cpp
    tests/test_streaming.cpp
    tests/test_settings.cpp
    tests/test_snippets.cpp
    tests/test_snippetstore_v2.cpp
    tests/test_textdiff.cpp
    tests/test_tones.cpp
    tests/test_transform.cpp
    tests/test_transform_runflow.cpp
    tests/test_transformstore.cpp
    tests/test_uibus.cpp
    tests/test_apikeystore.cpp
    tests/test_scratchpad_target.cpp
)
target_include_directories(lwf_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(lwf_tests PRIVATE LWF_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(lwf_tests PRIVATE lwf_core doctest::doctest)
add_test(NAME lwf-tests COMMAND lwf_tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(lwf-bench bench/lwf_bench.cpp)
target_link_libraries(lwf-bench PRIVATE lwf_core)

add_executable(lwf-cloud-bench bench/lwf_cloud_bench.cpp)
target_link_libraries(lwf-cloud-bench PRIVATE lwf_core)

if(LWF_BUILD_FULL)
    set(LLAMA_TAG      b10603)
    set(LLAMA_JOBS     4)
    set(LLAMA_INSTALL  "${CMAKE_BINARY_DIR}/llama-install")
    set(LLAMA_URL "https://github.com/ggml-org/llama.cpp/archive/refs/tags/${LLAMA_TAG}.tar.gz"
        CACHE STRING "llama.cpp source tarball (override with file:// for offline builds)")

    if(WIN32 AND NOT MSVC)
        set(LLAMA_GGML_FLAGS -DGGML_NATIVE=OFF -DGGML_OPENMP=OFF)
    else()
        set(LLAMA_GGML_FLAGS "")
    endif()

    include(ExternalProject)
    set(LLAMA_CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DBUILD_SHARED_LIBS=OFF
        -DLLAMA_BUILD_EXAMPLES=OFF
        -DLLAMA_BUILD_TESTS=OFF
        -DLLAMA_BUILD_SERVER=OFF
        -DLLAMA_BUILD_TOOLS=OFF
        -DLLAMA_BUILD_COMMON=OFF
        -DLLAMA_BUILD_APP=OFF
        -DLLAMA_CURL=OFF
    )
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND LLAMA_CMAKE_ARGS "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif()

    ExternalProject_Add(
        llama_ext
        URL               ${LLAMA_URL}
        DOWNLOAD_NAME     llama-${LLAMA_TAG}.tar.gz
        PREFIX            "${CMAKE_BINARY_DIR}/llama-ext"
        CMAKE_ARGS        ${LLAMA_CMAKE_ARGS} ${LLAMA_GGML_FLAGS}
            -DCMAKE_INSTALL_PREFIX=${LLAMA_INSTALL}
        BUILD_COMMAND  ${CMAKE_COMMAND} --build . --config Release --parallel ${LLAMA_JOBS}
        INSTALL_COMMAND ${CMAKE_COMMAND} --install . --config Release
        BUILD_BYPRODUCTS "${LLAMA_INSTALL}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}llama${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )

    if(EXISTS "${LLAMA_INSTALL}/lib/cmake/llama/llama-config.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${LLAMA_INSTALL}")
        find_package(ggml REQUIRED PATHS "${LLAMA_INSTALL}" NO_DEFAULT_PATH)
        find_package(llama REQUIRED PATHS "${LLAMA_INSTALL}" NO_DEFAULT_PATH)
        set(LLAMA_READY TRUE)
    else()
        file(MAKE_DIRECTORY "${LLAMA_INSTALL}/include")
        add_library(llama STATIC IMPORTED GLOBAL)
        set_target_properties(llama PROPERTIES
            IMPORTED_LOCATION "${LLAMA_INSTALL}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}llama${CMAKE_STATIC_LIBRARY_SUFFIX}"
            INTERFACE_INCLUDE_DIRECTORIES "${LLAMA_INSTALL}/include"
            MAP_IMPORTED_CONFIG_DEBUG Release
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
            MAP_IMPORTED_CONFIG_MINSIZEREL Release
        )
        message(STATUS "llama.cpp builds during this build - re-run cmake configure "
                       "afterwards so the full static-link closure is picked up")
    endif()

    set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
    if(LLAMA_READY)
        set(WHISPER_USE_SYSTEM_GGML ON CACHE BOOL "" FORCE)
    else()
        set(WHISPER_USE_SYSTEM_GGML OFF CACHE BOOL "" FORCE)
    endif()

    FetchContent_Declare(
        whisper
        GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
        GIT_TAG        v1.9.3
        GIT_SHALLOW    TRUE
        PATCH_COMMAND ${CMAKE_COMMAND} -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/patches/whisper-v1.9.3-ggml-alias.patch
                      -P ${CMAKE_CURRENT_SOURCE_DIR}/patches/apply_vendor_patch.cmake
    )
    FetchContent_MakeAvailable(whisper)

    set(GRPC_URL "https://github.com/grpc/grpc/archive/refs/tags/v1.71.0.tar.gz"
        CACHE STRING "gRPC source package; must include vendored third_party sources (see AGENTS.md recipe); pass file:// for offline builds")

    set(BUILD_TESTING            OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_TESTS         OFF CACHE BOOL "" FORCE)
    set(gRPC_BUILD_CSHARP_EXT    OFF CACHE BOOL "" FORCE)
    set(gRPC_INSTALL             OFF CACHE BOOL "" FORCE)
    set(gRPC_PROTOBUF_PROVIDER   "module" CACHE STRING "" FORCE)
    set(gRPC_ZLIB_PROVIDER       "module" CACHE STRING "" FORCE)
    set(gRPC_SSL_PROVIDER        "module" CACHE STRING "" FORCE)
    set(gRPC_CARES_PROVIDER      "module" CACHE STRING "" FORCE)
    set(protobuf_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
    set(protobuf_INSTALL         OFF CACHE BOOL "" FORCE)
    set(utf8_range_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    # MinGW + GCC cannot drive BoringSSL's vendored GNU-asm field-element
    # backends (the fiat_*.S files are ELF/Apple-gated, and BoringSSL's generated
    # NASM sources do not include them), which leaves bssl.exe with undefined
    # fiat_p256_adx_mul/sqr references. Fall back to the portable C field-element
    # arithmetic via OPENSSL_NO_ASM (the same fallback gRPC's Bazel config uses).
    # Reaching the subproject through CMAKE_C_FLAGS/CMAKE_CXX_FLAGS (rather than
    # only the subproject's own compiler-ID checks) keeps the flag visible to the
    # BoringSSL add_subdirectory scope.
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -DOPENSSL_NO_ASM")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOPENSSL_NO_ASM")

    FetchContent_Declare(
        grpc
        URL              ${GRPC_URL}
        DOWNLOAD_NAME    grpc-v1.71.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(grpc)

    set(GEN_DIR "${CMAKE_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${GEN_DIR}")

    add_custom_command(
        OUTPUT "${GEN_DIR}/inference.pb.cc" "${GEN_DIR}/inference.pb.h"
               "${GEN_DIR}/inference.grpc.pb.cc" "${GEN_DIR}/inference.grpc.pb.h"
        COMMAND $<TARGET_FILE:protoc>
                "--cpp_out=${GEN_DIR}"
                "--grpc_out=${GEN_DIR}"
                "--plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>"
                "-I${CMAKE_CURRENT_SOURCE_DIR}/proto"
                "${CMAKE_CURRENT_SOURCE_DIR}/proto/inference.proto"
        DEPENDS protoc grpc_cpp_plugin
        COMMENT "Generating gRPC stubs from inference.proto"
        VERBATIM
    )

    add_library(lwf_proto STATIC
        "${GEN_DIR}/inference.pb.cc"
        "${GEN_DIR}/inference.grpc.pb.cc"
    )
    target_include_directories(lwf_proto PUBLIC "${GEN_DIR}")
    target_link_libraries(lwf_proto PUBLIC grpc++ protobuf::libprotobuf)

    target_sources(local-wisper-flow PRIVATE
        src/audio/miniaudio_impl.cpp
        src/audio/MiniaudioCapture.cpp
        src/stt/LocalWhisperProvider.cpp
        src/stt/GrpcInferenceProvider.cpp
        src/llm/LocalLlamaProvider.cpp
    )
    add_dependencies(local-wisper-flow llama_ext)
    target_link_libraries(local-wisper-flow PRIVATE miniaudio whisper llama lwf_proto)

    if(WIN32)
        target_sources(local-wisper-flow PRIVATE
            src/os/ActiveWindow.cpp
            src/os/DataDir.cpp
            src/os/SingleInstance.cpp
            src/os/WindowsDownloader.cpp
            src/os/WindowsHotkeyListener.cpp
            src/os/WindowsTextInjector.cpp
            src/os/WindowStyles.cpp
            src/os/WindowsTray.cpp
            src/ui/MainWindow.cpp
            src/ui/Overlay.cpp
            src/ui/theme/ThemePalette.cpp
            src/ui/theme/StudioPalette.cpp
            src/ui/widgets/Buttons.cpp
            src/ui/widgets/Display.cpp
            src/ui/widgets/Overlays.cpp
        )
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/app.manifest")
            target_sources(local-wisper-flow PRIVATE source/app.manifest)
        endif()
        target_link_libraries(local-wisper-flow PRIVATE user32 wtsapi32 winhttp bcrypt shell32 advapi32 dwmapi)

        find_package(OpenGL REQUIRED)

        set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        3.4
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(glfw)

        FetchContent_Declare(
            imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG        v1.92.9b
            GIT_SHALLOW    TRUE
        )
        FetchContent_GetProperties(imgui)
        if(NOT imgui_POPULATED)
            FetchContent_Populate(imgui)
            add_library(imgui STATIC
                "${imgui_SOURCE_DIR}/imgui.cpp"
                "${imgui_SOURCE_DIR}/imgui_draw.cpp"
                "${imgui_SOURCE_DIR}/imgui_tables.cpp"
                "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
                "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
                "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
            )
            target_include_directories(imgui PUBLIC
                "${imgui_SOURCE_DIR}"
                "${imgui_SOURCE_DIR}/backends"
            )
            target_link_libraries(imgui PUBLIC glfw)
        endif()

        target_link_libraries(local-wisper-flow PRIVATE imgui OpenGL::GL)
    endif()

    set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime" CACHE PATH
        "Path to an extracted ONNX Runtime C++ distribution (must contain include/ and lib/)")

    if(EXISTS "${ONNXRUNTIME_DIR}/include/onnxruntime_cxx_api.h")
        add_library(onnxruntime SHARED IMPORTED)
        target_include_directories(onnxruntime INTERFACE "${ONNXRUNTIME_DIR}/include")
        if(WIN32)
            set_target_properties(onnxruntime PROPERTIES
                IMPORTED_IMPLIB "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib"
                IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
            )
        else()
            set_target_properties(onnxruntime PROPERTIES
                IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/libonnxruntime.so"
            )
        endif()

        add_executable(vad-selftest
            tests/vad_selftest.cpp
            src/vad/SileroVadProvider.cpp
        )
        target_include_directories(vad-selftest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(vad-selftest PRIVATE lwf_core onnxruntime)
        if(WIN32)
            add_custom_command(TARGET vad-selftest POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
                    $<TARGET_FILE_DIR:vad-selftest>
            )
        else()
            set_target_properties(vad-selftest PROPERTIES BUILD_RPATH "${ONNXRUNTIME_DIR}/lib")
        endif()
    else()
        message(STATUS "ONNX Runtime not found under '${ONNXRUNTIME_DIR}' - VAD provider and "
                       "vad-selftest skipped (see README section 'ONNX Runtime setup')")
    endif()
endif()
