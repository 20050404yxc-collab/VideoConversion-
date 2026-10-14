// 文件：ffmpeg_arguments.cpp
// 层：Interface
// 作用：生成 H.264/AAC 转换参数并核验输出媒体。
// 说明：分辨率按显示方向处理，成功条件不依赖进度结束标记。
#include "ffmpeg_arguments.h"

#include <algorithm>
#include <cmath>

namespace videoconversion {

QString extensionFor(Container container)
{
    return container == Container::Mp4 ? QStringLiteral("mp4") : QStringLiteral("mkv");
}

QSize outputSize(const MediaInfo &media, Resolution resolution)
{
    // 原始策略仍需偶数编码对齐；其他策略按显示方向构造上限框。
    const QSize original = media.displaySize;
    double factor = 1.0;
    if (resolution != Resolution::Original) {
        const int longSide = resolution == Resolution::FullHd ? 1920 : 1280;
        const int shortSide = resolution == Resolution::FullHd ? 1080 : 720;
        const bool portrait = original.height() > original.width();
        const QSize bounds(portrait ? shortSide : longSide, portrait ? longSide : shortSide);
        factor = std::min({1.0, static_cast<double>(bounds.width()) / original.width(),
                           static_cast<double>(bounds.height()) / original.height()});
    }
    // 向下对齐到偶数，避免编码器拒绝奇数尺寸；有效输入至少为 2。
    return QSize(std::max(2, static_cast<int>(std::floor(original.width() * factor / 2)) * 2),
                 std::max(2, static_cast<int>(std::floor(original.height() * factor / 2)) * 2));
}

QStringList probeArguments(const QString &path)
{
    return {QStringLiteral("-v"),
            QStringLiteral("error"),
            QStringLiteral("-show_format"),
            QStringLiteral("-show_streams"),
            QStringLiteral("-of"),
            QStringLiteral("json"),
            path};
}

QStringList thumbnailArguments(const QString &path, const MediaInfo &media)
{
    return {QStringLiteral("-hide_banner"),
            QStringLiteral("-loglevel"),
            QStringLiteral("error"),
            QStringLiteral("-nostdin"),
            QStringLiteral("-i"),
            path,
            QStringLiteral("-map"),
            QStringLiteral("0:%1").arg(media.videoIndex),
            QStringLiteral("-frames:v"),
            QStringLiteral("1"),
            QStringLiteral("-vf"),
            QStringLiteral("scale=160:100:force_original_aspect_ratio=decrease,setsar=1"),
            QStringLiteral("-f"),
            QStringLiteral("image2pipe"),
            QStringLiteral("-c:v"),
            QStringLiteral("png"),
            QStringLiteral("pipe:1")};
}

QStringList conversionArguments(const QString &input, const QString &temporary,
                                const MediaInfo &media, const EncodingOptions &options)
{
    // 通用输入选项禁止覆盖，保留 FFmpeg 自动旋转，并映射实际选中视频流。
    QStringList arguments{QStringLiteral("-hide_banner"),
                          QStringLiteral("-loglevel"),
                          QStringLiteral("warning"),
                          QStringLiteral("-nostdin"),
                          QStringLiteral("-n"),
                          QStringLiteral("-i"),
                          input,
                          QStringLiteral("-map"),
                          QStringLiteral("0:%1").arg(media.videoIndex)};
    // 有音轨才映射与编码；无音轨时显式禁用音频。
    if (media.audioIndex >= 0) {
        arguments << QStringLiteral("-map") << QStringLiteral("0:%1").arg(media.audioIndex)
                  << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-b:a")
                  << QStringLiteral("128k");
    } else {
        arguments << QStringLiteral("-an");
    }
    // 转为方形像素和显示方向尺寸，删除未转换的字幕、附件与输入元数据。
    const QSize size = outputSize(media, options.resolution);
    const int crf = options.quality == Quality::High      ? 18
                    : options.quality == Quality::Smaller ? 28
                                                          : 23;
    arguments << QStringLiteral("-sn") << QStringLiteral("-dn") << QStringLiteral("-map_metadata")
              << QStringLiteral("-1") << QStringLiteral("-vf")
              << QStringLiteral("scale=%1:%2,setsar=1").arg(size.width()).arg(size.height())
              << QStringLiteral("-c:v") << QStringLiteral("libx264") << QStringLiteral("-preset")
              << QStringLiteral("fast") << QStringLiteral("-crf") << QString::number(crf)
              << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
    // MP4 前置索引便于开始播放；MKV 没有该选项。
    if (options.container == Container::Mp4) {
        arguments << QStringLiteral("-movflags") << QStringLiteral("+faststart");
    }
    // 指定容器并将进度放到独立输出流，诊断日志留在 stderr。
    arguments << QStringLiteral("-progress") << QStringLiteral("pipe:1")
              << QStringLiteral("-nostats") << QStringLiteral("-f")
              << (options.container == Container::Mp4 ? QStringLiteral("mp4")
                                                      : QStringLiteral("matroska"))
              << temporary;
    return arguments;
}

bool validateOutput(const MediaInfo &input, const MediaInfo &output, const EncodingOptions &options,
                    QString *error)
{
    // 结果必须包含预期视频、尺寸和音轨，不能仅凭非空文件宣告成功。
    if (output.videoCodec != QStringLiteral("h264") ||
        output.displaySize != outputSize(input, options.resolution) ||
        (input.audioIndex >= 0 && output.audioCodec != QStringLiteral("aac")) ||
        (input.audioIndex < 0 && output.audioIndex >= 0)) {
        *error = QStringLiteral("结果编码、音轨或分辨率与设置不一致。");
        return false;
    }
    // 容器标识可能包含别名，因此核验目标容器所对应的真实格式名称。
    const QString container =
        options.container == Container::Mp4 ? QStringLiteral("mp4") : QStringLiteral("matroska");
    if (!output.container.split(QLatin1Char(',')).contains(container)) {
        *error = QStringLiteral("结果容器与设置不一致。");
        return false;
    }
    // 输入时长明确时结果不得明显缺失；未知时长不伪造时长保证。
    if (input.duration > 0 && (output.duration <= 0 || std::abs(output.duration - input.duration) >
                                                           std::max(2.0, input.duration * 0.02))) {
        *error = QStringLiteral("结果时长异常，未提交正式文件。");
        return false;
    }
    error->clear();
    return true;
}
}
