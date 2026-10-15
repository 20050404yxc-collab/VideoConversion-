// 文件：media_range.h
// 层：Interface
// 作用：定义相对于输入正常播放起点的单片段范围。
// 说明：整数毫秒；未启用表示整段，不用于输出偏好持久化。
#ifndef VIDEOCONVERSION_MEDIA_RANGE_H
#define VIDEOCONVERSION_MEDIA_RANGE_H

#include <cmath>

#include <QtGlobal>

namespace videoconversion {
struct MediaRange
{
    bool enabled = false;
    qint64 startMs = 0;
    qint64 endMs = 0;

    // 已知输入时长内的正长度范围才有效；整段允许未知时长。
    bool valid(double sourceSeconds) const
    {
        return !enabled || (std::isfinite(sourceSeconds) && sourceSeconds > 0 && startMs >= 0 &&
                            endMs > startMs && endMs <= std::llround(sourceSeconds * 1000));
    }
    // 片段返回目标秒数，整段保留原始时长及未知语义。
    double duration(double sourceSeconds) const
    {
        return enabled ? (endMs - startMs) / 1000.0 : sourceSeconds;
    }
};
}
#endif
