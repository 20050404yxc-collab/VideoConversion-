// 文件：ffmpeg_service.h
// 层：Interface
// 作用：拥有并异步管理有界外部工具进程。
// 说明：同一时刻仅一个操作；取消确认后才允许调用方清理文件。
#ifndef VIDEOCONVERSION_FFMPEG_SERVICE_H
#define VIDEOCONVERSION_FFMPEG_SERVICE_H

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include "progress_parser.h"

namespace videoconversion {
enum class Operation
{
    CheckEncoder,
    CheckProbe,
    Probe,
    Thumbnail,
    Convert,
    Verify
};
struct ProcessResult
{
    Operation operation = Operation::Probe;
    bool success = false;
    bool cancelled = false;
    QByteArray output;
    QString error;
};

class FfmpegService : public QObject
{
    Q_OBJECT
   public:
    // 工具目录可由入口指定；空目录使用发布包 tools 或系统 PATH。
    explicit FfmpegService(const QString &toolDirectory = QString(), QObject *parent = nullptr);
    // 正常销毁前由控制层等候 idle；异常销毁只终止本对象拥有的进程。
    ~FfmpegService() override;
    // 操作已受理返回 true；进程启动失败仍通过 completed 异步通知。
    bool start(quint64 requestId, Operation operation, const QStringList &arguments, int timeoutMs);
    // 先请求终止，2 秒后强杀；完成通知只发送一次。
    void cancel();
    // 活动进程包括启动和取消等待阶段。
    bool busy() const;
    // 返回实际工具路径，供诊断与测试记录使用。
    QString ffmpegPath() const;
    QString ffprobePath() const;
   signals:
    // 完成时已释放当前操作槽，调用方可在回调里启动下一项。
    void completed(quint64 requestId, const videoconversion::ProcessResult &result);
    // 完整进度记录中的输出秒数，尚不代表成功。
    void progress(quint64 requestId, double seconds);
    // 工具路径、启动参数与有界诊断输出。
    void diagnostic(const QString &text);

   private:
    // 分别读取标准输出与诊断，限制缓冲占用。
    void readOutput();
    void readError();
    // 汇合正常退出、崩溃与启动失败；严格忽略已完成进程的重复信号。
    void finish(QProcess *process, int exitCode, QProcess::ExitStatus exitStatus);
    // 取消和超时共用停止机制，保留各自终态原因。
    void stopProcess();
    QString m_ffmpeg;
    QString m_ffprobe;
    QPointer<QProcess> m_process;
    QTimer m_deadline;
    QTimer m_escalation;
    ProgressParser m_parser;
    quint64 m_requestId = 0;
    Operation m_operation = Operation::Probe;
    QByteArray m_output;
    QByteArray m_errorTail;
    bool m_cancelled = false;
    QString m_failure;
};
}
Q_DECLARE_METATYPE(videoconversion::ProcessResult)
#endif
