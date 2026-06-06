#include "ImGuiManager.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <opencv2/videoio.hpp>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

ImGuiManager::ImGuiManager(const std::string& video_path, const fs::path& model_path)
    : m_detector(model_path), m_videoPath(video_path) {
    m_alertSystem.setOnAlertAccepted([this](int frameNumber) {
        m_player.SeekFrame(frameNumber);
        m_currentFrameIndex = frameNumber;
        m_player.Pause();
    });
}

ImGuiManager::~ImGuiManager() {
    shutdown();
}

bool ImGuiManager::initGLFW() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Добавлен префикс u8 для корректной кодировки заголовка окна
    m_window = glfwCreateWindow(1600, 900, u8"Ассистент судьи — Автогонки", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    return true;
}

bool ImGuiManager::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    // --- НАЧАЛО ИЗМЕНЕНИЙ ДЛЯ КИРИЛЛИЦЫ ---
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear(); // Очищаем шрифт по умолчанию (ProggyClean)

    ImFontConfig font_config;
    font_config.OversampleH = 2;
    font_config.OversampleV = 2;
    font_config.PixelSnapH = true;

    // Путь относительно папки build/, откуда обычно запускается программа
    const char* font_path = "third_party/imgui/misc/fonts/Roboto-Medium.ttf";
    
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        font_path, 
        18.0f, 
        &font_config, 
        io.Fonts->GetGlyphRangesCyrillic() // <-- Загружаем русские символы
    );

    if (!font) {
        std::cerr << "WARNING: Не удалось загрузить шрифт по пути: " << font_path << "\n";
        std::cerr << "Убедитесь, что вы запускаете программу из папки build/\n";
        // Фоллбек, чтобы программа не упала, но кириллица будет ???
        io.Fonts->AddFontDefault();
    }
    // --- КОНЕЦ ИЗМЕНЕНИЙ ДЛЯ КИРИЛЛИЦЫ ---

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    return true;
}

void ImGuiManager::run() {
    if (!initGLFW()) return;
    if (!initImGui()) return;
    m_initialized = true;

    if (!m_player.LoadVideo(m_videoPath)) {
        std::cerr << u8"Не удалось открыть видео: " << m_videoPath << std::endl;
        std::cerr << u8"Положите файл в data/videos/ или передайте путь аргументом.\n";
    } else {
        m_totalFrames = m_player.GetTotalFrames();
        m_fps = m_player.GetFps();
        m_player.Play();
    }

    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (m_player.IsPlaying()) {
            if (m_player.Update()) {
                processFrame();
                m_currentFrameIndex = m_player.GetCurrentFrameIndex();
            }
        }

        renderSidebar();
        renderViewport();
        ImGui::SetNextWindowPos(ImVec2(10, 340), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 360), ImGuiCond_FirstUseEver);
        m_alertSystem.drawAlertPanel();
        renderTimeline();

        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(m_window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }

    shutdown();
}

void ImGuiManager::shutdown() {
    if (!m_initialized) return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    m_initialized = false;
}

void ImGuiManager::processFrame() {
    cv::Mat frame = m_player.GetCurrentFrame();
    if (frame.empty()) return;

    auto detections = m_detector.detect(frame);
    updateOverlays(detections);
    checkOffTrackAlerts(detections, frame);
}

void ImGuiManager::updateOverlays(const std::vector<Detection>& detections) {
    if (!m_showBoxes) {
        m_player.ClearOverlays();
        return;
    }

    std::vector<cv::Rect> boxes;
    std::vector<std::pair<cv::Point, std::string>> texts;

    m_hasTrackBBox = false;
    cv::Point2f carCenter;

    for (const auto& det : detections) {
        boxes.push_back(det.bbox);
        texts.push_back({cv::Point(det.bbox.x, det.bbox.y - 5),
                         det.getClassName() + " " + std::to_string(static_cast<int>(det.confidence * 100)) + "%"});

        if (det.class_id == 0) {
            m_trackBBox = det.bbox;
            m_hasTrackBBox = true;
        }
        if (det.class_id == 2) {
            carCenter = det.center;
        }
    }

    if (m_showTrajectory && carCenter.x > 0 && carCenter.y > 0) {
        m_trajectory.push_back(carCenter);
        if (m_trajectory.size() > 500) {
            m_trajectory.erase(m_trajectory.begin());
        }
        m_player.SetTrajectory(m_trajectory);
    }

    m_player.SetOverlayBoxes(boxes);
    m_player.SetOverlayTexts(texts);
}

