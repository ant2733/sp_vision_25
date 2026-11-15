#include <chrono>
#include <iostream>
#include "../io/hikrobot/hikrobot.hpp"
#include "io/camera.hpp"
#include "tasks/auto_aim/aimer.hpp"
#include "tasks/auto_aim/yolo.hpp"
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <string>


int main() {
    std::string config_path = "configs/camera.yaml";
    std::string yolo_path = "configs/demo.yaml";
    io::Camera camera(config_path);

    cv::Mat img;
    std::chrono::steady_clock::time_point timestamp;
    auto_aim::YOLO yolo(yolo_path);
    int count = 0;
    int frame_count = 0;
    cv::namedWindow("image", cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    while (true) {
        if (count == 500) break;
        camera.read(img, timestamp);
        auto yolo_start = std::chrono::steady_clock::now();
        auto armors = yolo.detect(img, frame_count);
        cv::imshow("image", img);
        if (!img.empty()) {
            int cx = img.cols / 2;
            int cy = img.rows / 2;
            cv::Rect roi(cx - 2, cy - 2, 5, 5);

            cv::Mat center_roi = img(roi);

            std::cout << "Center 5x5 Matrix:\n" 
                      << cv::format(center_roi, cv::Formatter::FMT_NUMPY)
                      << std::endl;
        }
        if (cv::waitKey(1) == 27) break;
    }
}

