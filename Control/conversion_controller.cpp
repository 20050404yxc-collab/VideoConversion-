// 文件：conversion_controller.cpp
// 层：Control
// 作用：串行调度媒体操作并保证取消、关闭及文件提交边界。
// 说明：所有回调在同一事件线程，进程结束后才能清理临时输出。
#include "conversion_controller.h"

#include <algorithm>
#include <cmath>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

namespace videoconversion {
ConversionController::ConversionController(FfmpegService *service, SettingsStore *store,
                                           QObject *parent)
    : QObject(parent), m_service(service), m_store(store)
{
    // 加载偏好与警告，入口保证服务和设置对象的生命周期。
    QString warnings;
    m_preferences = m_store->load(&warnings);
    appendLog(warnings);
    connect(m_service, &FfmpegService::completed, this, &ConversionController::onCompleted);
    connect(m_service, &FfmpegService::diagnostic, this, &ConversionController::appendLog);
    connect(m_service, &FfmpegService::progress, this, [this](quint64 requestId, double seconds) {
        // 只接收当前运行代次的转换进度，验证和取消阶段不得复活进度。
        ConversionTask *task = findTask(m_activeId);
        if (requestId != m_requestId || !task || task->state != TaskState::Converting) {
            return;
        }
        // 时长未知使用不确定进度，已知时长保持单调且提交前不达 100。
        const double duration = m_batchRanges.value(task->id).duration(task->media.duration);
        const int percent =
            duration > 0 ? std::min(99, static_cast<int>(std::min(99.0, seconds / duration * 100)))
                         : -1;
        task->percent = std::max(task->percent, percent);
        emit currentProgress(task->id, duration > 0 ? task->percent : -1, m_position,
                             m_batchIds.size());
    });
}

void ConversionController::initialize()
{
    // 初始化不打断现有操作，重复调用不会重启校验进程。
    if (m_initializing || m_service->busy() || m_batchActive || m_shutdown) {
        return;
    }
    m_initializing = true;
    m_toolsReady = false;
    launch(Operation::CheckEncoder, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")},
           15000);
    emit stateChanged();
}

bool ConversionController::launch(Operation operation, const QStringList &arguments, int timeoutMs)
{
    return m_service->start(++m_requestId, operation, arguments, timeoutMs);
}

void ConversionController::addFiles(const QStringList &paths)
{
    // 活动批次或退出中不能更改导入队列。
    if (m_batchActive || m_shutdown || m_previewId != 0) {
        report(QStringLiteral("转换期间无法添加视频。"));
        return;
    }
    // 规范化现有文件路径，Windows 按不区分大小写去重。
    for (const QString &path : paths) {
        const QFileInfo file(path);
        // 目录或无访问权限的路径只提示跳过，不在队列留下不可处理项。
        if (!file.isFile() || !file.isReadable()) {
            report(QStringLiteral("跳过目录或不可读文件：%1").arg(path));
            continue;
        }
        QString normalized = QDir::cleanPath(file.canonicalFilePath());
#ifdef Q_OS_WIN
        normalized = normalized.toCaseFolded();
#endif
        const auto duplicate = std::find_if(
            m_tasks.cbegin(), m_tasks.cend(),
            [normalized](const ConversionTask &task) { return task.normalizedPath == normalized; });
        // 重复项保持原状态，不自动重试已完成任务。
        if (duplicate != m_tasks.cend()) {
            report(QStringLiteral("重复视频已跳过：%1").arg(file.fileName()));
            continue;
        }
        ConversionTask task;
        task.id = ++m_nextTaskId;
        task.input = file.absoluteFilePath();
        task.normalizedPath = normalized;
        task.bytes = file.size();
        m_tasks.append(task);
    }
    // 新项由下一次事件统一调度，界面先显示读取状态。
    emit tasksChanged();
    emit stateChanged();
    schedule();
}

void ConversionController::removeTasks(const QVector<quint64> &ids)
{
    // 转换批次锁定列表，空闲探测阶段允许编辑。
    if (m_batchActive || m_shutdown || m_previewId != 0) {
        return;
    }
    // 被删除的活动探测先取消；完成时找不到标识便不回写该项。
    if (ids.contains(m_activeId) && m_activeId != 0) {
        m_service->cancel();
    }
    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
                       [ids](const ConversionTask &task) { return ids.contains(task.id); }),
        m_tasks.end());
    emit tasksChanged();
    emit stateChanged();
    schedule();
}

