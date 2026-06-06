#include "AlertSystem.h"
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
    
    // Простая проверка на дубликаты (не добавляем слишком часто)
    if (!alerts.empty() && (timestamp - alerts.back().timestamp < 1.0)) {
        return; // пропускаем, если алерт был меньше секунды назад
    }

    alerts.push_back(newAlert);
    
    std::cout << "🚨 ALERT #" << newAlert.id 
              << " | Frame: " << frameNumber 
              << " | Conf: " << (int)(confidence * 100) << "%" << std::endl;
}

void AlertSystem::drawAlertPanel() {
    ImGui::Begin("Алёрты (Потенциальные выезды)", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Всего алёртов: %d", (int)alerts.size());
    ImGui::Separator();

    for (int i = 0; i < (int)alerts.size(); ++i) {
        auto& alert = alerts[i];
        
        std::string label = "Alert #" + std::to_string(alert.id) +
                           " | " + std::to_string(alert.timestamp).substr(0, 5) + "s" +
                           " | Conf: " + std::to_string((int)(alert.confidence * 100)) + "%";

        ImGui::PushID(i);
        
        if (ImGui::Selectable(label.c_str(), selectedAlertIndex == i)) {
            selectedAlertIndex = i;
            // Здесь можно отправить сигнал в ImGuiManager для перемотки видео
        }

        ImGui::SameLine();

        if (alert.status == "pending") {
            if (ImGui::Button("✅")) {
                processAcceptReject(i, true);
            }
            ImGui::SameLine();
            if (ImGui::Button("❌")) {
                processAcceptReject(i, false);
            }
        } else {
            ImGui::TextColored(alert.status == "accepted" ? 
                             ImVec4(0,1,0,1) : ImVec4(1,0,0,1), 
                             "[%s]", alert.status.c_str());
        }

        ImGui::PopID();
    }

    if (ImGui::Button("Сохранить все принятые алерты")) {
        saveAcceptedAlerts();
    }

    ImGui::End();
}

void AlertSystem::processAcceptReject(int alertIndex, bool isAccepted) {
    if (alertIndex < 0 || alertIndex >= (int)alerts.size()) return;
    
    auto& alert = alerts[alertIndex];
    alert.status = isAccepted ? "accepted" : "rejected";
    
    std::cout << (isAccepted ? "✅ Принят" : "❌ Отклонён") 
              << " алерт #" << alert.id << std::endl;
}

std::vector<Alert> AlertSystem::getAllAlerts() const {
    return alerts;
}

void AlertSystem::saveAcceptedAlerts(const std::string& baseFolder) {
    for (const auto& alert : alerts) {
        if (alert.status == "accepted") {
            // Здесь можно сохранить сниппет, если у тебя есть доступ к кадру
            std::cout << "Сохранён алерт #" << alert.id << " для дообучения" << std::endl;
        }
    }
}

void AlertSystem::saveAlertSnippet(const Alert& alert, const cv::Mat& frame, 
                                  const std::string& baseFolder) {
    // Пока заглушка — потом можно расширить
    std::string filename = baseFolder + "/alert_" + std::to_string(alert.id) + ".jpg";
    cv::imwrite(filename, frame);
}