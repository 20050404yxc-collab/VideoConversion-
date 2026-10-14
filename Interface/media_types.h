// 文件：media_types.h
// 层：Interface
// 作用：描述媒体流信息并声明 ffprobe 解析契约。
// 说明：只处理工具数据，不包含队列或界面状态。
#ifndef VIDEOCONVERSION_MEDIA_TYPES_H
#define VIDEOCONVERSION_MEDIA_TYPES_H

#include <QByteArray>
#include <QSize>
#include <QString>

namespace videoconversion {

struct MediaInfo
{
    int videoIndex = -1;
    int audioIndex = -1;
    QSize codedSize;
    QSize displaySize;
    double sampleAspect = 1.0;
    int rotation = 0;
    // 秒；负数表示未知，不能据此计算确定进度。
    double duration = -1.0;
    QString videoCodec;
    QString audioCodec;
    QString container;
};

// 读取第一路非封面视频和第一路音频；拒绝首版不能可靠处理的 HDR 或异常尺寸。
bool parseMediaInfo(const QByteArray &json, MediaInfo *info, QString *error);

}
#endif
