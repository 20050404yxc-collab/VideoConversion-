// 文件：ffmpeg_service.cpp
// 层：Interface
// 作用：管理进程信号、超时、取消和输出缓冲。
// 说明：不在事件线程等待进程退出，不在底层提交业务结果文件。
#include "ffmpeg_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace videoconversion {
namespace tool_paths {
// 优先使用指定目录和随包工具，仅开发时允许从 PATH 回退。
QString resolve(const QString &directory, const QString &name)
{
    // 发布包路径优先，防止意外混用不同版本的 ffmpeg 与 ffprobe。
    const QString candidate = QDir(directory).filePath(name + QStringLiteral(".exe"));
    if (QFileInfo::exists(candidate)) {
        return QFileInfo(candidate).absoluteFilePath();
    }
    // 指定目录缺工具时直接保留其路径，让启动错误显示真正缺失位置。
    if (!directory.isEmpty()) {
        return candidate;
    }
    return QStandardPaths::findExecutable(name);
}
}

FfmpegService::FfmpegService(const QString &toolDirectory, QObject *parent) : QObject(parent)
{
    // 优先入口显式目录，其次应用随包工具，再回退系统 PATH。
    QString directory = toolDirectory;
    const QString bundled =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tools"));
    if (directory.isEmpty() && QDir(bundled).exists()) {
        directory = bundled;
    }
    m_ffmpeg = tool_paths::resolve(directory, QStringLiteral("ffmpeg"));
    m_ffprobe = tool_paths::resolve(directory, QStringLiteral("ffprobe"));
    // 所有计时器归当前对象所有，单次超时后异步升级终止。
    m_deadline.setSingleShot(true);
    m_escalation.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        // 超时是失败而不是用户取消，保留此原因直到退出。
        m_failure = QStringLiteral("外部工具操作超时。");
        stopProcess();
    });
    connect(&m_escalation, &QTimer::timeout, this, [this] {
        // 定时器只对当前仍在运行的进程生效，不影响后续任务。
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
    });
}

FfmpegService::~FfmpegService()
{
    // 正常关闭已异步清空进程；异常销毁防止外部任务继续独立运行。
    if (m_process) {
        m_process->kill();
    }
}

bool FfmpegService::start(quint64 requestId, Operation operation, const QStringList &arguments,
                          int timeoutMs)
{
    // 进程槽尚未释放时拒绝重入，保证一个活动操作。
    if (busy()) {
        return false;
    }
    // 每次请求新建进程，回调绑定其地址；代次防护由上层请求编号补充。
    auto *process = new QProcess(this);
    m_process = process;
    m_requestId = requestId;
    m_operation = operation;
    m_output.clear();
    m_errorTail.clear();
    m_parser.reset();
    m_cancelled = false;
    m_failure.clear();
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, &FfmpegService::readOutput);
    connect(process, &QProcess::readyReadStandardError, this, &FfmpegService::readError);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process](int code, QProcess::ExitStatus status) {
                // 统一完成入口排除过期进程和重复终态。
                finish(process, code, status);
            });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        // 启动失败不会保证 finished 信号，由此入口补齐完成通知。
        if (m_process == process && error == QProcess::FailedToStart) {
            m_failure = QStringLiteral("无法启动工具：%1\n%2")
                            .arg(process->program(), process->errorString());
            finish(process, -1, QProcess::CrashExit);
        }
    });
    // 校验类操作和探测交给 ffprobe，其余交给 ffmpeg，不经过 shell。
    const bool probe = operation == Operation::CheckProbe || operation == Operation::Probe ||
                       operation == Operation::Verify;
    const QString program = probe ? m_ffprobe : m_ffmpeg;
    emit diagnostic(
        QStringLiteral("启动 %1\n参数：%2").arg(program, arguments.join(QStringLiteral(" | "))));
    // 编码没有固定总时长限制，探测和抽帧使用有限超时。
    if (timeoutMs > 0) {
        m_deadline.start(timeoutMs);
    }
    process->start(program, arguments);
    return true;
}

void FfmpegService::readOutput()
{
    // 延迟到达的 readyRead 在完成后不再处理。
    if (!m_process) {
        return;
    }
    const QByteArray chunk = m_process->readAllStandardOutput();
    // 转换标准输出只含机器进度，不累计整个视频的进度文本。
    if (m_operation == Operation::Convert) {
        const QVector<double> records = m_parser.feed(chunk);
        for (double seconds : records) {
            emit progress(m_requestId, seconds);
        }
    } else {
        // JSON 和单帧图像最多保留 8 MiB，超限停止而不是截断后伪造成功。
        constexpr int kOutputLimit = 8 * 1024 * 1024;
        if (chunk.size() > kOutputLimit - m_output.size()) {
            m_failure = QStringLiteral("工具输出超出 8 MiB 安全上限。");
            stopProcess();
        } else {
            m_output += chunk;
        }
    }
}

void FfmpegService::readError()
{
    // 完成后不接收过期进程的诊断。
    if (!m_process) {
        return;
    }
    // 保留有限尾部用于错误信息，完整会话日志由控制层有界管理。
    const QByteArray chunk = m_process->readAllStandardError();
    m_errorTail = (m_errorTail + chunk).right(65536);
    if (!chunk.isEmpty()) {
        emit diagnostic(QString::fromLocal8Bit(chunk));
    }
}

void FfmpegService::finish(QProcess *process, int exitCode, QProcess::ExitStatus exitStatus)
{
    // errorOccurred 与 finished 可交错；只受理仍占用槽的进程一次。
    if (m_process != process) {
        return;
    }
    // 消费进程退出前剩余数据，停止旧任务计时器。
    readOutput();
    readError();
    m_deadline.stop();
    m_escalation.stop();
    ProcessResult result;
    result.operation = m_operation;
    result.cancelled = m_cancelled;
    result.success =
        !m_cancelled && m_failure.isEmpty() && exitCode == 0 && exitStatus == QProcess::NormalExit;
    result.output = m_output;
    // 失败保留首要原因及工具尾部，不把取消信号误报为转换成功。
    if (!result.success) {
        result.error = m_failure.isEmpty() ? QStringLiteral("工具退出失败，退出码 %1。\n%2")
                                                 .arg(exitCode)
                                                 .arg(QString::fromLocal8Bit(m_errorTail))
                                           : m_failure;
    }
    const quint64 requestId = m_requestId;
    m_process.clear();
    process->disconnect(this);
    process->deleteLater();
    // 在通知前释放操作槽，控制层可安全启动后续步骤。
    emit completed(requestId, result);
}

void FfmpegService::stopProcess()
{
    // 没有进程就不启用升级定时器。
    if (!m_process) {
        return;
    }
    m_deadline.stop();
    m_process->terminate();
    m_escalation.start(2000);
}

void FfmpegService::cancel()
{
    // 只有当前操作可以被取消；空闲取消不改变后续请求。
    if (!m_process) {
        return;
    }
    m_cancelled = true;
    stopProcess();
}

bool FfmpegService::busy() const { return !m_process.isNull(); }
QString FfmpegService::ffmpegPath() const { return m_ffmpeg; }
QString FfmpegService::ffprobePath() const { return m_ffprobe; }
}