void ConversionController::clearTasks()
{
    // 转成稳定标识列表，复用移除与活动探测取消规则。
    QVector<quint64> ids;
    for (const ConversionTask &task : m_tasks) {
        ids.append(task.id);
    }
    removeTasks(ids);
}

bool ConversionController::startBatch(const ConversionSettings &settings)
{
    // 探测未完成、工具未就绪或没有等待项时不能启动。
    if (!canStart()) {
        report(QStringLiteral("请等待信息读取和工具检查完成，并添加有效的待转换视频。"));
        return false;
    }
    // 先验证全部等待项，任何错误都拒绝整批，不发生部分启动。
    for (const ConversionTask &task : m_tasks) {
        // 已保存范围必须仍在原始输入时长内，错误包含文件名。
        if (task.state == TaskState::Waiting && !task.range.valid(task.media.duration)) {
            report(QStringLiteral("%1：片段范围无效，请重新选择。").arg(task.input));
            return false;
        }
    }
    // 保存偏好与本批快照；保存失败只提示，不破坏本批已选设置。
    m_preferences = settings;
    m_snapshot = settings;
    QString error;
    if (!m_store->save(settings, &error)) {
        report(error);
    }
    m_batchIds.clear();
    m_batchRanges.clear();
    for (const ConversionTask &task : m_tasks) {
        // 只选等待项，终态需要移除再添加才能重试。
        if (task.state == TaskState::Waiting) {
            m_batchIds.append(task.id);
            m_batchRanges.insert(task.id, task.range);
        }
    }
    // 先锁定编辑再异步调度，防止重复点击和设置变化。
    m_batchActive = true;
    m_stopping = false;
    m_position = 0;
    appendLog(QStringLiteral("开始批次，共 %1 项；输出：%2")
                  .arg(m_batchIds.size())
                  .arg(settings.outputDirectory));
    emit stateChanged();
    schedule();
    return true;
}

bool ConversionController::beginPreview(quint64 id, QString *error)
{
    // 活动批次、其他预览或退出过程不能新建预览会话。
    const ConversionTask *task = findTask(id);
    if (m_batchActive || m_shutdown || m_previewId != 0 || !task ||
        task->state == TaskState::Inspecting || !task->mediaValid ||
        !QFileInfo(task->input).isFile() || !QFileInfo(task->input).isReadable()) {
        *error = QStringLiteral("请选择一个已读取信息且原文件可访问的视频，转换期间不能预览。");
        return false;
    }
    // 锁住队列与批次入口；其他文件的独立探测仍可继续。
    m_previewId = id;
    error->clear();
    emit stateChanged();
    return true;
}

void ConversionController::endPreview()
{
    // 先释放互斥再通知窗口恢复操作，不提交草稿。
    m_previewId = 0;
    emit stateChanged();
}

bool ConversionController::saveRange(quint64 id, const MediaRange &range, QString *error)
{
    // 预览拥有提交资格，终态和运行中任务均不能变成新的等待项。
    ConversionTask *task = findTask(id);
    if (m_batchActive || m_shutdown || m_previewId != id || !task ||
        task->state != TaskState::Waiting || !range.valid(task->media.duration) ||
        !QFileInfo(task->input).isReadable()) {
        *error = QStringLiteral("无法保存：任务不可编辑或片段超出输入时长。");
        return false;
    }
    // 原子提交当前项的值副本，其他任务和输出偏好不受影响。
    task->range = range;
    error->clear();
    emit tasksChanged();
    return true;
}

bool ConversionController::previewActive() const { return m_previewId != 0; }

void ConversionController::recordPreviewDiagnostic(const QString &text) { appendLog(text); }

void ConversionController::savePreferences(const ConversionSettings &settings)
{
    // 批次运行中忽略配置修改，保持设置快照与界面锁一致。
    if (m_batchActive || m_shutdown || m_previewId != 0) {
        return;
    }
    m_preferences = settings;
    QString error;
    // 保存失败需披露，当前会话仍保留用户选择。
    if (!m_store->save(settings, &error)) {
        report(error);
    }
}

void ConversionController::schedule()
{
    // 多个状态变化合并为一次事件，避免同步失败形成递归深链。
    if (m_pumpPending) {
        return;
    }
    m_pumpPending = true;
    QTimer::singleShot(0, this, [this] {
        // QObject 上下文保证销毁后不执行调度。
        m_pumpPending = false;
        pump();
    });
}

