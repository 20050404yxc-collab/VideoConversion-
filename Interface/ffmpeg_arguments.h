// 文件：ffmpeg_arguments.h
// 层：Interface
// 作用：声明视频输出选项与外部工具参数生成。
// 说明：使用参数列表，不拼接 shell，不进行文件提交。
#ifndef VIDEOCONVERSION_FFMPEG_ARGUMENTS_H
#define VIDEOCONVERSION_FFMPEG_ARGUMENTS_H

#include <QStringList>

#include "media_range.h"
#include "media_types.h"

namespace videoconversion {

enum class Container
{
    Mp4,
    Mkv
};
enum class Quality
{
    High,
    Standard,
    Smaller
};
enum class Resolution
{
    Original,
    FullHd,
    Hd
};

struct EncodingOptions
{
    Container container = Container::Mp4;
    Quality quality = Quality::Standard;
    Resolution resolution = Resolution::Original;
};

// 返回容器扩展名，用于正式结果和同容器临时文件。
QString extensionFor(Container container);
// 按显示比例计算不放大、偶数且符合方向上限的输出尺寸。
QSize outputSize(const MediaInfo &media, Resolution resolution);
// 生成完整 JSON 探测参数；文件名按独立参数传递。
QStringList probeArguments(const QString &path);
// 抽取首帧 PNG 到标准输出，失败不影响信息有效性。
QStringList thumbnailArguments(const QString &path, const MediaInfo &media);
// 转换第一路视频及已选音频到临时路径，启用机器可读进度。
QStringList conversionArguments(const QString &input, const QString &temporary,
                                const MediaInfo &media, const EncodingOptions &options,
                                const MediaRange &range = {});
// 核验实际输出的编码、容器、尺寸、音轨和已知时长。
bool validateOutput(const MediaInfo &input, const MediaInfo &output, const EncodingOptions &options,
                    QString *error, const MediaRange &range = {});

}
#endif
