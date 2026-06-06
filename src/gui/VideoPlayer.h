#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <GLFW/glfw3.h>

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool LoadVideo(const std::string& path);
    void Close();
    void Play();
    void Pause();
    void Stop();
    bool IsPlaying() const;
    void Seek(float position);
    void SeekFrame(int frame);
    const cv::Mat& GetCurrentFrame() const;

    int GetTotalFrames() const;
    int GetCurrentFrameIndex() const;
    double GetFps() const;

    bool Update();
    void Render();

    int GetWidth() const;
    int GetHeight() const;

    void SetOverlayBoxes(const std::vector<cv::Rect>& boxes,
                         const cv::Scalar& color = cv::Scalar(0,255,0));
    void SetTrajectory(const std::vector<cv::Point2f>& points,
                       const cv::Scalar& color = cv::Scalar(255,0,0));
    void SetOverlayTexts(const std::vector<std::pair<cv::Point, std::string>>& texts,
                         const cv::Scalar& color = cv::Scalar(0,0,255));
    void ClearOverlays();

    static bool ExtractClip(const std::string& sourcePath,
                            int startFrame, int endFrame,
                            const std::string& outputPath,
                            double fps);

private:
    void UploadFrameToTexture();
    void DeleteTexture();
    void ResetFrameTimer();

    cv::VideoCapture cap;
    cv::Mat currentFrame;
    cv::Mat rgbFrame;
    GLuint textureID = 0;
    int texWidth = 0;
    int texHeight = 0;
    bool playing = false;
    bool videoLoaded = false;
    double frameTime = 0.0;
    int totalFrames = 0;
    int currentFrameIndex = 0;
    double fps = 30.0;

    std::chrono::steady_clock::time_point lastFrameClock;
    bool hasFrameClock = false;

    std::vector<cv::Rect> overlayBoxes;
    cv::Scalar boxColor;
    std::vector<cv::Point2f> trajectoryPoints;
    cv::Scalar trajectoryColor;
    std::vector<std::pair<cv::Point, std::string>> overlayTexts;
    cv::Scalar textColor;
};
