cmake_minimum_required(VERSION 3.16)
add_library(auto_buff OBJECT 
    buff_type.cpp
    buff_detector.cpp
    buff_solver.cpp
    buff_aimer.cpp
    buff_target.cpp
    yolo11_buff.cpp
)

target_link_libraries(auto_buff PUBLIC auto_aim openvino::runtime Ceres::ceres Eigen3::Eigen)
