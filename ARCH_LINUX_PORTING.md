# Arch Linux 适配修改说明

## 1. 修改目的

本次修改用于让项目在 Arch Linux 的滚动更新环境中正常配置、编译和运行，同时尽量保留对原 Ubuntu 22.04 环境的兼容。

验证环境如下：

- Arch Linux x86_64
- GCC 16.2.1
- CMake 4.4.3
- OpenCV 5.0.0
- OpenVINO 2026.3.1
- Eigen 5.0.1
- fmt 12.2.0
- yaml-cpp 0.9.0
- Ceres Solver 2.2.0

## 2. Arch Linux 依赖安装

使用以下命令安装项目依赖：

```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    can-utils \
    ceres-solver \
    eigen \
    fmt \
    libusb \
    nlohmann-json \
    opencv \
    openvino \
    openssh \
    screen \
    spdlog \
    yaml-cpp
```

项目已经包含 x86_64 和 aarch64 版本的海康、迈德威视相机运行库。编译程序不再依赖厂商 SDK 安装器写入的 Ubuntu `/opt` 路径。

如果需要使用相机厂商提供的配置工具、驱动或 udev 规则，仍需单独安装对应厂商 SDK。

## 3. 编译与运行

在项目根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

运行离线自瞄示例：

```bash
./build/auto_aim_test
```

如果机器没有图形界面，可以使用 Qt 的离屏模式进行有限帧数的测试：

```bash
QT_QPA_PLATFORM=offscreen ./build/auto_aim_test \
    --config-path=configs/demo.yaml \
    --end-index=1 \
    assets/demo/demo
```

## 4. 具体修改内容

### 4.1 根 CMake 配置

修改文件：`CMakeLists.txt`

- 将项目明确设置为 C++ 项目，并要求使用 C++17。
- 仅在用户未指定构建类型时默认使用 `Release`，不再覆盖用户传入的构建类型。
- 删除强制设置 OpenVINO 路径的代码。
- 优先通过标准 CMake 搜索路径查找 OpenVINO，支持 Arch 的 `/usr/lib/cmake/openvino`。
- 保留 `/opt/intel/openvino_2024.6.0/runtime/cmake` 作为旧 Ubuntu 安装方式的回退路径。
- 根据 OpenCV 主版本选择几何模块：
  - OpenCV 4 使用 `calib3d`。
  - OpenCV 5 使用 `calib` 和 `geometry`。
- 只链接项目实际使用的 OpenCV 模块，避免因为 `opencv_viz` 间接依赖 VTK 而出现链接错误。
- 使用 `Eigen3::Eigen`、`Ceres::ceres` 和 `yaml-cpp::yaml-cpp` 等现代 CMake 目标。
- 为旧版本 yaml-cpp 增加目标名称兼容处理。

### 4.2 OpenVINO 配置

修改文件：

- `tasks/auto_aim/CMakeLists.txt`
- `tasks/auto_buff/CMakeLists.txt`
- `tasks/omniperception/CMakeLists.txt`

删除各子目录中重复的以下硬编码：

```cmake
set(OpenVINO_DIR "/opt/intel/openvino_2024.6.0/runtime/cmake")
```

OpenVINO 现在统一由根 `CMakeLists.txt` 查找，各任务目标直接链接 `openvino::runtime`。

### 4.3 Eigen 配置和 Eigen 5 兼容

修改文件：

- `tools/CMakeLists.txt`
- `tasks/auto_aim/CMakeLists.txt`
- `tasks/auto_aim/planner/tinympc/CMakeLists.txt`
- `tasks/auto_buff/CMakeLists.txt`
- `tasks/omniperception/CMakeLists.txt`
- `tools/ransac_sine_fitter.cpp`

原项目依赖 `EIGEN3_INCLUDE_DIR` 变量，但 Arch 当前 Eigen CMake 配置主要提供 `Eigen3::Eigen` 目标，因此编译时会出现：

```text
fatal error: Eigen/Dense: No such file or directory
```

现在各目标通过 `Eigen3::Eigen` 获取正确的头文件目录。

同时将 Eigen 5 已弃用的：

```cpp
X.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV)
```

修改为：

```cpp
X.bdcSvd<Eigen::ComputeThinU | Eigen::ComputeThinV>()
```

### 4.4 工业相机 SDK 和 libusb

修改文件：`io/CMakeLists.txt`

- 使用 pkg-config 查找 `libusb-1.0`，不再直接写死库名称。
- 根据 `CMAKE_SYSTEM_PROCESSOR` 自动选择相机运行库：
  - `x86_64`、`amd64`、`AMD64` 对应 `amd64`。
  - `aarch64`、`arm64`、`ARM64` 对应 `arm64`。
