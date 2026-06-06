#include "AlertSystem.h"
#include "imgui.h"              // ← только здесь
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

AlertSystem::AlertSystem() {
    fs::create_directories("data/labeled/accepted");
    fs::create_directories("data/labeled/rejected");
}

void AlertSystem::addAlert(const cv::Mat& currentFrame, float confidence,
                           int frameNumber, double timestamp, cv::Rect bbox) {
    
    Alert newAlert(nextId++, timestamp, frameNumber, confidence, bbox);

    // Защита от спама (не чаще 1 раза в секунду)
    if (!alerts.empty() && (timestamp - alerts.back().timestamp < 1.0)) {
        return;
    }

    alerts.push_back(newAlert);
    
    std::cout << "🚨 ALERT #" << newAlert.id 
              << " | Frame: " << frameNumber 
              << " | Time: " << std::fixed << std::setprecision(2) << timestamp << "s"
              << " | Conf: " << (int)(confidence * 100) << "%" << std::endl;
}

void AlertSystem::drawAlertPanel() {
    ImGui::Begin("🚨 Алёрты (Потенциальные выезды)", nullptr, 
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Всего алёртов: %d", (int)alerts.size());
    ImGui::Separator();

    for (int i = 0; i < (int)alerts.size(); ++i) {
        auto& alert = alerts[i];
        
        std::stringstream ss;
        ss << "Alert #" << alert.id 
           << " | " << std::fixed << std::setprecision(2) << alert.timestamp << "s"
           << " | Conf: " << (int)(alert.confidence * 100) << "%";

        ImGui::PushID(i);
        
        if (ImGui::Selectable(ss.str().c_str(), selectedAlertIndex == i)) {
            selectedAlertIndex = i;
            // TODO: Сюда потом добавим сигнал на перемотку видео
        }

        ImGui::SameLine(0.0f, 15.0f);

        if (alert.status == "pending") {
            if (ImGui::SmallButton("✅")) {
                processAcceptReject(i, true);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("❌")) {
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
    if (ImGui::Button("💾 Сохранить все принятые алерты")) {
        saveAcceptedAlerts();
    }

    ImGui::End();
}

void AlertSystem::setOnAlertAccepted(std::function<void(int frameNumber)> callback) {
    onAlertAccepted = std::move(callback);
}

void AlertSystem::processAcceptReject(int alertIndex, bool isAccepted) {
    if (alertIndex < 0 || alertIndex >= (int)alerts.size()) return;
    
    auto& alert = alerts[alertIndex];
    alert.status = isAccepted ? "accepted" : "rejected";
    
    std::cout << (isAccepted ? "✅ Принят" : "❌ Отклонён") 
              << " алерт #" << alert.id << std::endl;

    if (isAccepted && onAlertAccepted) {
        onAlertAccepted(alert.frameNumber);
    }
}

std::vector<Alert> AlertSystem::getAllAlerts() const {
    return alerts;
}

void AlertSystem::saveAcceptedAlerts(const std::string& baseFolder) {
    int count = 0;
    for (const auto& alert : alerts) {
        if (alert.status == "accepted") {
            count++;
            // saveAlertSnippet(...) — можно расширить позже
        }
    }
    std::cout << "💾 Сохранено " << count << " алертов для дообучения.\n";
}

void AlertSystem::saveAlertSnippet(const Alert& alert, const cv::Mat& frame, 
                                  const std::string& baseFolder) {
    std::string filename = baseFolder + "/alert_" + std::to_string(alert.id) + ".jpg";
    if (!frame.empty()) {
        cv::imwrite(filename, frame);
    }
}