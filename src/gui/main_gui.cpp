#include "ImGuiManager.h"

#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    std::string video_path = "data/videos/test.mp4";
    std::filesystem::path model_path = "/home/user/hackaton1Tracker/models/best.onnx";

    if (argc > 1) video_path = argv[1];
    if (argc > 2) model_path = argv[2];

    if (!std::filesystem::exists(model_path)) {
        // Добавлен префикс u8 для гарантированной UTF-8 кодировки в консоли
        std::cerr << u8"Модель не найдена: " << model_path << std::endl;
        return 1;
    }

    try {
        ImGuiManager ui(video_path, model_path);
        ui.run();
    } catch (const std::exception& e) {
        // Добавлен префикс u8
        std::cerr << u8"Ошибка: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}