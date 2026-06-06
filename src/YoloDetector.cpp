#include "YoloDetector.h"
#include <iostream>
#include <algorithm>

std::string Detection::getClassName() const {
    if (class_id == 0) return "track";
    if (class_id == 1) return "wheel";
    if (class_id == 2) return "car";
    return "unknown";
}

YoloDetector::LetterboxResult YoloDetector::letterbox(const cv::Mat& img, int target_size) {
    LetterboxResult result;
    result.orig_w = img.cols;
    result.orig_h = img.rows;

    float scale = std::min(static_cast<float>(target_size) / img.cols,
                           static_cast<float>(target_size) / img.rows);
    result.scale = scale;

    int new_w = static_cast<int>(img.cols * scale);
    int new_h = static_cast<int>(img.rows * scale);

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

YoloDetector::YoloDetector(const std::filesystem::path& model_path, float conf_threshold)
    : conf_thresh(conf_threshold), input_size(832) {
    std::cout << "Загрузка ONNX модели..." << std::endl;

    env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "yolo");
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
    std::wstring wpath = model_path.wstring();
    session = Ort::Session(env, wpath.c_str(), session_options);
#else
    std::string spath = model_path.string();
    session = Ort::Session(env, spath.c_str(), session_options);
#endif

    std::cout << "Модель загружена (input_size=" << input_size << ")" << std::endl;
}

std::vector<Detection> YoloDetector::detect(const cv::Mat& frame) {
    std::vector<Detection> detections;

    LetterboxResult lb = letterbox(frame, input_size);

    cv::Mat rgb;
    cv::cvtColor(lb.padded_img, rgb, cv::COLOR_BGR2RGB);

    cv::Mat blob;
    cv::dnn::blobFromImage(rgb, blob, 1.0 / 255.0,
                           cv::Size(input_size, input_size),
                           cv::Scalar(), true, false, CV_32F);

    std::vector<int64_t> input_shape = {1, 3, input_size, input_size};
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
    int num_boxes = static_cast<int>(output_shape[1]);
    int num_values = static_cast<int>(output_shape[2]);

    for (int i = 0; i < num_boxes; i++) {
        int base_idx = i * num_values;
        float x1 = output_data[base_idx + 0];
        float y1 = output_data[base_idx + 1];
        float x2 = output_data[base_idx + 2];
        float y2 = output_data[base_idx + 3];
        float confidence = output_data[base_idx + 4];
        int class_id = static_cast<int>(output_data[base_idx + 5]);

        float class_conf_thresh;
        if (class_id == 0) class_conf_thresh = 0.30f;
        else if (class_id == 1) class_conf_thresh = 0.30f;
        else class_conf_thresh = 0.25f;

        if (confidence < class_conf_thresh) continue;

        float adj_x1 = (x1 - lb.pad_x) / lb.scale;
        float adj_y1 = (y1 - lb.pad_y) / lb.scale;
        float adj_x2 = (x2 - lb.pad_x) / lb.scale;
        float adj_y2 = (y2 - lb.pad_y) / lb.scale;

        adj_x1 = std::max(0.0f, adj_x1);
        adj_y1 = std::max(0.0f, adj_y1);
        adj_x2 = std::min(static_cast<float>(frame.cols), adj_x2);
        adj_y2 = std::min(static_cast<float>(frame.rows), adj_y2);

        int x = static_cast<int>(adj_x1);
        int y = static_cast<int>(adj_y1);
        int box_w = static_cast<int>(adj_x2 - adj_x1);
        int box_h = static_cast<int>(adj_y2 - adj_y1);

        if (box_w < 20 || box_h < 20) continue;

        Detection det;
        det.class_id = class_id;
        det.confidence = confidence;
        det.bbox = cv::Rect(x, y, box_w, box_h);
        det.center = cv::Point2f(x + box_w / 2.0f, y + box_h / 2.0f);
        detections.push_back(det);
    }

    return detections;
}
