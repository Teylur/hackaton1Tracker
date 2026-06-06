#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <functional>

class VideoPlayer;

struct Alert {
    int id;
    double timestamp;
    int frameNumber;
    float confidence;
    cv::Rect bbox;
    std::string status;
    std::string note;

    int clipStartFrame;
    int clipEndFrame;
    std::string clipPath;
    cv::Mat thumbnail;

    Alert(int id_, double ts, int frame, float conf, cv::Rect box,
          int clipStart, int clipEnd, std::string clip)
        : id(id_), timestamp(ts), frameNumber(frame),
          confidence(conf), bbox(box), status("pending"), note(""),
          clipStartFrame(clipStart), clipEndFrame(clipEnd), clipPath(std::move(clip)) {}
};

class AlertSystem {
public:
    AlertSystem();

    void setVideoSource(const std::string& path, double fps, int totalFrames);

    void addAlert(const cv::Mat& currentFrame, float confidence,
                  int frameNumber, double timestamp, cv::Rect bbox);

    void drawAlertPanel();
    void drawClipPopup(VideoPlayer& clipPlayer);

    void processAcceptReject(int alertIndex, bool isAccepted);

    std::vector<Alert> getAllAlerts() const;

    void saveAcceptedAlerts(const std::string& baseFolder = "data/labeled/");

    void setOnAlertSelected(std::function<void(const Alert& alert)> callback);
    void setOnAlertAccepted(std::function<void(int frameNumber)> callback);

    int getSelectedAlertIndex() const { return selectedAlertIndex; }

private:
    bool attachClipToAlert(Alert& alert, const cv::Mat& frame);

    std::vector<Alert> alerts;
    int nextId = 1;
    int selectedAlertIndex = -1;

    std::string videoSourcePath;
    double videoFps = 30.0;
    int videoTotalFrames = 0;

    bool showClipPopup = false;
    int popupAlertIndex = -1;
    bool clipNeedsLoad = false;
    bool openClipPopupRequested = false;

    std::function<void(const Alert& alert)> onAlertSelected;
    std::function<void(int frameNumber)> onAlertAccepted;
};
