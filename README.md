<div align="center">

# 📷 V4L2 Camera → LCD 实时显示

**在 Linux 上把摄像头画面实时怼到 LCD 上 —— 直写 `/dev/fb0` 显存，零图形库依赖**

[![Language](https://img.shields.io/badge/language-C-blue.svg)](#)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](#)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](#-license)

一个约 400 行的单文件 C 程序：枚举摄像头能力 → 交互选择分辨率 → MMAP 采集 → YUYV 转 RGB → 缩放到屏幕 → 写入 `/dev/fb0`。

</div>

---

> [!IMPORTANT]
> **声明**
>
> 本仓库**仅用于个人学习 V4L2 采集流程**，包括：能力枚举、MMAP 缓冲区申请、`DQBUF`/`QBUF` 取帧循环、YUYV→RGB 转换以及直写帧缓冲等基础环节。
>
> 代码是学习过程中的练手 demo，**并非生产级实现**，存在若干已知问题，请勿直接用于产品。
> 
> 可以作为查看摄像头属性，检测摄像头是否损坏
> 
> 📌 **后续计划**：本人将另外发布一个 **不依赖 OpenCV 库、纯 C/C++ 跑 YOLOv5 推理** 的仓库，敬请期待。

---

## ✨ 特性

| 特性 | 说明 |
|------|------|
| 🔍 能力枚举 | 自动遍历 `VIDIOC_ENUM_FMT` / `FRAMESIZES` / `FRAMEINTERVALS`，列出全部「格式 + 分辨率 + 帧率」组合 |
| 🎛 交互选模 | 终端输入编号即可切换采集分辨率，无需重编译 |
| 🚀 MMAP 零拷贝 | 4 个 MMAP 缓冲区，`DQBUF` / `QBUF` 循环取帧 |
| 🎨 色彩转换 | YUYV → RGB 定点整数运算（`>>10`），无浮点、无 libyuv |
| 🖥 直写显存 | 通过 `mmap` 直接把 RGB 像素写入 `/dev/fb0`，不依赖 SDL / DirectFB / X11 / Wayland 等任何图形库 |
| 📐 最近邻缩放 | 任意采集分辨率 → 任意 LCD 分辨率 |
| 💾 帧导出 | 内置 `save_ppm()`，可把当前帧存成 `frame.ppm` 调试 |

---

## 📋 环境要求

- Linux 内核，具备 **V4L2** 与 **Framebuffer** 支持
- 摄像头设备节点，例如 `/dev/video0`（UVC 摄像头一般可用）
- LCD 帧缓冲设备（Linux Framebuffer 兼容，如 `/dev/fb0`）
- `gcc`（代码使用 GNU 扩展类型 `__uint8_t`，请用 gcc/clang 编译）

```bash
# 确认设备存在
ls /dev/video* /dev/fb0

# 查看摄像头支持的格式（可选，v4l2-utils）
v4l2-ctl -d /dev/video0 --list-formats-ext
```

---

## 🔨 编译

```bash
gcc -O2 -Wall -o camera_fb camera_fb.c
```

无任何外部库依赖，仅需内核头文件（`linux/videodev2.h`、`linux/fb.h`）。

---

## 🚀 运行

```bash
sudo ./camera_fb /dev/video0
```

> 写 `/dev/fb0` 通常需要 root 权限或 `video` 组权限。

启动流程：

```
1. 打开 /dev/video0 与 /dev/fb0
2. 打印 LCD 信息：lcd_w:1280 lcd_h:800
3. 打印所有可用模式：
   mode=0 YUYV size(640,480) 30dps mode:0
   mode=1 YUYV size(1280,720) 30dps mode:1
   ...
4. 提示输入：
   please input mode:
5. 设置 V4L2_PIX_FMT_YUYV → 申请缓冲区 → STREAMON
6. 进入无限采集显示循环
```

退出方式：**`Ctrl+C`**（见下方 [Known Issues](#-known-issues)）。

---

## 🧠 工作原理

```
 ┌──────────┐   DQBUF    ┌──────────────┐   YUYV→RGB   ┌──────────┐   mmap    ┌────────┐
 │  摄像头  │ ─────────► │ MMAP Buffer  │ ───────────► │  缩放    │ ────────► │ /dev/fb0│
 │ (V4L2)   │   QBUF     │  (4 buffers) │              │ 最近邻   │           │  (LCD) │
 └──────────┘ ◄───────── └──────────────┘              └──────────┘           └────────┘
```

### 色彩转换公式

```
R = Y + 1.402·V
G = Y - 0.344·U - 0.714·V
B = Y + 1.772·U
```

定点近似实现：

```c
r = y + ((1436 * v) >> 10);
g = y - ((352 * u + 731 * v) >> 10);
b = y + ((1815 * u) >> 10);
```

结果裁剪到 `[0, 255]` 后：

- **16bpp** → `RGB565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)`
- **32bpp** → `0xFF000000 | (r << 16) | (g << 8) | b`

---

## 🗂 代码结构

| 函数 | 职责 |
|------|------|
| `mode_check(fd)` | 枚举所有格式/分辨率/帧率，构建全局链表 `Head` |
| `camera_list_add / _destory / look_camera_list_data` | 模式链表的增、删、打印 |
| `set_camera_mode(fd)` | 交互选择模式 → `VIDIOC_S_FMT` → 调用 `read_init` |
| `read_init(fd)` | 申请 4 个 MMAP 缓冲，`QUERYBUF` + `mmap` + `QBUF` + `STREAMON` |
| `write_init(fd2)` | 读取 LCD 分辨率/bpp，`mmap` 显存到 `buf_lcd` |
| `my_read(fd)` | `DQBUF` → `yuyv_rgb()` → `QBUF` |
| `yuyv_rgb()` | YUYV 转 RGB + 缩放 + 写显存（主循环热路径） |
| `save_ppm()` | 导出当前帧为 `frame.ppm`（调试用） |
| `exe_end(fd, fd2)` | `STREAMOFF` + `munmap` + `close` |

### 数据结构

```c
struct camera_mode {
    int index;                     // 像素格式索引
    unsigned char description[32]; // 格式描述，如 "YUYV"
    int width, high, fps;
    int mode;                      // 程序内部模式编号（0 起递增）
    struct camera_mode *next;
};

struct list_head { struct camera_mode *head, *tail; };
```

---

## 🗺 Roadmap

- [ ] 支持 MJPEG（配合 libjpeg 解码）
- [ ] 支持 V4L2 `multi-planar` API
- [ ] 双缓冲 / 页翻转减少撕裂
- [ ] 抽取 `-W` `-H` `-f` 命令行参数
- [ ] 支持 YUYV 转灰度 / 二值化等可选处理

---

## 🔗 相关项目

| 项目 | 说明 | 状态 |
|------|------|------|
| **本项目** | 个人学习 V4L2 采集流程的练手 demo | ✅ 已开源 |
| **YOLOv5 推理（无 OpenCV 依赖）** | 纯 C/C++ 实现 YOLOv5 前向推理，不依赖 OpenCV，计划与 V4L2 采集流程打通做实时检测 | 🚧 开发中，敬请期待 |

> 后续仓库发布后会在此处补充链接。

---

## 🤝 贡献

```bash
git clone https://github.com/<your-name>/v4l2-camera-fb.git
cd v4l2-camera-fb
# fork → 修改 → PR
```

Issue / PR 都欢迎 🙂 但请注意：本仓库定位为**学习 demo**，不会为了工程完备性过度扩展。

---

## 📄 License

[MIT](LICENSE) © Your Name

---

<div align="center">

**如果这个项目对你有帮助，点个 ⭐ Star 吧！**

</div>
