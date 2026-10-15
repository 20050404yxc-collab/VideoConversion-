// 文件：media_playback_service.h
// 层：Interface
// 作用：封装 libVLC 播放会话及其异步状态。
// 说明：所有 API 调用归 GUI 线程，播放器窗口由入口装配。
#ifndef VIDEOCONVERSION_MEDIA_PLAYBACK_SERVICE_H
#define VIDEOCONVERSION_MEDIA_PLAYBACK_SERVICE_H

#include <memory>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class QWidget;
namespace videoconversion {
struct LibVlcApi;
struct PlaybackState
{
    qint64 positionMs = 0;
    qint64 durationMs = 0;
    bool loaded = false;
    bool seekable = false;
    bool playing = false;
    bool failed = false;
    QString error;
};

class MediaPlaybackService : public QObject
{
    Q_OBJECT
   public:
    // 创建独立播放服务，所有原生接口由所属 GUI 线程调用。
    explicit MediaPlaybackService(QObject *parent = nullptr);
    // 停止媒体并释放实例后再卸载原生 DLL。
    ~MediaPlaybackService() override;
    // 创建新媒体与播放器，返回用于隔离旧通知的代次。
    quint64 load(const QString &path);
    // 停止计时器和日志回调，释放媒体会话但保留已加载的库。
    void unload();
    // 启动或恢复当前媒体。
    void play();
    // 暂停当前媒体并保留播放位置。
    void pause();
    // 以正常播放起点为零定位，单位为毫秒。
    void seek(qint64 milliseconds);
    // 保存并应用 0 至 100 的音量；加载取首帧期间保持静音。
    void setVolume(int percent);
    // 返回当前播放位置与能力的值副本。
    PlaybackState state() const;
    // 绑定窗口的原生显示表面，窗口所有权仍归 Application。
    void setVideoOutput(QWidget *output);
   signals:
    // 发布当前媒体代次的播放状态。
    void updated(quint64 generation, const videoconversion::PlaybackState &state);
    // 把带输入路径的有界诊断交给原有会话日志。
    void diagnostic(const QString &text);

   private:
    // 校验 VLC 3 版本并创建实例，失败时回报当前会话。
    bool initializeRuntime();
    // 注册当前代次的有界原生日志并启动静音首帧加载。
    bool startPlayer();
    // 将已创建播放器绑定到当前 HWND。
    void attachOutput();
    // 固定当前会话的失败状态并禁用播放能力。
    void fail(const QString &reason);
    // 轮询当前播放器并发布状态，不接收解码线程的状态回调。
    void publish();
    // 后端状态或能力改变时才记录快照。
    void logState(int backendState);
    // 为诊断附加当前输入路径和会话代次。
    void logDiagnostic(const QString &text);
    std::unique_ptr<LibVlcApi> m_api;
    void *m_instance = nullptr;
    void *m_media = nullptr;
    void *m_player = nullptr;
    QPointer<QWidget> m_output;
    quint64 m_generation = 0;
    PlaybackState m_state;
    QTimer m_loadTimer;
    QTimer m_pollTimer;
    QString m_input;
    QString m_lastDiagnosticState;
    bool m_pauseAfterStart = false;
    int m_volume = 70;
    qint64 m_pendingSeekMs = -1;
};
}
Q_DECLARE_METATYPE(videoconversion::PlaybackState)
#endif
