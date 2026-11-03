#include "ui/OnboardingWizard.h"

#include "core/Onboarding.h"
#include "core/SettingsStore.h"

#include "imgui.h"

OnboardingWizard::OnboardingWizard(SettingsStore& settings) : settings_(settings) {}

bool OnboardingWizard::IsNeeded() const {
    return settings_.GetBool("first_run") ||
           settings_.GetString("whisper_model_path").empty();
}

bool OnboardingWizard::IsOpen() const {
    return step_ > 0;
}

void OnboardingWizard::Open() {
    step_ = 1;
}

void OnboardingWizard::SetMicLevel(float level) {
    mic_level_ = level;
}

void OnboardingWizard::SetDetectedHotkey(const std::string& chord) {
    detected_hotkey_ = chord;
}

void OnboardingWizard::RenderFrame() {
    if (!IsOpen()) {
        return;
    }
    ImGui::Begin("Welcome to Local Wisper Flow", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    if (step_ == 1) {
        ImGui::TextWrapped("Step 1: microphone test. Speak now - the meter should move.");
        ImGui::ProgressBar(mic_level_, ImVec2(-1.0f, 0.0f));
        if (mic_level_ > 0.05f && ImGui::Button("Microphone works")) {
            step_ = 2;
        }
        ImGui::SameLine();
        if (ImGui::Button("Open microphone settings")) {
            ImGui::SetTooltip("run: ms-settings:privacy-microphone");
        }
    } else if (step_ == 2) {
        ImGui::TextWrapped("Step 2: press your push-to-talk chord now.");
        ImGui::Text("detected: %s", detected_hotkey_.c_str());
        if (!detected_hotkey_.empty() && ImGui::Button("That is my hotkey")) {
            settings_.SetString("hotkey_ptt", detected_hotkey_);
            step_ = 3;
        }
    } else if (step_ == 3) {
        ImGui::TextWrapped("Step 3: models.");
        const Onboarding::ModelCatalog catalog = Onboarding::ParseModelsJson("models.json");
        for (const auto& model : catalog.whisper) {
            ImGui::Text("%s (%.0f MB)", model.name.c_str(),
                        static_cast<double>(model.size_bytes) / 1048576.0);
            if (model.sha256.empty()) {
                ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.2f, 1.0f), "unverified file");
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", model.url.c_str());
        }
        for (const auto& model : catalog.llm) {
            ImGui::Text("%s (%.0f MB)", model.name.c_str(),
                        static_cast<double>(model.size_bytes) / 1048576.0);
            if (model.sha256.empty()) {
                ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.2f, 1.0f), "unverified file");
            }
        }
        ImGui::TextWrapped("Downloads only start when you click Download. Files are verified "
                           "against the listed SHA-256 checksums.");
        if (ImGui::Button("Skip (I have models)")) {
            step_ = 4;
        }
    } else if (step_ == 4) {
        ImGui::TextWrapped("Step 4: try it! Hold the hotkey and read a sentence aloud.");
        if (ImGui::Button("Finish")) {
            settings_.SetBool("first_run", false);
            settings_.Save();
            step_ = 0;
        }
    }
    ImGui::End();
}
