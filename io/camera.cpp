#include "camera.hpp"

#include <stdexcept>

#include "hikrobot/hikrobot.hpp"
#include "mindvision/mindvision.hpp"
#include "tools/yaml.hpp"

namespace io
{
static std::unique_ptr<CameraBase> create_camera_from_node(const YAML::Node & node)
{
  auto camera_name = tools::read<std::string>(node, "camera_name");
  auto exposure_ms = tools::read<double>(node, "exposure_ms");

  if (camera_name == "mindvision") {
    auto gamma = tools::read<double>(node, "gamma");
    auto vid_pid = tools::read<std::string>(node, "vid_pid");
    return std::make_unique<MindVision>(exposure_ms, gamma, vid_pid);
  }

  else if (camera_name == "hikrobot") {
    auto gain = tools::read<double>(node, "gain");
    auto vid_pid = tools::read<std::string>(node, "vid_pid");
    // 读取序列号；如果单相机配置没写 serial_number，默认给空字符串
    std::string serial_number = "";
    if (node["serial_number"]) {
      serial_number = tools::read<std::string>(node, "serial_number");
    }
    return std::make_unique<HikRobot>(exposure_ms, gain, vid_pid, serial_number);
  }

  else {
    throw std::runtime_error("Unknown camera_name: " + camera_name + "!");
  }
}

Camera::Camera(const std::string & config_path)
{
  auto yaml = tools::load(config_path);
  camera_ = create_camera_from_node(yaml);
}

Camera::Camera(const std::string & config_path, const std::string & camera_node)
{
  auto yaml = tools::load(config_path);
  if (!yaml[camera_node]) {
    throw std::runtime_error("Camera node '" + camera_node + "' not found in " + config_path);
  }
  camera_ = create_camera_from_node(yaml[camera_node]);
}

void Camera::read(cv::Mat & img, std::chrono::steady_clock::time_point & timestamp)
{
  camera_->read(img, timestamp);
}

}  // namespace io
