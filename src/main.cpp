#include "YoloDetector.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <string>

namespace fs = std::filesystem;
using namespace std;
using namespace cv;

void visualize(cv::Mat& frame, const vector<Detection>& detections) {
    int wheels_count = 0;
    int cars_count = 0;

    for (const auto& det : detections) {
        cv::Scalar color;
        string class_name = det.getClassName();

        if (det.class_id == 1) {
            color = cv::Scalar(0, 255, 0);
            wheels_count++;
        } else if (det.class_id == 2) {
            color = cv::Scalar(255, 0, 0);
            cars_count++;
        } else {
            color = cv::Scalar(0, 255, 255);
        }

        cv::rectangle(frame, det.bbox, color, 2);
        cv::circle(frame, det.center, 4, color, -1);

        string label = class_name + ": " + to_string((int)(det.confidence * 100)) + "%";
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        cv::rectangle(frame,
                     cv::Point(det.bbox.x, det.bbox.y - text_size.height - 5),
                     cv::Point(det.bbox.x + text_size.width, det.bbox.y),
                     color, -1);
        cv::putText(frame, label,
                   cv::Point(det.bbox.x, det.bbox.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    }

    string stats = format("Cars: %d | Wheels: %d | Total: %d",
                         cars_count, wheels_count, (int)detections.size());
    cv::putText(frame, stats, cv::Point(20, 40),
               cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);

    cout << "   " << cars_count << " болидов, " << wheels_count << " колёс" << endl;
}

static void printUsage(const char* prog) {
    cerr << "Использование:\n"
         << "  " << prog << " [images_dir] [model_path] [output_dir]\n\n"
         << "По умолчанию (относительно текущей папки):\n"
         << "  images_dir = data/images/test\n"
         << "  model_path = models/best.onnx\n"
         << "  output_dir = output/detections\n";
}

static bool isImageExtension(const fs::path& path) {
    string ext = path.extension().string();
    transform(ext.begin(), ext.end(), ext.begin(),
              [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png";
}

int main(int argc, char* argv[]) {
    try {
        cout << "YOLO Racing Detector (ONNX Runtime + NMS)" << endl;
        cout << "==========================================" << endl;

        if (argc > 1 && (string(argv[1]) == "-h" || string(argv[1]) == "--help")) {
            printUsage(argv[0]);
            return 0;
        }

        fs::path images_dir = "data/images/test";
        fs::path model_path = "models/best.onnx";
        fs::path output_dir = "output/detections";

        if (argc > 1) images_dir = argv[1];
        if (argc > 2) model_path = argv[2];
        if (argc > 3) output_dir = argv[3];
        if (argc > 4) {
            printUsage(argv[0]);
            return 1;
        }

        if (!fs::exists(images_dir)) {
            cerr << "Папка с изображениями не найдена: " << images_dir << endl;
            printUsage(argv[0]);
            return -1;
        }

        if (!fs::exists(model_path)) {
            cerr << "Модель не найдена: " << model_path << endl;
            return -1;
        }

        fs::create_directories(output_dir);

        cout << "Изображения: " << fs::absolute(images_dir) << endl;
        cout << "Модель:       " << fs::absolute(model_path) << endl;
        cout << "Результаты:   " << fs::absolute(output_dir) << endl;

        YoloDetector detector(model_path);

        vector<fs::path> image_files;
        for (const auto& entry : fs::directory_iterator(images_dir)) {
            if (entry.is_regular_file() && isImageExtension(entry.path())) {
                image_files.push_back(entry.path());
            }
        }

        if (image_files.empty()) {
            cerr << "Изображения не найдены в " << images_dir << endl;
            return -1;
        }

        sort(image_files.begin(), image_files.end());
        cout << "\nНайдено изображений: " << image_files.size() << endl;
        cout << "\nНачало обработки...\n" << endl;

        for (size_t i = 0; i < image_files.size(); i++) {
            const auto& img_path = image_files[i];
            cout << "[" << (i + 1) << "/" << image_files.size() << "] "
                 << img_path.filename().string() << endl;

            cv::Mat frame = cv::imread(img_path.string());
            if (frame.empty()) {
                cerr << "   Не удалось загрузить изображение" << endl;
                continue;
            }

            auto detections = detector.detect(frame);
            cout << "   Найдено детекций: " << detections.size() << endl;
            visualize(frame, detections);

            fs::path output_path = output_dir / (img_path.stem().string() + "_detected.jpg");
            cv::imwrite(output_path.string(), frame);
            cout << "   Сохранено: " << output_path << endl;
        }

        cout << "\nОбработка завершена!" << endl;

    } catch (const exception& e) {
        cerr << "\nОшибка: " << e.what() << endl;
        return -1;
    }

    return 0;
}
