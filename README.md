PictureView 是由C++代码的QT6开发框架运行于windows平台。
1.4增加基础的压缩包读取（依赖libarchive第三方库）
沉浸大图模式，简单编辑（镜像旋转），幻灯浏览；空背景桌面摆放；缩略图浏览；还可以粘贴图片，以及投影式画布模式。

技术顾问：deepseek AI 
图标创意：豆包AI。
开发：berylok

缩略图模式文件夹与ZIP压缩包：
<img width="802" height="632" alt="批注 2025-11-24 133341" src="https://github.com/user-attachments/assets/b12a74d4-57fb-417d-9e9e-703146af5d06" />

压缩包内部缩略图：

<img width="799" height="630" alt="批注 2025-11-24 133251" src="https://github.com/user-attachments/assets/f651d800-21d4-4e40-b636-cb93415bb979" />

单张大图片模式：

<img width="802" height="632" alt="批注 2025-11-24 133530" src="https://github.com/user-attachments/assets/09e6a20b-a088-401a-9f77-2da4fc3636f5" />




2026年2月15日 在x11上的无边框图片模式，因窗口管理器不支持形状置顶，导致置顶无法穿透望悉知。如果一定要使用置顶建议使用投影画布。
提示：下载的AppImage需要先右键属性打开执行能力才能运行。

[PictureView1.5x86_64_ubuntu_x11.AppImage](https://github.com/berylok/PictureView/releases/download/PictureView/PictureView1.5-x86_64_ubuntu_x11.AppImage)

2026年1月13日 现在ubuntu或者debian可以下载打包的AppImage执行文件。

[PictureView1.4-x86_64_ubuntu_x11_beta.zip](https://github.com/berylok/PictureView/releases/download/PictureView/PictureView1.4-x86_64_ubuntu_x11_beta.zip)

[PictureView1.421_2025_12_4_win_release.zip](https://github.com/berylok/PictureView/releases/download/PictureView/PictureView1.421_2025_12_4_win_release.zip)

编译环境配置：
1，windows平台：使用PictureView.pro在QT环境加载项目。

2：Cmake  for ubuntu(linux) 


2025年12月6日 更新仅针对Linux系统； x11桌面投影画布 以正确工作；调整透明度为快捷键pageup、pagedown；旋转为ctrl+；镜像为shift+；v1.430

2025年12月4日 添加缺失的许可证信息。修补带标题窗口的错误。v1.421

2025年11月30日 归档整理。已知linix系统透明穿透及画布问题。目前有些许瑕疵，待完善。

v1.4.0.0支持ZIP压缩包(+libarchive)

v1.3.8.1补齐一些功能及修复

v1.3投影画布支持

v1.2缩略图支持

v1.1幻灯支持

v1.0基础图片显示(Qt6+opencv)
