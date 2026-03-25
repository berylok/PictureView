# PictureView

**PictureView** 是一款基于 Qt6 开源框架开发的高性能图片查看器，支持常见图片格式、压缩包内预览、全屏浏览、幻灯片放映、图像变换等丰富功能。它专为 Linux 平台设计，同时保留了跨平台潜力。
它同时包含画布投影模式以帮助画师、设计师摆放图像样张临摹。

缩略图模式文件夹与ZIP压缩包：
<img width="802" height="632" alt="批注 2025-11-24 133341" src="https://github.com/user-attachments/assets/b12a74d4-57fb-417d-9e9e-703146af5d06" />

压缩包内部缩略图：

<img width="799" height="630" alt="批注 2025-11-24 133251" src="https://github.com/user-attachments/assets/f651d800-21d4-4e40-b636-cb93415bb979" />

单张大图片模式：

<img width="802" height="632" alt="批注 2025-11-24 133530" src="https://github.com/user-attachments/assets/09e6a20b-a088-401a-9f77-2da4fc3636f5" />




[PictureView1.51-x86_64_ubuntu_x11.AppImage](https://github.com/berylok/PictureView/releases/download/PictureView/PictureView1.51-x86_64_ubuntu_x11.AppImage)


[PictureView1.51_forwindows_2026_3_19.zip](https://github.com/berylok/PictureView/releases/download/PictureView/PictureView1.51_forwindows_2026_3_19.zip)


更新版本号1.5.1 ,调整文件结构，修改内存限制128至512。增加在新窗口打开图片，和打开图片文件夹菜单。修改了透明背景模式的提示。2026-03-18




2026年2月15日 在x11上的无边框图片模式，因窗口管理器不支持形状置顶，导致置顶无法穿透望悉知。如果一定要使用置顶建议使用投影画布。
提示：下载的AppImage需要先右键属性打开执行能力才能运行。

[PictureView1.5x86_64_ubuntu_x11.AppImage](https://github.com/berylok/PictureView/releases/download/PictureView/PictureView1.5-x86_64_ubuntu_x11.AppImage)

2026年1月13日 现在ubuntu或者debian可以下载打包的AppImage执行文件。

[PictureView1.421_2025_12_4_win_release.zip](https://github.com/berylok/PictureView/releases/download/PictureView/PictureView1.421_2025_12_4_win_release.zip)


## 功能特性

- 📸 **支持多种图片格式**：通过 Qt 图像插件支持 JPEG、PNG、GIF、BMP、TIFF 等。
- 📦 **压缩包预览**：集成 libarchive，可直接查看 ZIP、RAR、TAR 等压缩包内的图片。
- 🎨 **图像变换**：旋转、翻转、缩放、自适应窗口/原始尺寸。
- ⌨️ **键盘/鼠标快捷键**：支持自定义快捷键，操作便捷。
- 🖼️ **缩略图模式**：快速浏览目录下所有图片。
- ⏯️ **幻灯片放映**：自动播放，可设置间隔时间。
- 🖱️ **画布控制面板**：显示缩放比例、位置、操作提示。
- 🌙 **支持深色主题**（若系统主题支持）。
- 🌍 **国际化准备**：支持多语言（翻译文件待完善）。

## 系统要求

- **操作系统**：Linux（主要开发环境），理论上也支持 Windows 和 macOS（需适当调整依赖）。
- **编译环境**：CMake 3.16+，支持 C++17 的编译器（GCC 8+ 或 Clang 8+）。
- **Qt 版本**：Qt 6.0 或更高版本。
- **libarchive**：用于压缩包支持。
- **X11**：仅 Linux 下需要，提供窗口系统集成。

## 依赖项

### Ubuntu / Debian
```bash
sudo apt update
sudo apt install cmake build-essential qt6-base-dev qt6-tools-dev \
    libarchive-dev libx11-dev libxext-dev
# 可选：Qt 图像格式插件（如需支持更多图片格式）
sudo apt install qt6-image-formats-plugins

