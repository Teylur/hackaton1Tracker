#include "ImGuiManager.h"

#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    std::string video_path = "data/videos/test.mp4";
    std::filesystem::path model_path = "models/best.onnx";

    if (argc > 1) video_path = argv[1];
    if (argc > 2) model_path = argv[2];

    if (!std::filesystem::exists(model_path)) {
        std::cerr << "Модель не найдена: " << model_path << std::endl;
        return 1;
    }

    try {
        ImGuiManager ui(video_path, model_path);
        ui.run();
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
