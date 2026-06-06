#include "AlertSystem.h"
#include "VideoPlayer.h"
#include "imgui.h"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

AlertSystem::AlertSystem() {
    fs::create_directories("data/labeled/accepted");
    fs::create_directories("data/labeled/rejected");
    fs::create_directories("data/alerts");
}

void AlertSystem::setVideoSource(const std::string& path, double fps, int totalFrames) {
    videoSourcePath = path;
    videoFps = fps > 0 ? fps : 30.0;
    videoTotalFrames = totalFrames;
}

void AlertSystem::addAlert(const cv::Mat& currentFrame, float confidence,
                           int frameNumber, double timestamp, cv::Rect bbox) {
    if (!alerts.empty() && (timestamp - alerts.back().timestamp < 1.0)) {
        return;
    }

    const int preFrames = static_cast<int>(2.0 * videoFps);
    const int postFrames = static_cast<int>(3.0 * videoFps);
    int clipStart = std::max(0, frameNumber - preFrames);
    int clipEnd = frameNumber + postFrames;
    if (videoTotalFrames > 0) {
        clipEnd = std::min(videoTotalFrames - 1, clipEnd);
    }

    const int alertId = nextId++;
    std::string clipPath = "data/alerts/alert_" + std::to_string(alertId) + ".mp4";

    Alert newAlert(alertId, timestamp, frameNumber, confidence, bbox,
                   clipStart, clipEnd, clipPath);

    if (!currentFrame.empty()) {
        currentFrame.copyTo(newAlert.thumbnail);
    }

    if (!attachClipToAlert(newAlert, currentFrame)) {
        std::cerr << u8"Не удалось извлечь клип для алёрта #" << alertId << std::endl;
        newAlert.clipPath.clear();
    }

    alerts.push_back(std::move(newAlert));

    const auto& saved = alerts.back();
    std::cout << u8"🚨 ALERT #" << saved.id
              << u8" | Frame: " << frameNumber
              << u8" | Time: " << std::fixed << std::setprecision(2) << timestamp << "s"
              << u8" | Conf: " << static_cast<int>(confidence * 100) << "%"
              << u8" | Clip: [" << saved.clipStartFrame << ".." << saved.clipEndFrame << "]"
              << std::endl;
}

bool AlertSystem::attachClipToAlert(Alert& alert, const cv::Mat& frame) {
    if (videoSourcePath.empty() || alert.clipPath.empty()) return false;

    bool ok = VideoPlayer::ExtractClip(
        videoSourcePath, alert.clipStartFrame, alert.clipEndFrame,
        alert.clipPath, videoFps);

    if (ok && frame.empty() && !alert.thumbnail.empty()) {
        return true;
    }

    if (ok && alert.thumbnail.empty() && !frame.empty()) {
        frame.copyTo(alert.thumbnail);
    }

    return ok;
}

void AlertSystem::drawAlertPanel() {
    ImGui::Begin(u8"🚨 Алёрты (Потенциальные выезды)", nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Text(u8"Всего алёртов: %d", static_cast<int>(alerts.size()));
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(alerts.size()); ++i) {
        auto& alert = alerts[i];

        std::stringstream ss;
        ss << u8"Alert #" << alert.id
           << u8" | " << std::fixed << std::setprecision(2) << alert.timestamp << "s"
           << u8" | Conf: " << static_cast<int>(alert.confidence * 100) << "%";
        if (!alert.clipPath.empty()) {
            ss << u8" | [видео]";
        }

        ImGui::PushID(i);

        if (ImGui::Selectable(ss.str().c_str(), selectedAlertIndex == i)) {
            selectedAlertIndex = i;
            showClipPopup = true;
            popupAlertIndex = i;
            clipNeedsLoad = true;
            openClipPopupRequested = true;
            if (onAlertSelected) {
                onAlertSelected(alert);
            }
        }

        ImGui::SameLine(0.0f, 15.0f);

        if (alert.status == "pending") {
            if (ImGui::SmallButton("+")) {
                processAcceptReject(i, true);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-")) {
                processAcceptReject(i, false);
            }
        } else {
            ImVec4 color = (alert.status == "accepted") ?
                           ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            ImGui::TextColored(color, "[%s]", alert.status.c_str());
        }

        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button(u8"💾 Сохранить все принятые алерты")) {
        saveAcceptedAlerts();
    }

    ImGui::End();
}

