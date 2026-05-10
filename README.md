## Supported Languages

| 语言代码 | 中文名称 | 英文标准名称 |
| --- | --- | --- |
| zh_CN | 中文（简体） | Chinese (China) |
| en_US | 英文（美国） | English (United States) |
| ja_JP | 日文 | Japanese (Japan) |
| ko_KR | 韩文 | Korean (South Korea) |
| fr_FR | 法文 | French (France) |
| de_DE | 德文 | German (Germany) |
| es_ES | 西班牙文 | Spanish (Spain) |
| ru_RU | 俄语 | Russian (Russia) |
| pt_BR | 葡萄牙语（巴西） | Portuguese (Brazil) |
| pt_PT | 葡萄牙语（葡萄牙） | Portuguese (Portugal) |
| zh_TW | 中文（繁体，台湾） | Chinese (Taiwan, China) |
| fr_CA | 法语（加拿大） | French (Canada) |
| it_IT | 意大利语 | Italian (Italy) |
| en_CA | 英语（加拿大） | English (Canada) |
| ar_SA | 阿拉伯语 | Arabic (Saudi Arabia) |
| hi_IN | 印地语 | Hindi (India) |
| id_ID | 印尼语 | Indonesian (Indonesia) |
| nl_NL | 荷兰语 | Dutch (Netherlands) |
| pl_PL | 波兰语 | Polish (Poland) |
| tr_TR | 土耳其语 | Turkish (Turkey) |
| th_TH | 泰语 | Thai (Thailand) |
| vi_VN | 越南语 | Vietnamese (Vietnam) |


## Download

[PictureView1.53-x86_64.AppImage](https://github.com/berylok/PictureView/releases/download/v1.53/PictureView1.5-x86_64.AppImage)

[PictureView1.53_win.zip](https://github.com/berylok/PictureView/releases/download/v1.53/PictureView153_win.zip)


# PictureView

**PictureView** 是一款基于 Qt6 开源框架开发的高性能图片查看器，支持常见图片格式、压缩包内预览、全屏浏览、幻灯片放映、图像变换等丰富功能。它专为 Linux 平台设计，同时保留了跨平台潜力。
它同时包含画布投影模式以帮助画师、设计师摆放图像样张临摹。

缩略图模式文件夹与ZIP压缩包：
<img width="802" height="632" alt="批注 2025-11-24 133341" src="https://github.com/user-attachments/assets/b12a74d4-57fb-417d-9e9e-703146af5d06" />

压缩包内部缩略图：

<img width="799" height="630" alt="批注 2025-11-24 133251" src="https://github.com/user-attachments/assets/f651d800-21d4-4e40-b636-cb93415bb979" />

单张大图片模式：

<img width="802" height="632" alt="批注 2025-11-24 133530" src="https://github.com/user-attachments/assets/09e6a20b-a088-401a-9f77-2da4fc3636f5" />




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

