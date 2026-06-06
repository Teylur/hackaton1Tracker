#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

struct Alert {
    int id;
    double timestamp;      // время в секундах
    int frameNumber;
    float confidence;
    cv::Rect bbox;
    std::string status;    // "pending", "accepted", "rejected"
    std::string note;      // опционально: комментарий судьи

    Alert(int id_, double ts, int frame, float conf, cv::Rect box)
        : id(id_), timestamp(ts), frameNumber(frame),
          confidence(conf), bbox(box), status("pending"), note("") {}
};

class AlertSystem {
public:
    AlertSystem();

    // Добавление нового алерта (вызывается из main или детектора)
    void addAlert(const cv::Mat& currentFrame, float confidence, 
                  int frameNumber, double timestamp, cv::Rect bbox);

    // Рисование панели с алертами (вызывается из ImGuiManager)
    void drawAlertPanel();

    // Обработка кнопок Accept / Reject
    void processAcceptReject(int alertIndex, bool isAccepted);

    // Получить все алерты (для передачи в ImGuiManager)
    std::vector<Alert> getAllAlerts() const;

    // Сохранение принятых алертов для дообучения
    void saveAcceptedAlerts(const std::string& folder = "data/labeled/");

    int getSelectedAlertIndex() const { return selectedAlertIndex; }

private:
    std::vector<Alert> alerts;
    int nextId = 1;
    int selectedAlertIndex = -1;

    // Вспомогательная функция для сохранения сниппета
    void saveAlertSnippet(const Alert& alert, const cv::Mat& frame, 
                         const std::string& baseFolder);
};