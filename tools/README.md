# FFmpeg 工具

本次开发与测试使用 Gyan 的 Windows 64 位 FFmpeg **9.0.2 essentials build**，FFmpeg 与 ffprobe 来自同一压缩包；工具检查要求 `libx264` 与 `aac` 编码器可用。

- 下载入口：[Gyan FFmpeg builds](https://www.gyan.dev/ffmpeg/builds/)
- 本次下载：[release essentials ZIP](https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip)
- 压缩包目录：`ffmpeg-9.0.2-essentials_build`
- 本次压缩包 SHA256：`60f467265b1e312373dbcd92200c2618a74850f98d3d078e94296bb3fa2047ba`
- GPL v3 或后续版本，构建启用 `--enable-gpl --enable-version3`，包含 libx264 等第三方组件；原包 LICENSE 随发布包提供。
- FFmpeg 项目：[源码与许可证](https://ffmpeg.org/legal.html)，构建提供者源码说明见上方 Gyan 入口与原包 README。
- 本次构建提供者注明的 FFmpeg 源码：[commit 946fcce07b](https://github.com/FFmpeg/FFmpeg/commit/946fcce07b)，原包 README 随包保留为 `build-provider-README.txt`。

开发时将 `ffmpeg.exe`、`ffprobe.exe`、原包 `LICENSE` 放入本地 `tools/bin/`，该目录与下载压缩包不提交 Git。设置 `VIDEOCONVERSION_TOOLS` 为此目录的绝对路径后启动开发版。

发布版携带 `tools/ffmpeg.exe` 和 `tools/ffprobe.exe`，默认从程序旁定位；版本、配置与各文件 SHA256 记录在发布包中。上述下载 URL 会随发行版本更新；更换压缩包时必须更新版本、指纹与测试记录，不能沿用本次验证结论。

简单说：开发机要准备这一对工具；发布包已经把它们带上。更换工具版本后需要重新检查兼容性。

## VLC 播放运行时

第四版使用 VideoLAN **VLC 3.0.24 Windows x64** 的 C API 动态加载，不链接 MSVC 导入库，也不使用 VLC Qt 界面或第三方 Qt 包装库。

- 官方归档：[VLC 3.0.24 win64 ZIP](https://download.videolan.org/pub/videolan/vlc/3.0.24/win64/vlc-3.0.24-win64.zip)
- ZIP SHA256：`fcf30850371ad10c9373cc4f0f4501e7dee49e3e9ae9f20c72fb2661a1ca6323`，已对照官方校验文件核对。
- 对应源码：[VLC 3.0.24](https://download.videolan.org/pub/videolan/vlc/3.0.24/vlc-3.0.24.tar.xz)，SHA256：`e7cab503d1d7d5849b89d2cf0e1ee60d0ef6d012407791b644b9cfc0cc225fdf`。
- 执行 `scripts/prepare_vlc.ps1` 下载、校验归档、提取 LGPL 文本并生成逐文件指纹；无效归档不会解压，已有运行时也逐项对照官方 ZIP。
- 开发时设置 `VIDEOCONVERSION_VLC` 为 `tools/vlc/vlc-3.0.24` 的绝对路径，Qt Creator 运行环境同样需要设置；VLC 目录不能只复制一个 DLL。
- Qt Creator 入口为“项目 → 运行 → 运行环境”：本机填写 `VIDEOCONVERSION_VLC=D:\code\VideoConversion\tools\vlc\vlc-3.0.24`。已有 FFmpeg 变量不能代替它；终端变量不会自动传给已经打开的 IDE。影子构建 EXE 旁也可放置两个核心 DLL 和完整 `plugins`、`hrtfs`、`lua`，无需将库安装到 Qt 目录或添加 `.lib` 链接项。完整步骤见仓库 README 的 Qt Creator 故障排查。
- 发布脚本复制核心 DLL、完整 `plugins`、`hrtfs`、`lua` 及 GPL/LGPL 文本和来源清单；逐文件校验失败就拒绝打包。VLC 3 自动从 DLL 旁发现插件，未使用已废弃的 `--plugin-path` 参数。

libVLC 核心采用 LGPL 2.1+，官方完整播放器/插件含 GPL 组件；不能把整套插件笼统写成 LGPL。此工程本身尚无作者指定许可证，正式分发时仍需按实际二进制及对应源码满足许可要求，详见依赖说明。
