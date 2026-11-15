# 视觉组—电控组通信对接说明

本文用于 `sp_vision_25` 视觉上位机与 RoboMaster C 板（STM32F407）下位机之间的通信对接。

> **建议方案：** 若运行 `src/standard_mpc.cpp` 及轨迹规划器，使用 **MicroUSB 虚拟串口（USB CDC）**，对应 `io/gimbal/`。旧版 USB2CAN 对应 `io/cboard.*`，不包含完整的速度、加速度前馈，不建议用于新版轨迹规划器。

---

## 1. 相关代码位置

| 文件 | 用途 |
|---|---|
| `io/gimbal/gimbal.hpp` | 新版串口收发结构体与接口定义 |
| `io/gimbal/gimbal.cpp` | 串口打开、接收线程、帧头同步、CRC 校验和发送实现 |
| `io/serial/` | 跨平台串口库 |
| `tools/crc.hpp`、`tools/crc.cpp` | CRC8/CRC16 算法 |
| `src/standard_mpc.cpp` | 新版通信在步兵自瞄/打符主程序中的实际调用 |
| `tests/gimbal_test.cpp` | 串口、云台状态、四元数及开火响应测试 |
| `configs/standard3.yaml`、`configs/standard4.yaml` | 串口设备名配置 |
| `io/cboard.hpp`、`io/cboard.cpp`、`io/socketcan.hpp` | 旧版 USB2CAN 通信实现 |

当前配置使用固定设备名：

```yaml
com_port: "/dev/gimbal"
```

`/dev/gimbal` 通常是通过 udev 规则为 `/dev/ttyACM*` 创建的固定软链接。

---

## 2. 推荐的数据流

```text
C板/电控                         小电脑/视觉
   │                                │
   │  模式、四元数、云台状态、弹速  │
   │ ─────────────────────────────> │
   │                                │  图像识别、目标估计、轨迹规划
   │  控制状态、目标角度、速度、加速度、开火
   │ <───────────────────────────── │
   │                                │
   │  云台闭环控制、发射机构执行     │
```

职责建议：

- **视觉组**：目标识别、位姿解算、状态估计、弹道解算、瞄准轨迹规划和开火决策。
- **电控组**：IMU 读取、云台底层闭环、发射机构控制、模式切换、通信收发和失联保护。
- **双方共同确认**：协议、坐标系、单位、时间戳/延迟、频率、CRC 和安全策略。

---

## 3. 电控发给视觉：`GimbalToVision`

协议定义位于 `io/gimbal/gimbal.hpp`：

```cpp
struct __attribute__((packed)) GimbalToVision
{
  uint8_t head[2] = {'S', 'P'};
  uint8_t mode;
  float q[4];
  float yaw;
  float yaw_vel;
  float pitch;
  float pitch_vel;
  float bullet_speed;
  uint16_t bullet_count;
  uint16_t crc16;
};
```

在 8 位字节、32 位 IEEE-754 `float`、无填充的前提下，固定帧长为 **43 字节**。

### 3.1 字段说明

| 偏移（字节） | 长度 | 字段 | 类型 | 约定/用途 |
|---:|---:|---|---|---|
| 0 | 2 | `head` | `uint8_t[2]` | 固定为 ASCII `S`、`P`，即 `0x53 0x50` |
| 2 | 1 | `mode` | `uint8_t` | `0` 空闲；`1` 自瞄；`2` 小符；`3` 大符 |
| 3 | 16 | `q` | `float[4]` | IMU/云台姿态四元数，顺序固定为 `w, x, y, z` |
| 19 | 4 | `yaw` | `float` | 当前 yaw 角度 |
| 23 | 4 | `yaw_vel` | `float` | 当前 yaw 角速度 |
| 27 | 4 | `pitch` | `float` | 当前 pitch 角度 |
| 31 | 4 | `pitch_vel` | `float` | 当前 pitch 角速度 |
| 35 | 4 | `bullet_speed` | `float` | 当前弹速，用于弹道解算 |
| 39 | 2 | `bullet_count` | `uint16_t` | 累计实际发弹数，用于确认是否已经出弹 |
| 41 | 2 | `crc16` | `uint16_t` | 前 41 字节的 CRC16，小端存放 |

