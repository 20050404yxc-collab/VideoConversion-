// 文件：preview_controller.h
// 层：Control
// 作用：管理输入预览会话、能力约束与片段播放边界。
// 说明：不依赖 QWidget；保存仍由队列控制器原子提交。
#ifndef VIDEOCONVERSION_PREVIEW_CONTROLLER_H
#define VIDEOCONVERSION_PREVIEW_CONTROLLER_H

#include <QObject>
#include <QTimer>

#include "conversion_controller.h"
#include "media_playback_service.h"

namespace videoconversion {
class PreviewController : public QObject
{
    Q_OBJECT
   public:
    // 非拥有依赖必须比本控制器活得更久。
    explicit PreviewController(MediaPlaybackService *service, ConversionController *conversion,
                               QObject *parent = nullptr);
    // 释放预览会话及其队列互斥。
    ~PreviewController() override;
    // 按稳定标识复制输入和已保存范围，不保存队列内部引用。
    bool open(quint64 id, QString *error);
    // 失效待加载和定位请求，卸载媒体并恢复队列操作。
    void close();
    // 返回打开时复制的任务信息，不暴露队列内部引用。
    ConversionTask task() const;
    // 返回当前会话后端状态。
    PlaybackState state() const;
    // 仅当前等待项拥有选区编辑资格。
    bool editable() const;
    // 返回当前能力限制；未知时长或后端时长差异不改变已保存范围。
    QString rangeError() const;
    // 复核播放能力后向队列提交草稿；失败保留已保存范围。
    bool save(const MediaRange &range, QString *error);
    // 普通播放或暂停会取消选区自动暂停规则。
    void togglePlayback();
    // 手动定位取消选区播放，毫秒值限制在当前媒体内。
    void seek(qint64 milliseconds);
    // 转交音量百分比，不修改统一转换设置。
    void setVolume(int percent);
    // 先完成定位再开始选区播放；到终点暂停，不循环。
    bool playSelected(const MediaRange &range, QString *error);
    // 仅撤销待定位和片段边界，不改变草稿或保存范围。
    void cancelSelected();
   signals:
    // 通知窗口刷新当前播放状态和编辑资格。
    void changed();
    // 通知异步定位失败等可见原因。
    void notice(const QString &text);

   private:
    // 接收当前服务代次的状态，忽略关闭会话与旧会话通知。
    void onPlayback(quint64 generation, const PlaybackState &state);
    MediaPlaybackService *m_service;
    ConversionController *m_conversion;
    ConversionTask m_task;
    PlaybackState m_state;
    MediaRange m_playRange;
    QTimer m_seekTimer;
    quint64 m_generation = 0;
    quint64 m_sessionToken = 0;
    bool m_open = false;
    bool m_seeking = false;
    bool m_selected = false;
};
}
#endif
