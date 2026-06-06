#include "VideoPlayer.h"
#include "AlertSystem.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <string>

namespace fs = std::filesystem;
using namespace std;
using namespace cv;

// ======================== Структуры и функции из бэкенда ========================
struct Detection {
    int class_id;
    float confidence;
    cv::Rect bbox;
    cv::Point2f center;

    string getClassName() const {
        if (class_id == 0) return "track";
        if (class_id == 1) return "wheel";
        if (class_id == 2) return "car";
        return "unknown";
    }
};

struct LetterboxResult {
    cv::Mat padded_img;
    float scale;
    int pad_x;
    int pad_y;
    int orig_w;
    int orig_h;
};

LetterboxResult letterbox(const cv::Mat& img, int target_size) {
    LetterboxResult result;
    result.orig_w = img.cols;
    result.orig_h = img.rows;

    float scale = min((float)target_size / img.cols, (float)target_size / img.rows);
    result.scale = scale;

    int new_w = (int)(img.cols * scale);
    int new_h = (int)(img.rows * scale);

    cv::Mat resized;
    cv::resize(img, resized, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);

    int pad_x = (target_size - new_w) / 2;
    int pad_y = (target_size - new_h) / 2;
    result.pad_x = pad_x;
    result.pad_y = pad_y;

    cv::Mat padded(target_size, target_size, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(padded(cv::Rect(pad_x, pad_y, new_w, new_h)));

    result.padded_img = padded;
    return result;
}

class YoloDetector {
public:
    YoloDetector(const fs::path& model_path, float conf_threshold = 0.25) {
        cout << "🔧 Загрузка ONNX модели с NMS..." << endl;
        env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "yolo");
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
        wstring wpath = model_path.wstring();
        session = Ort::Session(env, wpath.c_str(), session_options);
#else
        string spath = model_path.string();
        session = Ort::Session(env, spath.c_str(), session_options);
#endif

        this->conf_thresh = conf_threshold;
        this->input_size = 832;
        cout << "✅ Модель загружена (input_size=" << input_size << ")" << endl;
    }

    vector<Detection> detect(const cv::Mat& frame) {
        vector<Detection> detections;

        LetterboxResult lb = letterbox(frame, input_size);

        cv::Mat rgb;
        cv::cvtColor(lb.padded_img, rgb, cv::COLOR_BGR2RGB);

        cv::Mat blob;
        cv::dnn::blobFromImage(rgb, blob, 1.0/255.0,
                               cv::Size(input_size, input_size),
                               cv::Scalar(), true, false, CV_32F);

        vector<int64_t> input_shape = {1, 3, input_size, input_size};
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, blob.ptr<float>(), blob.total(), input_shape.data(), input_shape.size());

        const char* input_names[] = {"images"};
        const char* output_names[] = {"output0"};

        auto outputs = session.Run(
            Ort::RunOptions{nullptr},
            input_names, &input_tensor, 1,
            output_names, 1);

        float* output_data = outputs[0].GetTensorMutableData<float>();
        auto output_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        int num_boxes = output_shape[1];
        int num_values = output_shape[2];

        for (int i = 0; i < num_boxes; i++) {
            int base_idx = i * num_values;
            float x1 = output_data[base_idx + 0];
            float y1 = output_data[base_idx + 1];
            float x2 = output_data[base_idx + 2];
            float y2 = output_data[base_idx + 3];
            float confidence = output_data[base_idx + 4];
            int class_id = (int)output_data[base_idx + 5];

            float class_conf_thresh;
            if (class_id == 0) class_conf_thresh = 0.30f;
            else if (class_id == 1) class_conf_thresh = 0.30f;
            else class_conf_thresh = 0.25f;

            if (confidence < class_conf_thresh) continue;

            float adj_x1 = (x1 - lb.pad_x) / lb.scale;
            float adj_y1 = (y1 - lb.pad_y) / lb.scale;
            float adj_x2 = (x2 - lb.pad_x) / lb.scale;
            float adj_y2 = (y2 - lb.pad_y) / lb.scale;

            adj_x1 = max(0.0f, adj_x1);
            adj_y1 = max(0.0f, adj_y1);
            adj_x2 = min((float)frame.cols, adj_x2);
            adj_y2 = min((float)frame.rows, adj_y2);

            int x = (int)adj_x1;
            int y = (int)adj_y1;
            int box_w = (int)(adj_x2 - adj_x1);
            int box_h = (int)(adj_y2 - adj_y1);

            if (box_w < 20 || box_h < 20) continue;

            Detection det;
            det.class_id = class_id;
            det.confidence = confidence;
            det.bbox = cv::Rect(x, y, box_w, box_h);
            det.center = cv::Point2f(x + box_w/2.0f, y + box_h/2.0f);
            detections.push_back(det);
        }

        cout << "   Найдено детекций: " << detections.size() << endl;
        return detections;
    }

private:
    Ort::Env env{nullptr};
    Ort::SessionOptions session_options;
    Ort::Session session{nullptr};
    float conf_thresh;
    int input_size;
};

// ======================== GUI-приложение ========================
int main(int argc, char* argv[]) {
    // ---------- Настройки ----------
    fs::path model_path = "models/best.onnx";
    string video_path = "test.mp4";
    if (argc > 1) video_path = argv[1];
    if (argc > 2) model_path = argv[2];

    // ---------- Инициализация GLFW + ImGui ----------
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow* window = glfwCreateWindow(1600, 900, "Traffic Analysis GUI", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ---------- Модули ----------
    VideoPlayer player;
    AlertSystem alertSystem;

    if (!player.LoadVideo(video_path)) {
        cerr << "ERROR: Cannot open video: " << video_path << endl;
    } else {
        player.Play();
    }

    // Детектор
    YoloDetector detector(model_path);

    // Переменные UI
    bool showBoxes = true;
    bool showAlertsOverlay = true;

    // Главный цикл
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---------- Простое размещение окон без докинга ----------
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowSize(ImVec2(200, 300));
        ImGui::Begin("Controls");
        if (ImGui::Button("Play")) player.Play();
        ImGui::SameLine();
        if (ImGui::Button("Pause")) player.Pause();
        ImGui::SameLine();
        if (ImGui::Button("Stop")) player.Stop();
        ImGui::Separator();
        ImGui::Checkbox("Show Boxes", &showBoxes);
        ImGui::End();

        // Обработка кадра детектором
        if (player.IsPlaying()) {
            player.Update();
            cv::Mat frame = player.GetCurrentFrame();
            if (!frame.empty()) {
                auto detections = detector.detect(frame);
                if (showBoxes) {
                    std::vector<cv::Rect> boxes;
                    std::vector<std::pair<cv::Point, std::string>> texts;
                    for (const auto& det : detections) {
                        boxes.push_back(det.bbox);
                        string label = det.getClassName() + " " + to_string((int)(det.confidence*100)) + "%";
                        texts.push_back({cv::Point(det.bbox.x, det.bbox.y - 5), label});
                    }
                    player.SetOverlayBoxes(boxes);
                    player.SetOverlayTexts(texts);
                } else {
                    player.ClearOverlays();
                }
            }
        }

        // Окно видео
        ImGui::SetNextWindowPos(ImVec2(220, 10));
        ImGui::SetNextWindowSize(ImVec2(1100, 700));
        ImGui::Begin("Video");
        player.Render();
        ImGui::End();

        // Окно алертов рисует сам AlertSystem (внутри вызывает Begin/End)
        alertSystem.drawAlertPanel();

        // Рендеринг
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Очистка
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}