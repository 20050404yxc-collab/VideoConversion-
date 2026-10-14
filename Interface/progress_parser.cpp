// 文件：progress_parser.cpp
// 层：Interface
// 作用：从完整行和完整记录提取输出时间。
// 说明：out_time_ms 是 FFmpeg 的历史微秒字段，不按毫秒计算。
#include "progress_parser.h"

#include <cmath>

#include <QList>

namespace videoconversion {
QVector<double> ProgressParser::feed(const QByteArray &chunk)
{
    // 保留未完成行，避免 readyRead 边界造成时间与记录错配。
    m_pending += chunk;
    QVector<double> completed;
    int end = m_pending.indexOf('\n');
    // 仅消费完整行，末尾残片留到下次。
    while (end >= 0) {
        const QByteArray line = m_pending.left(end).trimmed();
        m_pending.remove(0, end + 1);
        const int separator = line.indexOf('=');
        const QByteArray key = line.left(separator);
        const QByteArray value = line.mid(separator + 1);
        // 优先接受微秒字段，兼容 FFmpeg 历史命名。
        if (separator > 0 && (key == "out_time_us" || key == "out_time_ms")) {
            bool valid = false;
            const double microseconds = value.toDouble(&valid);
            // 无效和非有限数据不能污染上一有效时间。
            if (valid && std::isfinite(microseconds) && microseconds >= 0) {
                m_recordSeconds = microseconds / 1000000.0;
            }
            // 时间字符串是没有微秒字段时的兼容回退。
        } else if (key == "out_time" && m_recordSeconds < 0) {
            const QList<QByteArray> fields = value.split(':');
            // 只有完整的时分秒才作回退。
            if (fields.size() == 3) {
                bool valid = false;
                const double seconds = fields.at(2).toDouble(&valid);
                // 小数秒有效时构造总秒数。
                if (valid && std::isfinite(seconds) && seconds >= 0) {
                    m_recordSeconds =
                        fields.at(0).toInt() * 3600 + fields.at(1).toInt() * 60 + seconds;
                }
            }
            // progress 行提交记录，尚未完成的时间不得提前报告。
        } else if (key == "progress") {
            // 空记录没有可报告的时间。
            if (m_recordSeconds >= 0) {
                completed.append(m_recordSeconds);
            }
            m_recordSeconds = -1.0;
        }
        end = m_pending.indexOf('\n');
    }
    // 异常工具输出不能造成无限残片增长。
    if (m_pending.size() > 65536) {
        m_pending.clear();
    }
    return completed;
}

void ProgressParser::reset()
{
    // 新进程不能继承上一进程的半行或时间。
    m_pending.clear();
    m_recordSeconds = -1.0;
}
}