### 3.2 视觉端如何使用这些数据

- `mode` 决定进入空闲、自瞄、小符或大符流程。
- `q` 被放入队列，视觉按照图像时间戳做四元数球面插值，以获得曝光时刻姿态。
- `yaw/pitch` 及角速度用于规划器和调试。
- `bullet_speed` 用于计算弹丸飞行时间和弹道补偿。
- `bullet_count` 用于判断发射机构是否真正完成发弹。

---

## 4. 视觉发给电控：`VisionToGimbal`

协议定义位于 `io/gimbal/gimbal.hpp`：

```cpp
struct __attribute__((packed)) VisionToGimbal
{
  uint8_t head[2] = {'S', 'P'};
  uint8_t mode;
  float yaw;
  float yaw_vel;
  float yaw_acc;
  float pitch;
  float pitch_vel;
  float pitch_acc;
  uint16_t crc16;
};
```

在 8 位字节、32 位 IEEE-754 `float`、无填充的前提下，固定帧长为 **29 字节**。

### 4.1 字段说明

| 偏移（字节） | 长度 | 字段 | 类型 | 约定/用途 |
|---:|---:|---|---|---|
| 0 | 2 | `head` | `uint8_t[2]` | 固定为 ASCII `S`、`P` |
| 2 | 1 | `mode` | `uint8_t` | `0` 不控制且不开火；`1` 控制云台但不开火；`2` 控制云台并允许开火 |
| 3 | 4 | `yaw` | `float` | yaw 目标角度 |
| 7 | 4 | `yaw_vel` | `float` | yaw 目标角速度前馈 |
| 11 | 4 | `yaw_acc` | `float` | yaw 目标角加速度前馈 |
| 15 | 4 | `pitch` | `float` | pitch 目标角度 |
| 19 | 4 | `pitch_vel` | `float` | pitch 目标角速度前馈 |
| 23 | 4 | `pitch_acc` | `float` | pitch 目标角加速度前馈 |
| 27 | 2 | `crc16` | `uint16_t` | 前 27 字节的 CRC16，小端存放 |

“前馈”是指视觉将期望速度和加速度提前发送给云台控制器，电控不必只依靠角度误差进行追踪，可减小相位滞后并提高高速小陀螺场景的跟随能力。

---

## 5. 二进制格式约定

双方必须统一以下底层约定：

1. 一个字节为 8 bit。
2. `float` 必须为 32 位 IEEE-754 单精度浮点数。
3. 整数和浮点数均按 **小端序**传输（STM32F407 和 x86 小电脑通常都是小端）。
4. 结构体必须采用 **1 字节对齐/禁止填充**。
5. 不能直接使用默认结构体布局而不检查长度。

STM32 端可以使用：

```c
#pragma pack(push, 1)
typedef struct {
    uint8_t head[2];
    uint8_t mode;
    float q[4];
    float yaw;
    float yaw_vel;
    float pitch;
    float pitch_vel;
    float bullet_speed;
    uint16_t bullet_count;
    uint16_t crc16;
} GimbalToVision_t;

// VisionToGimbal_t 同理
#pragma pack(pop)

_Static_assert(sizeof(GimbalToVision_t) == 43, "GimbalToVision size mismatch");
_Static_assert(sizeof(VisionToGimbal_t) == 29, "VisionToGimbal size mismatch");
```

> 如果编译器不支持 `_Static_assert`，也要在初始化时检查 `sizeof`，防止结构体填充造成协议错位。

---

## 6. CRC16 约定

CRC 实现位于 `tools/crc.cpp`，关键约定如下：

- 初始值：`0xFFFF`
- 按项目中的 `CRC16_TABLE` 逐字节计算
- 计算范围不包含最后的 CRC16 字段
- CRC 字段按小端序放在帧尾：低字节在前，高字节在后

发送时：

```cpp
crc16 = get_crc16(frame_bytes, frame_size - 2);
```

接收时：

```cpp
received_crc = data[len - 2] | (data[len - 1] << 8);
valid = get_crc16(data, len - 2) == received_crc;
```

