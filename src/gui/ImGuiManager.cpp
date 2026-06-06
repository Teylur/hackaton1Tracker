#include "ImGuiManager.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>

// Конструктор / Деструктор
ImGuiManager::ImGuiManager() = default;
ImGuiManager::~ImGuiManager() { shutdown(); }

// ==================== ИНИЦИАЛИЗАЦИЯ ====================

bool ImGuiManager::initGLFW() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    m_window = glfwCreateWindow(1280, 720, "Ассистент судьи - Автогонки", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create window\n";
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    return true;
}

bool ImGuiManager::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*)m_window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    return true;
}

void ImGuiManager::run() {
    if (!initGLFW()) return;
    if (!initImGui()) return;
    m_initialized = true;

    while (!glfwWindowShouldClose((GLFWwindow*)m_window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderDockSpace();
        renderSidebar();
        renderViewport();
        renderAlertsPanel();
        renderTimeline();

        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize((GLFWwindow*)m_window, &w, &h);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers((GLFWwindow*)m_window);
    }

    shutdown();
}

void ImGuiManager::shutdown() {
    if (m_videoTexture) {
        glDeleteTextures(1, &m_videoTexture);
        m_videoTexture = 0;
    }
    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow((GLFWwindow*)m_window);
        glfwTerminate();
        m_window = nullptr;
    }
}

// ==================== ОБНОВЛЕНИЕ ТЕКСТУРЫ ====================

