#pragma once

#include "onnxruntime_cxx_api.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <vector>
#include <string>

struct Detection {
    int class_id;
    float confidence;
    cv::Rect bbox;
    cv::Point2f center;

    std::string getClassName() const;
};

class YoloDetector {
public:
    explicit YoloDetector(const std::filesystem::path& model_path, float conf_threshold = 0.25f);

    std::vector<Detection> detect(const cv::Mat& frame);

private:
    struct LetterboxResult {
        cv::Mat padded_img;
        float scale;
        int pad_x;
        int pad_y;
        int orig_w;
        int orig_h;
    };

    static LetterboxResult letterbox(const cv::Mat& img, int target_size);

    Ort::Env env{nullptr};
    Ort::SessionOptions session_options;
    Ort::Session session{nullptr};
    float conf_thresh;
    int input_size;
};