电控端建议直接移植 `tools/crc.cpp` 中的算法和查找表，不要只根据“CRC16”名称选择其他库。CRC16 存在多种参数组合，名称相同不代表计算结果相同。

---

## 7. 坐标系和单位（联调前必须确认）

以下项目必须由视觉组和电控组共同填写并冻结：

| 项目 | 建议约定 | 最终确认 |
|---|---|---|
| 角度单位 | 弧度 `rad` | `[ ]` |
| 角速度单位 | `rad/s` | `[ ]` |
| 角加速度单位 | `rad/s²` | `[ ]` |
| 弹速单位 | `m/s` | `[ ]` |
| 四元数顺序 | `w, x, y, z` | `[ ]` |
| yaw 正方向 | 需结合机器人坐标系实测确认 | `[ ]` |
| pitch 正方向 | 需结合机器人坐标系实测确认 | `[ ]` |
| yaw/pitch 含义 | 建议为同一世界坐标系下的绝对目标角 | `[ ]` |
| 四元数含义 | 明确是云台到世界还是世界到云台 | `[ ]` |
| 云台零点 | 编码器零点及机械零位定义 | `[ ]` |

建议先做小角度测试：视觉依次发送 `yaw = +0.1 rad`、`yaw = -0.1 rad`、`pitch = +0.1 rad`、`pitch = -0.1 rad`，双方观察实际方向，确认符号后再进行自动控制。

---

## 8. 发送频率、串口和缓存

### 8.1 建议频率

- 视觉到电控：当前 `standard_mpc.cpp` 规划线程约每 `10 ms` 发送一次，即约 **100 Hz**。
- 电控到视觉：建议至少 **100 Hz**，并保持周期稳定。
- 四元数发送频率不宜过低，否则图像时间戳对应姿态的插值误差会变大。

### 8.2 串口参数

当前视觉代码只读取 `com_port`，没有显式调用 `setBaudrate()`，底层串口库的默认值为 `9600`。

- 如果使用 STM32 USB CDC，主机设置的“波特率”通常只是一项线路编码参数，不一定限制实际 USB 吞吐量，但电控端必须确认实现行为。
- 如果改用真实 UART/USB 转串口，双方必须显式统一波特率、数据位、停止位和校验位；9600 不足以支撑当前高频双向数据。
- 建议后续在 YAML 中增加明确的 `baudrate` 配置，并由视觉代码显式设置，避免协议依赖默认值。

### 8.3 接收方式

视觉端当前采用：

1. 连续寻找两个帧头字节 `S`、`P`；
2. 找到帧头后按固定帧长读取剩余内容；
3. 对整帧执行 CRC16 校验；
4. CRC 正确才更新状态。

电控端接收视觉指令时也应实现相同的帧头重同步和 CRC 校验，不能假设一次 USB 接收回调必然得到一整帧。

---

## 9. 安全与失联保护

电控端必须实现独立于视觉的安全策略：

1. 超过约定时间（建议从 `100 ms` 起调）未收到合法视觉帧：
   - 取消视觉控制；
   - 立即禁止开火；
   - 云台切回人工控制或安全状态。
2. CRC 错误、模式非法、浮点数为 `NaN/Inf` 时丢弃整帧。
3. 对目标角度、角速度和角加速度进行限幅。
4. `mode = 2` 只能表示视觉允许开火，电控仍应检查裁判系统、热量、弹速、枪口状态及自身安全条件。
5. 上电、视觉重启或串口重连后，默认状态必须是“不控制、不开火”。
6. 不允许沿用最后一帧的 `fire=true` 指令。

建议后续协议增加序号或视觉侧时间戳，以便检测重复帧、丢帧和通信延迟。当前协议没有长度、版本和序号字段，修改结构体时必须同步修改上下位机。

---

## 10. 建议联调顺序

### 阶段 1：只验证电控 → 视觉

运行：

```bash
./build/gimbal_test configs/standard3.yaml
```

先不要加 `-f`，检查：

- `[Gimbal] First q received.` 是否出现；
- 模式切换是否正确；
- 四元数是否归一、姿态方向是否正确；
- yaw/pitch 及角速度是否合理；
- 弹速是否合理；
- `bullet_count` 是否只在实际发弹时递增。