void ImGuiManager::updateTexture() {
    if (m_currentFrame.empty()) return;

    cv::Mat rgb;
    cv::cvtColor(m_currentFrame, rgb, cv::COLOR_BGR_RGB);

    if (m_videoTexture == 0) {
        glGenTextures(1, &m_videoTexture);
        glBindTexture(GL_TEXTURE_2D, m_videoTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glBindTexture(GL_TEXTURE_2D, m_videoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    m_textureWidth = rgb.cols;
    m_textureHeight = rgb.rows;
}

// ==================== РЕНДЕР ПАНЕЛЕЙ ====================

void ImGuiManager::renderDockSpace() {
    ImGui::DockSpaceOverViewport();
}

void ImGuiManager::renderSidebar() {
    ImGui::Begin("Управление", nullptr, ImGuiWindowFlags_NoCollapse);

    // Кнопки управления
    if (ImGui::Button("▶ Play")) play();
    ImGui::SameLine();
    if (ImGui::Button("⏸ Pause")) pause();
    ImGui::SameLine();
    if (ImGui::Button("⏹ Stop")) stop();
    ImGui::SameLine();
    if (ImGui::Button("📂 Open Video")) {
        // Простой вариант: открыть тестовое видео из папки
        if (std::filesystem::exists("test_video.mp4")) {
            openVideo("test_video.mp4");
        }
    }

    ImGui::Separator();

    ImGui::Checkbox("Показывать Bounding Box", &showBBoxes);
    ImGui::Checkbox("Показыватьтраекторию", &showTrajectory);
    ImGui::Checkbox("Показывать алерты на видео", &showAlertsOverlay);

    ImGui::End();
}

void ImGuiManager::renderViewport() {
    ImGui::Begin("Видеоплеер", nullptr, ImGuiWindowFlags_NoCollapse);

    if (m_videoTexture && !m_currentFrame.empty()) {
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        ImGui::Image((void*)(intptr_t)m_videoTexture, viewportSize);

        // Рисуем Bounding Box поверх видео
        if (showBBoxes && m_currentBBox.width > 0 && m_currentBBox.height > 0) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 imagePos = ImGui::GetCursorScreenPos();
            float scaleX = viewportSize.x / m_textureWidth;
            float scaleY = viewportSize.y / m_textureHeight;

            ImVec2 p1(imagePos.x + m_currentBBox.x * scaleX, 
                       imagePos.y + m_currentBBox.y * scaleY);
            ImVec2 p2(p1.x + m_currentBBox.width * scaleX, 
                      p1.y + m_currentBBox.height * scaleY);
            drawList->AddRect(p1, p2, IM_COL32(0, 255, 0, 255), 2.0f);
        }

        // Рисуем траекторию
        if (showTrajectory && m_trajectory.size() > 1) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 imagePos = ImGui::GetCursorScreenPos();
            float scaleX = viewportSize.x / m_textureWidth;
            float scaleY = viewportSize.y / m_textureHeight;

            for (size_t i = 1; i < m_trajectory.size(); i++) {
                ImVec2 from(imagePos.x + m_trajectory[i-1].x * scaleX,
                            imagePos.y + m_trajectory[i-1].y * scaleY);
                ImVec2 to(imagePos.x + m_trajectory[i].x * scaleX,
                          imagePos.y + m_trajectory[i].y * scaleY);
                drawList->AddLine(from, to, IM_COL32(255, 255, 0, 255), 2.0f);
            }
        }
    } else {
        ImGui::Text("Видео не загружено");
    }

    ImGui::End();
}

void ImGuiManager::renderAlertsPanel() {
    ImGui::Begin("Алерты", nullptr, ImGuiWindowFlags_NoCollapse);

    if (m_alerts.empty()) {
        ImGui::Text("Нет алертов");
    } else {
        for (auto& alert : m_alerts) {
            if (alert.accepted || alert.rejected) continue;

            ImGui::Text("⚠ Alert #%d - %.2f сек - %.0f%%", 
                        alert.id, alert.timestamp, alert.confidence * 100);
            ImGui::SameLine();
            if (ImGui::SmallButton(("Принять##" + std::to_string(alert.id)).c_str())) {
                alert.accepted = true;
                jumpToFrame(alert.start_frame);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(("Отклонить##" + std::to_string(alert.id)).c_str())) {
                alert.rejected = true;
            }
            ImGui::Separator();
        }
    }

    ImGui::End();
}

void ImGuiManager::renderTimeline() {
    ImGui::Begin("Таймлайн", nullptr, ImGuiWindowFlags_NoCollapse);

    int frame = m_currentFrame;
    if (ImGui::SliderInt("Кадр", &frame, 0, m_totalFrames > 0 ? m_totalFrames : 1000)) {
        onSeek(frame);
    }

    ImGui::End();
}

// ==================== ПУБЛИЧНЫЕ МЕТОДЫ ====================

void ImGuiManager::setCurrentFrame(const cv::Mat& frame) {
    frame.copyTo(m_currentFrame);
    updateTexture();
}

void ImGuiManager::setCarBBox(const cv::Rect& bbox) {
    m_currentBBox = bbox;
}

void ImGuiManager::setCarPosition(const cv::Point2f& pos) {
    m_currentPosition = pos;
}

void ImGuiManager::setTrajectory(const std::vector<cv::Point2f>& trajectory) {
    m_trajectory = trajectory;
}

void ImGuiManager::setAlerts(const std::vector<Alert>& alerts) {
    m_alerts = alerts;
}

void ImGuiManager::play() {
    m_playing = true;
}

void ImGuiManager::pause() {
    m_playing = false;
}

void ImGuiManager::stop() {
    m_playing = false;
    m_currentFrame = 0;
}

void ImGuiManager::jumpToFrame(int frameId) {
    m_currentFrame = frameId;
    //Здесь будет вызов к бэкенду для загрузки кадра
}

void ImGuiManager::openVideo(const std::string& path) {
    m_currentVideoPath = path;
    cv::VideoCapture cap(path);
    if (cap.isOpened()) {
        m_totalFrames = (int)cap.get(cv::CAP_PROP_FRAME_COUNT);
        m_fps = cap.get(cv::CAP_PROP_FPS);
        cap.release();
    }
    m_currentFrame = 0;
}

void ImGuiManager::onSeek(int frame) {
    m_currentFrame = frame;
    m_playing = false;
}