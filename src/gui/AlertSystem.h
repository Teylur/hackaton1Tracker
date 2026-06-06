#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

struct Alert {
    int id;
    double timestamp;      // в секундах
    int frameNumber;
    float confidence;
    cv::Rect bbox;
    std::string status;    // "pending", "accepted", "rejected"
    std::string note;

    Alert(int id_, double ts, int frame, float conf, cv::Rect box)
        : id(id_), timestamp(ts), frameNumber(frame),
          confidence(conf), bbox(box), status("pending"), note("") {}
};

class AlertSystem {
public:
    AlertSystem();

    void addAlert(const cv::Mat& currentFrame, float confidence,
                  int frameNumber, double timestamp, cv::Rect bbox);

    void drawAlertPanel();                    // Основная функция отрисовки

    void processAcceptReject(int alertIndex, bool isAccepted);

    std::vector<Alert> getAllAlerts() const;

    void saveAcceptedAlerts(const std::string& baseFolder = "data/labeled/");

    int getSelectedAlertIndex() const { return selectedAlertIndex; }

private:
    std::vector<Alert> alerts;
    int nextId = 1;
    int selectedAlertIndex = -1;

    void saveAlertSnippet(const Alert& alert, const cv::Mat& frame, 
                         const std::string& baseFolder);
};