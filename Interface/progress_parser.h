// 文件：progress_parser.h
// 层：Interface
// 作用：解析 FFmpeg 分块进度记录。
// 说明：只在完整 progress 记录到达时报告时间，结束记录不是成功凭据。
#ifndef VIDEOCONVERSION_PROGRESS_PARSER_H
#define VIDEOCONVERSION_PROGRESS_PARSER_H

#include <QByteArray>
#include <QVector>

namespace videoconversion {
class ProgressParser
{
   public:
    // 消费任意分块，返回完整记录中的输出秒数；保留跨块残片。
    QVector<double> feed(const QByteArray &chunk);
    // 新任务开始前清空残片与上一记录时间。
    void reset();

   private:
    QByteArray m_pending;
    double m_recordSeconds = -1.0;
};
}
#endif