void ConversionController::pump()
{
    // 取消等待结束前不得启动后续操作或清理正在写入的文件。
    if (m_service->busy() || m_initializing) {
        return;
    }
    // 退出优先于所有新任务，汇总完成后通知窗口可关闭。
    if (m_shutdown) {
        // 当前进程已停止；如有批次先结算汇总，再通知安全退出。
        if (m_batchActive) {
            finishBatch();
        }
        // 重复关闭或延迟调度只允许一次安全退出通知。
        if (!m_shutdownNotified) {
            m_shutdownNotified = true;
            emit shutdownReady();
        }
        return;
    }
    // 批次只访问开始时选中的标识，严格按导入顺序串行运行。
    if (m_batchActive) {
        for (quint64 id : m_batchIds) {
            ConversionTask *task = findTask(id);
            // 停止全部已将剩余项设为取消，不再调度。
            if (task && task->state == TaskState::Waiting && !m_stopping) {
                ++m_position;
                beginConversion(task);
                return;
            }
        }
        finishBatch();
        return;
    }
    // 依次探测和抽帧，不为一次导入启动大量进程。
    if (m_toolsReady) {
        for (ConversionTask &task : m_tasks) {
            // 未探测项依次占用唯一进程槽，完成后才轮到下一项。
            if (task.state == TaskState::Inspecting) {
                m_activeId = task.id;
                launch(Operation::Probe, probeArguments(task.input), 15000);
                return;
            }
        }
    }
    emit stateChanged();
}

void ConversionController::onCompleted(quint64 requestId, const ProcessResult &result)
{
    // 服务代次不匹配的回调不能更改当前队列与调度。
    if (requestId != m_requestId) {
        return;
    }
    // 启动校验先查编码器，再检查 ffprobe 可启动性。
    if (result.operation == Operation::CheckEncoder || result.operation == Operation::CheckProbe) {
        const QString encoders = QString::fromLatin1(result.output);
        const bool encoderAvailable =
            encoders.contains(QRegularExpression(QStringLiteral("\\blibx264\\b"))) &&
            encoders.contains(QRegularExpression(QStringLiteral("\\baac\\b")));
        // 关闭请求或校验失败必须释放初始化状态，否则退出会无限等待。
        if (m_shutdown || !result.success ||
            (result.operation == Operation::CheckEncoder && !encoderAvailable)) {
            m_initializing = false;
            m_toolsReady = false;
            // 主动退出不用再弹出工具失败提示。
            if (!m_shutdown) {
                report(result.success ? QStringLiteral("FFmpeg 未提供 libx264/AAC 编码器。")
                                      : result.error);
            }
        } else if (result.operation == Operation::CheckEncoder) {
            // 编码器满足后独立验证 ffprobe，不假定两个工具总是同时存在。
            launch(Operation::CheckProbe, {QStringLiteral("-version")}, 15000);
            return;
        } else {
            // 全部检查完成后解锁有效视频的导入处理和转换。
            m_initializing = false;
            m_toolsReady = true;
            appendLog(QStringLiteral("FFmpeg / ffprobe 与 H.264/AAC 编码器检查通过。"));
        }
        emit stateChanged();
        schedule();
        return;
    }
    ConversionTask *task = findTask(m_activeId);
    // 探测期间被移除的任务只释放活动标识，不向新项写入旧结果。
    if (!task) {
        m_activeId = 0;
        schedule();
        return;
    }
    // 按操作类型分流，业务取消标志在进程退出后仍优先于正常完成。
    if (result.operation == Operation::Probe || result.operation == Operation::Thumbnail) {
        completeInspection(task, result);
    } else {
        // 转换与验证共享临时文件事务，交给转换结算链处理。
        completeConversion(task, result);
    }
    emit tasksChanged();
    emit stateChanged();
    schedule();
}

void ConversionController::completeInspection(ConversionTask *task, const ProcessResult &result)
{
    // 退出中的信息探测不再进入可转换状态或启动抽帧。
    if (m_shutdown || result.cancelled) {
        task->state = TaskState::Cancelled;
        task->detail = QStringLiteral("读取已取消。");
        m_activeId = 0;
        return;
    }
    // 信息无效使该项失败，其他导入项仍继续探测。
    if (result.operation == Operation::Probe) {
        QString error = result.error;
        // 工具失败或元数据不满足约束仅拒绝当前项，不阻断后续导入。
        if (!result.success || !parseMediaInfo(result.output, &task->media, &error)) {
            task->state = TaskState::Failed;
            task->detail = error;
            appendLog(QStringLiteral("信息读取失败：%1\n%2").arg(task->input, error));
            m_activeId = 0;
            return;
        }
        // 有效媒体随后抽取单帧，直到抽帧完成前不允许整批开始。
        task->mediaValid = true;
        launch(Operation::Thumbnail, thumbnailArguments(task->input, task->media), 15000);
        return;
    }
    // 图像失败使用占位图，保留已验证的媒体信息和等待状态。
    task->thumbnail = result.success ? QImage::fromData(result.output, "PNG") : QImage();
    if (task->thumbnail.isNull()) {
        task->detail = QStringLiteral("缩略图不可用，仍可转换。%1").arg(result.error);
        appendLog(QStringLiteral("%1：%2").arg(task->input, task->detail));
    }
    task->state = TaskState::Waiting;
    m_activeId = 0;
}

