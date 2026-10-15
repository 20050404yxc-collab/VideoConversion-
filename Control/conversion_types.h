// 文件：conversion_types.h
// 层：Control
// 作用：描述导入任务、批次设置与汇总。
// 说明：只含值对象，队列状态只能由控制层提交。
#ifndef VIDEOCONVERSION_CONVERSION_TYPES_H
#define VIDEOCONVERSION_CONVERSION_TYPES_H

#include <QImage>
#include <QString>
#include <QtGlobal>

#include "ffmpeg_arguments.h"
#include "media_types.h"

namespace videoconversion {
enum class TaskState
{
    Inspecting,
    Waiting,
    Converting,
    Verifying,
    Cancelling,
    Succeeded,
    Failed,
    Cancelled
};
struct ConversionSettings
{
    EncodingOptions encoding;
    QString outputDirectory;
};
struct ConversionTask
{
    quint64 id = 0;
    QString input;
    QString normalizedPath;
    qint64 bytes = 0;
    MediaInfo media;
    bool mediaValid = false;
    MediaRange range;
    QImage thumbnail;
    TaskState state = TaskState::Inspecting;
    QString detail;
    QString output;
    int percent = 0;
};
struct BatchSummary
{
    int succeeded = 0;
    int failed = 0;
    int cancelled = 0;
};
}
Q_DECLARE_METATYPE(videoconversion::BatchSummary)
#endif
