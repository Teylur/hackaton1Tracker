#include "VideoPlayer.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <opencv2/imgproc.hpp>

VideoPlayer::VideoPlayer() {}
VideoPlayer::~VideoPlayer() { Close(); }

bool VideoPlayer::LoadVideo(const std::string& path) {
    Close();
    if (!cap.open(path)) return false;
    videoLoaded = true;
    // Захватываем первый кадр
    cap >> currentFrame;
    if (currentFrame.empty()) {
        videoLoaded = false;
        return false;
    }
    cv::cvtColor(currentFrame, rgbFrame, cv::COLOR_BGR2RGB);
    CreateTexture();
    frameTime = cap.get(cv::CAP_PROP_POS_MSEC) / 1000.0;
    return true;
}

void VideoPlayer::Close() {
    playing = false;
    videoLoaded = false;
    DeleteTexture();
    if (cap.isOpened()) cap.release();
    currentFrame.release();
    rgbFrame.release();
    ClearOverlays();
}

void VideoPlayer::Play() { if (videoLoaded) playing = true; }
void VideoPlayer::Pause() { playing = false; }
void VideoPlayer::Stop() {
    playing = false;
    if (videoLoaded) {
        cap.set(cv::CAP_PROP_POS_FRAMES, 0);
        cap >> currentFrame;
        if (!currentFrame.empty()) {
            cv::cvtColor(currentFrame, rgbFrame, cv::COLOR_BGR2RGB);
            CreateTexture();
            frameTime = 0.0;
        }
    }
}

bool VideoPlayer::IsPlaying() const { return playing; }

void VideoPlayer::Seek(float position) {
    if (!videoLoaded) return;
    double totalFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
    double targetFrame = totalFrames * position;
    cap.set(cv::CAP_PROP_POS_FRAMES, targetFrame);
    cap >> currentFrame;
    if (!currentFrame.empty()) {
        cv::cvtColor(currentFrame, rgbFrame, cv::COLOR_BGR2RGB);
        CreateTexture();
        frameTime = cap.get(cv::CAP_PROP_POS_MSEC) / 1000.0;
    }
}

const cv::Mat& VideoPlayer::GetCurrentFrame() const {
    return currentFrame;
}

bool VideoPlayer::Update() {
    if (!videoLoaded || !playing) return false;
    cap >> currentFrame;
    if (currentFrame.empty()) {
        playing = false;   // конец видео
        return false;
    }
    cv::cvtColor(currentFrame, rgbFrame, cv::COLOR_BGR2RGB);
    CreateTexture();
    frameTime = cap.get(cv::CAP_PROP_POS_MSEC) / 1000.0;
    return true;
}

void VideoPlayer::Render() {
    if (!videoLoaded || currentFrame.empty()) return;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float vidW = (float)currentFrame.cols;
    float vidH = (float)currentFrame.rows;
    float scaleW = avail.x / vidW;
    float scaleH = avail.y / vidH;
    float scale = (scaleW < scaleH) ? scaleW : scaleH;
    ImVec2 imageSize(vidW * scale, vidH * scale);

    ImGui::Image((void*)(intptr_t)textureID, imageSize);

    // Оверлеи
    ImVec2 imagePos = ImGui::GetItemRectMin();
    float sx = scale;
    float sy = scale;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (const auto& rect : overlayBoxes) {
        float x1 = imagePos.x + rect.x * sx;
        float y1 = imagePos.y + rect.y * sy;
        float x2 = x1 + rect.width * sx;
        float y2 = y1 + rect.height * sy;
        drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2),
                          IM_COL32(boxColor[2], boxColor[1], boxColor[0], 255), 0.0f, 0, 2.0f);
    }

    if (trajectoryPoints.size() > 1) {
        for (size_t i = 0; i < trajectoryPoints.size() - 1; ++i) {
            ImVec2 p1(imagePos.x + trajectoryPoints[i].x * sx,
                      imagePos.y + trajectoryPoints[i].y * sy);
            ImVec2 p2(imagePos.x + trajectoryPoints[i+1].x * sx,
                      imagePos.y + trajectoryPoints[i+1].y * sy);
            drawList->AddLine(p1, p2,
                              IM_COL32(trajectoryColor[2], trajectoryColor[1], trajectoryColor[0], 255), 2.0f);
        }
    }

    for (const auto& [point, str] : overlayTexts) {
        ImVec2 pos(imagePos.x + point.x * sx, imagePos.y + point.y * sy);
        drawList->AddText(pos,
                          IM_COL32(textColor[2], textColor[1], textColor[0], 255),
                          str.c_str());
    }
}

int VideoPlayer::GetWidth() const { return videoLoaded ? currentFrame.cols : 0; }
int VideoPlayer::GetHeight() const { return videoLoaded ? currentFrame.rows : 0; }

void VideoPlayer::CreateTexture() {
    if (rgbFrame.empty()) return;
    if (textureID == 0) glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbFrame.cols, rgbFrame.rows,
                 0, GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
}

void VideoPlayer::DeleteTexture() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }
}

// Реализация методов оверлеев (без изменений)
void VideoPlayer::SetOverlayBoxes(const std::vector<cv::Rect>& boxes, const cv::Scalar& color) {
    overlayBoxes = boxes;
    boxColor = color;
}

void VideoPlayer::SetTrajectory(const std::vector<cv::Point2f>& points, const cv::Scalar& color) {
    trajectoryPoints = points;
    trajectoryColor = color;
}

void VideoPlayer::SetOverlayTexts(const std::vector<std::pair<cv::Point, std::string>>& texts,
                                  const cv::Scalar& color) {
    overlayTexts = texts;
    textColor = color;
}

void VideoPlayer::ClearOverlays() {
    overlayBoxes.clear();
    trajectoryPoints.clear();
    overlayTexts.clear();
}