void ConversionController::beginConversion(ConversionTask *task)
{
    // 任务先进入转换中，准备阶段失败也属于本批结果并继续下一项。
    m_activeId = task->id;
    task->state = TaskState::Converting;
    task->percent = 0;
    task->detail.clear();
    QString error;
    if (!m_outputs.prepare(task->input, m_snapshot.outputDirectory, m_snapshot.encoding.container,
                           &error)) {
        settle(task, TaskState::Failed, error);
        emit tasksChanged();
        schedule();
        return;
    }
    // 使用不可变批次设置，将输出写入本任务拥有的唯一临时路径。
    launch(Operation::Convert,
           conversionArguments(task->input, m_outputs.temporaryPath(), task->media,
                               m_snapshot.encoding, m_batchRanges.value(task->id)),
           0);
    emit tasksChanged();
    emit currentProgress(task->id, task->media.duration > 0 ? 0 : -1, m_position,
                         m_batchIds.size());
}

void ConversionController::completeConversion(ConversionTask *task, const ProcessResult &result)
{
    // 取消标志先于正常退出处理，结果验证阶段也可以取消。
    if (task->state == TaskState::Cancelling || result.cancelled || m_shutdown) {
        settle(task, TaskState::Cancelled, QStringLiteral("任务已取消。"));
        return;
    }
    // 非正常退出或工具超时不能触发验证/提交。
    if (!result.success) {
        settle(task, TaskState::Failed, result.error);
        return;
    }
    // 编码正常退出后先检查非空并异步探测输出，进度仍不表示成功。
    if (result.operation == Operation::Convert) {
        // 空文件直接失败，避免额外启动无意义的结果探测。
        if (QFileInfo(m_outputs.temporaryPath()).size() <= 0) {
            settle(task, TaskState::Failed, QStringLiteral("转换未产生非空结果。"));
            return;
        }
        task->state = TaskState::Verifying;
        task->detail = QStringLiteral("正在验证结果，尚未提交文件。");
        launch(Operation::Verify, probeArguments(m_outputs.temporaryPath()), 20000);
        return;
    }
    // 实际结果符合编码、容器、尺寸、音轨和时长后才能提交正式结果。
    MediaInfo output;
    QString error;
    if (!parseMediaInfo(result.output, &output, &error) ||
        !validateOutput(task->media, output, m_snapshot.encoding, &error,
                        m_batchRanges.value(task->id)) ||
        !m_outputs.commit(&task->output, &error)) {
        settle(task, TaskState::Failed, error);
        return;
    }
    // 不覆盖提交成功是唯一成功点，后续取消不再拥有该正式路径。
    settle(task, TaskState::Succeeded, QStringLiteral("已保存：%1").arg(task->output));
    task->percent = 100;
    emit currentProgress(task->id, 100, m_position, m_batchIds.size());
}

void ConversionController::settle(ConversionTask *task, TaskState state, const QString &detail)
{
    // 调用方只在进程已经结束或尚未启动时清理本任务临时文件。
    QString cleanupError;
    task->state = state;
    task->detail = detail;
    // 无法删除时追加残留位置，保留主要错误或取消原因。
    if (!m_outputs.cleanup(&cleanupError)) {
        task->detail += QLatin1Char('\n') + cleanupError;
    }
    // 先记录唯一终态再释放标识，调度下一项前界面可见完整结果。
    appendLog(QStringLiteral("%1\n%2").arg(task->input, task->detail));
    m_activeId = 0;
}

void ConversionController::cancelCurrent()
{
    // 只取消本批活动转换/验证，重复点击不重复变更终态。
    ConversionTask *task = findTask(m_activeId);
    if (!m_batchActive || !task ||
        (task->state != TaskState::Converting && task->state != TaskState::Verifying)) {
        return;
    }
    // 先写业务取消状态，临界时的正常退出回调也不得提交结果。
    task->state = TaskState::Cancelling;
    task->detail = QStringLiteral("正在停止并清理临时输出……");
    m_service->cancel();
    emit tasksChanged();
    emit stateChanged();
}