### 阶段 2：验证视觉 → 电控，但不开火

- 强制 `mode = 1`；
- 只发送很小的目标角度；
- 先关闭速度和加速度前馈；
- 检查正负方向、单位、零点和限幅；
- 检查断开 USB 后电控是否自动退出视觉控制。

### 阶段 3：加入速度和加速度前馈

- 记录电控收到的 `yaw/yaw_vel/yaw_acc`；
- 使用 PlotJuggler 对比目标轨迹和实际轨迹；
- 确认前馈符号、单位和控制周期一致。

### 阶段 4：开火链路

只有在云台控制、失联保护和限幅均验证完成后，才运行开火测试：

```bash
./build/gimbal_test -f configs/standard3.yaml
```

开火测试前必须卸弹或采取可靠的物理安全措施；首先验证 `mode=2` 的识别、超时自动停火和 `bullet_count` 回传。

### 阶段 5：完整自瞄

最后运行 `standard_mpc`，依次验证：

1. 静止目标；
2. 慢速平移目标；
3. 低速小陀螺；
4. 高速小陀螺；
5. 自身运动和通信抖动场景。

---

## 11. 双方对接清单

### 视觉组

- [ ] 固定上位机协议版本和结构体定义
- [ ] 明确坐标系、单位和正方向
- [ ] 提供 `tools/crc.cpp` 的 CRC16 实现
- [ ] 提供合法收发帧样例（十六进制）
- [ ] 提供正常发送频率和最大允许延迟
- [ ] 串口断连时发送/保持安全状态
- [ ] 测试程序默认不开火

### 电控组

- [ ] 完成 USB CDC 双向收发
- [ ] 结构体采用 1 字节对齐并检查 `sizeof`
- [ ] 实现 `SP` 帧头重同步
- [ ] 实现同一 CRC16 算法
- [ ] 按稳定周期发送四元数和云台状态
- [ ] 实现角度、速度、加速度限幅
- [ ] 实现通信超时后退出控制并禁止开火
- [ ] 检查裁判系统、热量和发射机构安全条件
- [ ] 正确维护实际发弹计数 `bullet_count`

### 共同验收

- [ ] 结构体长度：电控→视觉 43 字节，视觉→电控 29 字节
- [ ] 约定小端序和 IEEE-754 单精度浮点数
- [ ] 四元数顺序为 `wxyz`
- [ ] CRC 测试向量一致
- [ ] yaw/pitch 正方向一致
- [ ] 角度、角速度、角加速度和弹速单位一致
- [ ] 模式切换一致
- [ ] 100 Hz 连续通信无明显丢帧
- [ ] USB 拔出、视觉崩溃和 CRC 错误时能够安全降级
- [ ] 开火指令不会因旧帧或失联持续生效

---

## 12. 旧版 USB2CAN 说明

旧版入口位于 `io/cboard.*`，主要 CAN ID 由 YAML 配置：

```yaml
quaternion_canid: 0x100
bullet_speed_canid: 0x101
send_canid: 0xff
can_interface: "can0"
```

旧版视觉发送的一帧包含：

```text
control + shoot + yaw + pitch + horizon_distance
```

由于经典 CAN 单帧只有 8 字节，该协议没有完整传输新版轨迹规划器的 yaw/pitch 速度和加速度前馈。若继续使用旧协议，需要视觉和电控双方重新设计多帧协议，并处理帧序号、原子更新和超时；因此新版步兵轨迹规划器优先采用 USB CDC 协议。

---

## 13. 当前协议已知注意事项

1. 串口波特率没有在 `Gimbal` 中显式配置。
2. 协议没有版本号、负载长度、序号和时间戳。
3. 使用原始 C/C++ 结构体直接序列化，依赖双方的数据布局与字节序一致。
4. 视觉接收时间是主机完成帧头读取附近的时间，不是 IMU 原始采样时间。
5. 模式字段没有独立的错误码或健康状态。
6. 当前视觉端重连后会清空四元数队列，但协议层没有重新握手过程。

第一版联调可以保持现有协议以快速跑通；稳定后建议增加协议版本、帧长、序号、采样时间戳和设备状态字段。