- 将项目自带的海康和迈德威视动态库声明为 CMake imported target。
- 构建产物会记录相机库路径，不依赖 `/opt/MVS` 或其他 Ubuntu SDK 安装路径。

使用的项目内动态库为：

```text
io/hikrobot/lib/amd64/libMvCameraControl.so
io/hikrobot/lib/arm64/libMvCameraControl.so
io/mindvision/lib/amd64/libMVSDK.so
io/mindvision/lib/arm64/libMVSDK.so
```

### 4.5 OpenCV 5 API 兼容

新增文件：`tools/opencv_compat.hpp`

OpenCV 5 将部分二维和三维几何 API 移动到了新的头文件：

```text
opencv2/geometry/2d.hpp
opencv2/geometry/3d.hpp
```

受影响的 API 包括：

- `cv::minAreaRect`
- `cv::moments`
- `cv::solvePnP`
- `cv::Rodrigues`
- `cv::projectPoints`

兼容头根据 `CV_VERSION_MAJOR` 自动选择 OpenCV 4 或 OpenCV 5 对应的头文件。

应用兼容头的文件包括：

- `tasks/auto_aim/detector.hpp`
- `tasks/auto_aim/solver.hpp`
- `tasks/auto_buff/buff_detector.cpp`
- `tasks/auto_buff/buff_solver.cpp`
- `calibration/calibrate_camera.cpp`
- `calibration/calibrate_handeye.cpp`
- `calibration/calibrate_robotworld_handeye.cpp`

### 4.6 fmt 12 兼容

Arch 当前 fmt 版本不再保证仅包含 `fmt/core.h` 就可以使用完整的 `fmt::format` 功能。因此，使用 `fmt::format` 的源码改为包含：

```cpp
#include <fmt/format.h>
```

涉及目录：

- `calibration/`
- `src/`
- `tests/`

另外，fmt 12 不再隐式格式化 `ArmorName` 枚举。保存检测图片时改为使用项目已有的 `ARMOR_NAMES` 字符串表，涉及：

- `tasks/auto_aim/detector.cpp`
- `tasks/auto_aim/yolos/yolov5.cpp`
- `tasks/auto_aim/yolos/yolov8.cpp`
- `tasks/auto_aim/yolos/yolo11.cpp`

### 4.7 GCC 16 头文件检查

修改文件：

- `tools/recorder.hpp`
- `io/dm_imu/dm_imu.hpp`
- `src/standard_mpc.cpp`
- `src/mt_standard.cpp`

这些文件使用了 `std::atomic`，但没有直接包含 `<atomic>`。较新的 GCC 不再允许依赖其他标准库头文件间接包含它，因此补充了：

```cpp
#include <atomic>
```

### 4.8 日志目录初始化

修改文件：`tools/logger.cpp`

全新克隆的项目中不存在 `logs` 目录，而 spdlog 的文件 sink 无法自动创建父目录。程序现在会在初始化日志前执行：

```cpp
std::filesystem::create_directories("logs");
```

避免首次运行时因为日志文件无法创建而退出。

### 4.9 README

修改文件：`readme.md`

- 增加 Arch Linux 的 pacman 依赖安装命令。
- 说明项目内相机运行库的使用方式。
- 将构建命令更新为跨生成器的 `cmake --build` 形式。

## 5. 验证结果

执行了全新目录配置和完整编译：

```bash
cmake -S . -B /tmp/sp_vision_arch_verify -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/sp_vision_arch_verify -j4
```

结果：所有目标编译完成，进度达到 100%。

随后在清除 `LD_LIBRARY_PATH` 的情况下检查动态库和运行示例：

```bash
env -u LD_LIBRARY_PATH \
    QT_QPA_PLATFORM=offscreen \
    /tmp/sp_vision_arch_verify/auto_aim_test \
    --config-path=configs/demo.yaml \
    --end-index=1 \
    assets/demo/demo
```

程序成功加载 OpenCV、OpenVINO、海康 SDK、迈德威视 SDK 和模型文件，完成两帧推理后正常退出。

## 6. 尚未验证的部分

以下功能需要连接实际设备，当前仅确认能够编译和加载对应动态库：

- 海康工业相机采集
- 迈德威视工业相机采集
- USB 摄像头设备
- 串口通信
- USB2CAN/CAN 通信
- 达妙 IMU
- ROS 2 相关目标

ROS 2 仍为可选依赖。未检测到 ROS 2 环境时，CMake 会跳过哨兵和 ROS 2 测试目标，不影响其他程序的编译。

