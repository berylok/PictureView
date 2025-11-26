PictureView 是由C++代码的QT6开发框架运行于windows平台。
1.4增加基础的压缩包读取（依赖libarchive第三方库）
沉浸大图模式，简单编辑（镜像旋转），幻灯浏览；空背景桌面摆放；缩略图浏览；还可以粘贴图片，以及投影式画布模式。

技术顾问：deepseek AI 图标创意：豆包AI。开发：berylok

缩略图模式文件夹与ZIP压缩包：
<img width="802" height="632" alt="批注 2025-11-24 133341" src="https://github.com/user-attachments/assets/b12a74d4-57fb-417d-9e9e-703146af5d06" />

压缩包内部缩略图：

<img width="799" height="630" alt="批注 2025-11-24 133251" src="https://github.com/user-attachments/assets/f651d800-21d4-4e40-b636-cb93415bb979" />

单张大图片模式：

<img width="802" height="632" alt="批注 2025-11-24 133530" src="https://github.com/user-attachments/assets/09e6a20b-a088-401a-9f77-2da4fc3636f5" />


编译环境配置：
1，windows平台：使用PictureView.pro在QT环境加载项目。


2：Cmake已更新至1.4 for ubuntu(linux) 
更新：2025年11月22日11时24分
修复缩略图部分错误，修改esc可退出程序，原enter切换不变。


v1.4.0.0支持ZIP压缩包(+libarchive)
v1.3.8.1补齐一些功能及修复
v1.3投影画布支持
v1.2缩略图支持
v1.1幻灯支持
v1.0基础图片显示(Qt6+opencv)