void AlertSystem::drawClipPopup(VideoPlayer& clipPlayer) {
    if (!showClipPopup || popupAlertIndex < 0 ||
        popupAlertIndex >= static_cast<int>(alerts.size())) {
        return;
    }

    const Alert& alert = alerts[popupAlertIndex];

    if (openClipPopupRequested) {
        ImGui::OpenPopup(u8"Клип алёрта");
        openClipPopupRequested = false;
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(u8"Клип алёрта", &showClipPopup,
                               ImGuiWindowFlags_NoScrollbar)) {
        if (clipNeedsLoad) {
            clipPlayer.Pause();
            if (!alert.clipPath.empty() && fs::exists(alert.clipPath)) {
                clipPlayer.LoadVideo(alert.clipPath);
                clipPlayer.Play();
            } else {
                std::cerr << u8"Клип не найден: " << alert.clipPath << std::endl;
            }
            clipNeedsLoad = false;
        }

        ImGui::Text(u8"Alert #%d | кадр %d | %.2fs | conf %d%%",
                    alert.id, alert.frameNumber, alert.timestamp,
                    static_cast<int>(alert.confidence * 100));
        ImGui::Text(u8"Отрезок: %.1fs до — %.1fs после (кадры %d–%d)",
                    2.0, 3.0, alert.clipStartFrame, alert.clipEndFrame);
        ImGui::Separator();

        ImGui::BeginChild(u8"ClipViewport", ImVec2(680, 380), true);
        if (clipPlayer.GetWidth() > 0) {
            if (clipPlayer.IsPlaying()) {
                clipPlayer.Update();
            }
            clipPlayer.Render();
        } else {
            ImGui::TextWrapped(u8"Клип недоступен. Проверьте data/alerts/.");
        }
        ImGui::EndChild();

        if (ImGui::Button(u8"▶ Play")) clipPlayer.Play();
        ImGui::SameLine();
        if (ImGui::Button(u8"⏸ Pause")) clipPlayer.Pause();
        ImGui::SameLine();
        if (ImGui::Button(u8"⏮ Restart")) {
            clipPlayer.Stop();
            clipPlayer.Play();
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"Закрыть")) {
            clipPlayer.Pause();
            showClipPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void AlertSystem::setOnAlertSelected(std::function<void(const Alert& alert)> callback) {
    onAlertSelected = std::move(callback);
}

void AlertSystem::setOnAlertAccepted(std::function<void(int frameNumber)> callback) {
    onAlertAccepted = std::move(callback);
}

void AlertSystem::processAcceptReject(int alertIndex, bool isAccepted) {
    if (alertIndex < 0 || alertIndex >= static_cast<int>(alerts.size())) return;

    auto& alert = alerts[alertIndex];
    alert.status = isAccepted ? "accepted" : "rejected";

    std::cout << (isAccepted ? u8"✅ Принят" : u8"❌ Отклонён")
              << u8" алерт #" << alert.id << std::endl;

    if (isAccepted && onAlertAccepted) {
        onAlertAccepted(alert.frameNumber);
    }
}

std::vector<Alert> AlertSystem::getAllAlerts() const {
    return alerts;
}

void AlertSystem::saveAcceptedAlerts(const std::string& baseFolder) {
    fs::create_directories(baseFolder + "/accepted/clips");
    fs::create_directories(baseFolder + "/accepted/thumbnails");

    int count = 0;
    for (const auto& alert : alerts) {
        if (alert.status != "accepted") continue;

        if (!alert.clipPath.empty() && fs::exists(alert.clipPath)) {
            std::string dest = baseFolder + "/accepted/clips/alert_" +
                               std::to_string(alert.id) + ".mp4";
            fs::copy_file(alert.clipPath, dest, fs::copy_options::overwrite_existing);
        }

        if (!alert.thumbnail.empty()) {
            std::string thumbPath = baseFolder + "/accepted/thumbnails/alert_" +
                                    std::to_string(alert.id) + ".jpg";
            cv::imwrite(thumbPath, alert.thumbnail);
        }

        count++;
    }
    std::cout << u8"💾 Сохранено " << count << u8" алертов для дообучения.\n";
}
