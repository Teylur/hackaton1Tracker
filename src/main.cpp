#include "onnxruntime_cxx_api.h"
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

// Letterbox
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
        
        // 1. Letterbox
        LetterboxResult lb = letterbox(frame, input_size);
        
        // 2. Preprocessing (BGR → RGB, нормализация)
        cv::Mat rgb;
        cv::cvtColor(lb.padded_img, rgb, cv::COLOR_BGR2RGB);
        
        cv::Mat blob;
        cv::dnn::blobFromImage(rgb, blob, 1.0/255.0, 
                               cv::Size(input_size, input_size), 
                               cv::Scalar(), true, false, CV_32F);
        
        // 3. Инференс через ONNX Runtime (НЕ через OpenCV DNN!)
        vector<int64_t> input_shape = {1, 3, input_size, input_size};
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, blob.ptr<float>(), blob.total(), input_shape.data(), input_shape.size()
        );
        
        const char* input_names[] = {"images"};
        const char* output_names[] = {"output0"};
        
        auto outputs = session.Run(
            Ort::RunOptions{nullptr},
            input_names, &input_tensor, 1,
            output_names, 1
        );
        
        // 4. Парсинг вывода (формат с NMS: [1, N, 6])
        float* output_data = outputs[0].GetTensorMutableData<float>();
        auto output_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        
        int num_boxes = output_shape[1];
        int num_values = output_shape[2];  // 6 = [x1, y1, x2, y2, confidence, class_id]
        
        cout << "   Output shape: [1, " << num_boxes << ", " << num_values << "]" << endl;
        
        for (int i = 0; i < num_boxes; i++) {
            int base_idx = i * num_values;
            
            // Координаты bbox (уже в пикселях на padded изображении)
            float x1 = output_data[base_idx + 0];
            float y1 = output_data[base_idx + 1];
            float x2 = output_data[base_idx + 2];
            float y2 = output_data[base_idx + 3];
            float confidence = output_data[base_idx + 4];
            int class_id = (int)output_data[base_idx + 5];
            
            // Пропускаем track (class 0) и слабые детекции
            float class_conf_thresh;
            if (class_id == 0) {  // track
                class_conf_thresh = 0.30;  // Порог для трека
            } else if (class_id == 1) {  // wheel
                class_conf_thresh = 0.30;
            } else {  // car
                class_conf_thresh = 0.25;
            }

            if (confidence < class_conf_thresh) continue;
            
            // Масштабирование к оригинальному размеру (убираем padding)
            float adj_x1 = (x1 - lb.pad_x) / lb.scale;
            float adj_y1 = (y1 - lb.pad_y) / lb.scale;
            float adj_x2 = (x2 - lb.pad_x) / lb.scale;
            float adj_y2 = (y2 - lb.pad_y) / lb.scale;
            
            // Ограничение границ
            adj_x1 = max(0.0f, adj_x1);
            adj_y1 = max(0.0f, adj_y1);
            adj_x2 = min((float)frame.cols, adj_x2);
            adj_y2 = min((float)frame.rows, adj_y2);
            
            int x = (int)adj_x1;
            int y = (int)adj_y1;
            int box_w = (int)(adj_x2 - adj_x1);
            int box_h = (int)(adj_y2 - adj_y1);
            
            // Пропускаем слишком маленькие
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

void visualize(cv::Mat& frame, const vector<Detection>& detections, const string& image_name) {
    int wheels_count = 0;
    int cars_count = 0;
    int track_count = 0;
    
    for (const auto& det : detections) {
        cv::Scalar color;
        string class_name;
        
        if (det.class_id == 1) {
            color = cv::Scalar(0, 255, 0);  // ЗЕЛЁНЫЙ для wheel
            class_name = "wheel";
            wheels_count++;
        } else if (det.class_id == 2) {
            color = cv::Scalar(255, 0, 0);  // СИНИЙ для car
            class_name = "car";
            cars_count++;
        } else {
            color = cv::Scalar(0, 255, 255);  // ЖЁЛТЫЙ для track
            class_name = "track";
            track_count++;
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
    
    cout << "   🏎️ " << cars_count << " болидов, 🛞 " << wheels_count << " колёс" << endl;
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
        cout << "🏎️ YOLO Racing Detector (ONNX Runtime + NMS)" << endl;
        cout << "==============================================" << endl;

        if (argc > 1 && (string(argv[1]) == "-h" || string(argv[1]) == "--help")) {
            printUsage(argv[0]);
            return 0;
        }

        fs::path images_dir = "data/images/test";
        fs::path model_path = "models/best.onnx";
        fs::path output_dir = "output/detections";
        float conf_threshold = 0.25;

        if (argc > 1) images_dir = argv[1];
        if (argc > 2) model_path = argv[2];
        if (argc > 3) output_dir = argv[3];
        if (argc > 4) {
            printUsage(argv[0]);
            return 1;
        }

        if (!fs::exists(images_dir)) {
            cerr << "❌ Папка с изображениями не найдена: " << images_dir << endl;
            printUsage(argv[0]);
            return -1;
        }

        if (!fs::exists(model_path)) {
            cerr << "❌ Модель не найдена: " << model_path << endl;
            return -1;
        }

        fs::create_directories(output_dir);

        cout << "📂 Изображения: " << fs::absolute(images_dir) << endl;
        cout << "🧠 Модель:       " << fs::absolute(model_path) << endl;
        cout << "💾 Результаты:   " << fs::absolute(output_dir) << endl;

        YoloDetector detector(model_path, conf_threshold);

        vector<fs::path> image_files;
        for (const auto& entry : fs::directory_iterator(images_dir)) {
            if (entry.is_regular_file() && isImageExtension(entry.path())) {
                image_files.push_back(entry.path());
            }
        }

        if (image_files.empty()) {
            cerr << "❌ Изображения не найдены в " << images_dir << endl;
            return -1;
        }

        sort(image_files.begin(), image_files.end());

        cout << "\n📁 Найдено изображений: " << image_files.size() << endl;
        cout << "🎯 Порог уверенности: " << conf_threshold << endl;
        cout << "\n🚀 Начало обработки...\n" << endl;

        for (size_t i = 0; i < image_files.size(); i++) {
            const auto& img_path = image_files[i];
            cout << "[" << (i+1) << "/" << image_files.size() << "] "
                 << img_path.filename().string() << endl;

            cv::Mat frame = cv::imread(img_path.string());
            if (frame.empty()) {
                cerr << "   ⚠️  Не удалось загрузить изображение" << endl;
                continue;
            }

            auto detections = detector.detect(frame);
            visualize(frame, detections, img_path.filename().string());

            fs::path output_path = output_dir / (img_path.stem().string() + "_detected.jpg");
            cv::imwrite(output_path.string(), frame);
            cout << "   ✅ Сохранено: " << output_path << endl;
        }

        cout << "\n✅ Обработка завершена!" << endl;

    } catch (const exception& e) {
        cerr << "\n❌ Ошибка: " << e.what() << endl;
        return -1;
    }

    return 0;
}