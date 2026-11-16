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
    std::string config_path = "configs/two_camera.yaml";
    io::Camera camera_left(config_path, "camera_left");
    io::Camera camera_right(config_path, "camera_right");

    cv::Mat img_left, img_right;
    std::chrono::steady_clock::time_point timestamp;
    int frame_count_left = 0;
    int frame_count_right = 0;
    cv::namedWindow("image_left", cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    cv::namedWindow("image_right", cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    while (true) {
        camera_left.read(img_left, timestamp);
        camera_right.read(img_right, timestamp);
        auto yolo_start_left = std::chrono::steady_clock::now();
        auto yolo_start_right = std::chrono::steady_clock::now();
        cv::imshow("image_left", img_left);
        cv::imshow("image_right", img_right);
        if (!img_left.empty()) {
            int cx_left = img_left.cols / 2;
            int cy_left = img_left.rows / 2;
            cv::Rect roi_left(cx_left - 2, cy_left - 2, 5, 5);

            cv::Mat center_roi_left = img_left(roi_left);

            // std::cout << "Center 5x5 Matrix:\n" 
            //           << cv::format(center_roi, cv::Formatter::FMT_NUMPY)
            //           << std::endl;
        }

        if (!img_right.empty()) {
            int cx_right = img_right.cols / 2;
            int cy_right = img_right.rows / 2;
            cv::Rect roi_right(cx_right - 2, cy_right - 2, 5, 5);

            cv::Mat center_roi_right = img_right(roi_right);

            // std::cout << "Center 5x5 Matrix:\n" 
            //           << cv::format(center_roi, cv::Formatter::FMT_NUMPY)
            //           << std::endl;
        }
        if (cv::waitKey(1) == 27) break;
    }
}

