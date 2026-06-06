#pragma once

#include "VideoPlayer.h"
#include "AlertSystem.h"
#include "YoloDetector.h"

#include <opencv2/core.hpp>
#include <filesystem>
#include <string>
#include <vector>

struct GLFWwindow;

class ImGuiManager {
public:
    ImGuiManager(const std::string& video_path, const std::filesystem::path& model_path);
    ~ImGuiManager();

    void run();

private:
    bool initGLFW();
    bool initImGui();
    void shutdown();

    void processFrame();
    void updateOverlays(const std::vector<Detection>& detections);
    void checkOffTrackAlerts(const std::vector<Detection>& detections, const cv::Mat& frame);

    void renderSidebar();
    void renderViewport();
    void renderTimeline();

    GLFWwindow* m_window = nullptr;
    bool m_initialized = false;

    VideoPlayer m_player;
    AlertSystem m_alertSystem;
    YoloDetector m_detector;

    std::string m_videoPath;
    int m_totalFrames = 0;
    int m_currentFrameIndex = 0;
    double m_fps = 30.0;

    bool m_showBoxes = true;
    bool m_showTrajectory = true;
    bool m_showAlertsOverlay = true;

    std::vector<cv::Point2f> m_trajectory;
    cv::Rect m_trackBBox;
    bool m_hasTrackBBox = false;
};