void ConversionController::stopAll()
{
    // 停止标志必须先于取消当前进程，禁止完成回调调度下一项。
    if (!m_batchActive) {
        return;
    }
    m_stopping = true;
    for (quint64 id : m_batchIds) {
        ConversionTask *task = findTask(id);
        // 未开始项立即标记取消，已成功的结果保持不动。
        if (task && task->state == TaskState::Waiting) {
            task->state = TaskState::Cancelled;
            task->detail = QStringLiteral("已停止全部，任务未开始。");
        }
    }
    cancelCurrent();
    emit tasksChanged();
    emit stateChanged();
    schedule();
}

void ConversionController::requestShutdown()
{
    // 禁止导入、批次与探测调度，停止完成后才允许窗口退出。
    m_shutdown = true;
    stopAll();
    m_service->cancel();
    for (ConversionTask &task : m_tasks) {
        // 未开始探测无需新建进程即可取消。
        if (task.state == TaskState::Inspecting && task.id != m_activeId) {
            task.state = TaskState::Cancelled;
        }
    }
    emit tasksChanged();
    emit stateChanged();
    schedule();
}

void ConversionController::finishBatch()
{
    // 只统计本批开始时选中的任务，不混入导入探测错误或上次结果。
    BatchSummary summary;
    for (quint64 id : m_batchIds) {
        const ConversionTask *task = findTask(id);
        // 正常批次禁止移除；仍防护异常缺失标识，避免访问无效项。
        if (!task) {
            continue;
        }
        // 成功、失败和取消分别累计，终态不自动再运行。
        switch (task->state) {
        // 已提交结果计入成功，后续取消不影响这个终态。
        case TaskState::Succeeded:
            ++summary.succeeded;
            break;
        // 探测后转换或提交失败计入本批失败。
        case TaskState::Failed:
            ++summary.failed;
            break;
        // 停止后的未运行项或活动取消项均计入取消。
        default:
            ++summary.cancelled;
            break;
        }
    }
    // 释放锁定后发一次汇总，已完成文件继续保留。
    m_batchActive = false;
    m_stopping = false;
    appendLog(QStringLiteral("批次完成：成功 %1，失败 %2，取消 %3")
                  .arg(summary.succeeded)
                  .arg(summary.failed)
                  .arg(summary.cancelled));
    emit batchFinished(summary);
    emit stateChanged();
}

ConversionTask *ConversionController::findTask(quint64 id)
{
    // 按稳定标识查找，索引不随列表编辑泄漏到异步回调。
    for (ConversionTask &task : m_tasks) {
        // 找到标识时返回只在当前事件内使用的非拥有指针。
        if (task.id == id) {
            return &task;
        }
    }
    return nullptr;
}

void ConversionController::appendLog(const QString &text)
{
    // 空文本不生成空日志；长会话仅保留最近约 1 Mi 个字符。
    if (text.isEmpty()) {
        return;
    }
    const QString entry =
        QStringLiteral("[%1] %2\n")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), text);
    m_log = (m_log + entry).right(1024 * 1024);
    emit logAdded(entry);
}

void ConversionController::report(const QString &text)
{
    // 拒绝和警告同时进入日志与界面可见提示。
    appendLog(text);
    emit notice(text);
}

const QVector<ConversionTask> &ConversionController::tasks() const { return m_tasks; }
ConversionSettings ConversionController::preferences() const { return m_preferences; }
QString ConversionController::sessionLog() const { return m_log; }
bool ConversionController::batchActive() const { return m_batchActive; }
bool ConversionController::toolsReady() const { return m_toolsReady; }
bool ConversionController::hasActiveWork() const
{
    return m_initializing || m_service->busy() || m_batchActive ||
           std::any_of(m_tasks.cbegin(), m_tasks.cend(), [](const ConversionTask &task) {
               return task.state == TaskState::Inspecting;
           });
}

bool ConversionController::canStart() const
{
    // 所有导入检查完成才允许开始，终态不会自动参加新批次。
    if (m_shutdown || m_initializing || !m_toolsReady || m_service->busy() || m_batchActive ||
        m_previewId != 0) {
        return false;
    }
    bool waiting = false;
    for (const ConversionTask &task : m_tasks) {
        // 存在尚未探测项时保持开始按钮禁用。
        if (task.state == TaskState::Inspecting) {
            return false;
        }
        waiting = waiting || task.state == TaskState::Waiting;
    }
    return waiting;
}
}