void ImGuiManager::checkOffTrackAlerts(const std::vector<Detection>& detections,
                                       const cv::Mat& frame) {
    if (!m_showAlertsOverlay || !m_hasTrackBBox) return;

    for (const auto& det : detections) {
        if (det.class_id != 1) continue;

        cv::Point2f wheelCenter = det.center;
        bool insideTrack = m_trackBBox.contains(cv::Point(static_cast<int>(wheelCenter.x),
                                                          static_cast<int>(wheelCenter.y)));
        if (!insideTrack) {
            double timestamp = m_currentFrameIndex / (m_fps > 0 ? m_fps : 30.0);
            m_alertSystem.addAlert(frame, det.confidence, m_currentFrameIndex, timestamp, det.bbox);
            break;
        }
    }
}

void ImGuiManager::renderSidebar() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 320), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"Управление", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button(u8"Play")) m_player.Play();
    ImGui::SameLine();
    if (ImGui::Button(u8"Pause")) m_player.Pause();
    ImGui::SameLine();
    if (ImGui::Button(u8"Stop")) {
        m_player.Stop();
        m_currentFrameIndex = 0;
        m_trajectory.clear();
    }

    ImGui::Separator();

    if (ImGui::Button(u8"Open test video") && fs::exists("data/videos/test.mp4")) {
        m_videoPath = "data/videos/test.mp4";
        if (m_player.LoadVideo(m_videoPath)) {
            m_totalFrames = m_player.GetTotalFrames();
            m_fps = m_player.GetFps();
            m_currentFrameIndex = 0;
            m_trajectory.clear();
            m_player.Play();
        }
    }

    ImGui::Separator();
    ImGui::Checkbox(u8"Показывать Bounding Box", &m_showBoxes);
    ImGui::Checkbox(u8"Показывать траекторию", &m_showTrajectory);
    ImGui::Checkbox(u8"Детектировать выезды", &m_showAlertsOverlay);

    ImGui::Text(u8"Видео: %s", m_videoPath.c_str());
    ImGui::Text(u8"Кадр: %d / %d", m_currentFrameIndex, m_totalFrames);

    ImGui::End();
}

void ImGuiManager::renderViewport() {
    ImGui::SetNextWindowPos(ImVec2(300, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1100, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"Видеоплеер", nullptr, ImGuiWindowFlags_NoCollapse);

    if (m_player.GetWidth() > 0) {
        m_player.Render();
    } else {
        ImGui::Text(u8"Видео не загружено");
        ImGui::TextWrapped(u8"Положите файл в data/videos/test.mp4 или запустите:");
        ImGui::TextWrapped(u8"./build/racing_gui path/to/video.mp4");
    }

    ImGui::End();
}

void ImGuiManager::renderTimeline() {
    ImGui::SetNextWindowPos(ImVec2(10, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1580, 80), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"Таймлайн", nullptr, ImGuiWindowFlags_NoCollapse);

    int maxFrame = m_totalFrames > 0 ? m_totalFrames - 1 : 0;
    int frame = m_currentFrameIndex;

    if (ImGui::SliderInt(u8"Кадр", &frame, 0, maxFrame)) {
        m_player.SeekFrame(frame);
        m_currentFrameIndex = frame;
        m_player.Pause();
        processFrame();
    }

    ImGui::End();
}