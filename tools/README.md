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
