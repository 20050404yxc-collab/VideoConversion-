// 文件：conversion_controller.h
// 层：Control
// 作用：管理有界导入、串行转换和异步停止生命周期。
// 说明：只通过 Interface 操作进程；队列和设置归 GUI 事件线程所有。
#ifndef VIDEOCONVERSION_CONVERSION_CONTROLLER_H
#define VIDEOCONVERSION_CONVERSION_CONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QVector>

#include "conversion_types.h"
#include "ffmpeg_service.h"
#include "output_file_manager.h"
#include "settings_store.h"

namespace videoconversion {
class ConversionController : public QObject
{
    Q_OBJECT
   public:
    // service/store 为入口拥有的对象，必须比控制器活得更久。
    explicit ConversionController(FfmpegService *service, SettingsStore *store,
                                  QObject *parent = nullptr);
    // 异步校验工具和编码器，校验完成前不能开始转换。
    void initialize();
    // 加入真实文件并自动串行探测，活动批次中拒绝导入。
    void addFiles(const QStringList &paths);
    // 空闲时移除指定项；活动探测被移除时先异步取消该探测。
    void removeTasks(const QVector<quint64> &ids);
    void clearTasks();
    // 空闲时更新并保存偏好，不改变已开始批次的快照。
    void savePreferences(const ConversionSettings &settings);
    // 保存偏好并对本批 Waiting 项取得设置快照；重复开始不重复调度。
    bool startBatch(const ConversionSettings &settings);
    // 取消当前转换或结果探测，清理完成后继续下一项。
    void cancelCurrent();
    // 禁止后续调度，取消尚未开始的本批任务并停止当前进程。
    void stopAll();
    // 关闭窗口前停止所有操作，清理结束后发送 shutdownReady。
    void requestShutdown();
    // 读取队列只读快照，窗口不得保存内部项引用跨事件使用。
    const QVector<ConversionTask> &tasks() const;
    ConversionSettings preferences() const;
    QString sessionLog() const;
    bool batchActive() const;
    bool hasActiveWork() const;
    bool canStart() const;
    // 两个工具与必需编码器的异步校验已全部通过。
    bool toolsReady() const;
   signals:
    // 队列或展示字段发生变化。
    void tasksChanged();
    // 按钮锁定、工具就绪或批次状态发生变化。
    void stateChanged();
    // 可理解的操作拒绝或警告，详细上下文同时进入会话日志。
    void notice(const QString &text);
    // 已记录的日志内容，窗口按需显示。
    void logAdded(const QString &text);
    // 未知时长 percent 为 -1；已知时长提交前最多 99。
    void currentProgress(quint64 taskId, int percent, int position, int total);
    // 一次批次结束仅通知一次。
    void batchFinished(const videoconversion::BatchSummary &summary);
    // 已停止所有进程并清理当前临时文件，可安全销毁窗口。
    void shutdownReady();

   private:
    // 统一接收工具完成，请求编号与操作类型过滤过期回调。
    void onCompleted(quint64 requestId, const ProcessResult &result);
    // 合并调度请求，避免完成回调里的递归转换链。
    void schedule();
    void pump();
    // 从 Waiting 批次项启动转换，失败仍进入下一项。
    void beginConversion(ConversionTask *task);
    // 处理探测和抽帧，抽帧失败仅提示并保留有效媒体。
    void completeInspection(ConversionTask *task, const ProcessResult &result);
    // 处理编码退出、结果验证与不覆盖提交。
    void completeConversion(ConversionTask *task, const ProcessResult &result);
    // 清理已停止任务的临时输出并提交唯一终态。
    void settle(ConversionTask *task, TaskState state, const QString &detail);
    // 从任务终态计算一次批次汇总，再释放编辑锁。
    void finishBatch();
    // 启动带新请求编号的工具操作；false 表示内部调度违反单进程约束。
    bool launch(Operation operation, const QStringList &arguments, int timeoutMs);
    // 用稳定标识重新查找，允许探测期间移除项。
    ConversionTask *findTask(quint64 id);
    // 追加有界日志，避免长会话无限占用内存。
    void appendLog(const QString &text);
    void report(const QString &text);

    FfmpegService *m_service;
    SettingsStore *m_store;
    QVector<ConversionTask> m_tasks;
    ConversionSettings m_preferences;
    ConversionSettings m_snapshot;
    OutputFileManager m_outputs;
    QVector<quint64> m_batchIds;
    QString m_log;
    quint64 m_nextTaskId = 0;
    quint64 m_requestId = 0;
    quint64 m_activeId = 0;
    int m_position = 0;
    bool m_initializing = false;
    bool m_toolsReady = false;
    bool m_batchActive = false;
    bool m_stopping = false;
    bool m_shutdown = false;
    bool m_shutdownNotified = false;
    bool m_pumpPending = false;
};
}
#endif
