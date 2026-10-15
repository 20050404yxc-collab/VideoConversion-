// 文件：preview_controller.cpp
// 层：Control
// 作用：处理预览互斥、草稿提交资格和片段播放状态。
// 说明：位置和时长来自后端，转换范围仍以探测输入的正常起点为准。
#include "preview_controller.h"

#include <cmath>

namespace videoconversion {
PreviewController::PreviewController(MediaPlaybackService *service,
                                     ConversionController *conversion, QObject *parent)
    : QObject(parent), m_service(service), m_conversion(conversion)
{
    // 会话回调绑定控制器生命周期，定位超时不伪装成成功播放。
    connect(service, &MediaPlaybackService::updated, this, &PreviewController::onPlayback);
    connect(service, &MediaPlaybackService::diagnostic, conversion,
            &ConversionController::recordPreviewDiagnostic);
    connect(conversion, &ConversionController::shutdownReady, this, &PreviewController::close);
    m_seekTimer.setSingleShot(true);
    m_seekTimer.setInterval(2000);
    connect(&m_seekTimer, &QTimer::timeout, this, [this] {
        // 定位未确认时取消选区状态并提示，保留草稿与保存范围。
        m_seeking = false;
        m_selected = false;
        m_service->pause();
        m_conversion->recordPreviewDiagnostic(
            QStringLiteral(
                "[预览][任务 %1][%2] 片段起点定位超过 2000ms 未确认；目标=%3ms，当前=%4ms")
                .arg(m_task.id)
                .arg(m_task.input)
                .arg(m_playRange.startMs)
                .arg(m_state.positionMs));
        emit notice(QStringLiteral("播放器未能确认片段起点，请重新定位或取消预览。"));
        emit changed();
    });
}
PreviewController::~PreviewController() { close(); }

bool PreviewController::open(quint64 id, QString *error)
{
    // 主控制器检查访问和批次互斥，失败不改变现有任务。
    if (m_open) {
        *error = QStringLiteral("请先关闭当前预览窗口。");
        m_conversion->recordPreviewDiagnostic(
            QStringLiteral("[预览][任务 %1] 打开被拒绝：%2").arg(id).arg(*error));
        return false;
    }
    // 队列检查失败保留原队列状态并向调用方提供拒绝原因。
    if (!m_conversion->beginPreview(id, error)) {
        m_conversion->recordPreviewDiagnostic(
            QStringLiteral("[预览][任务 %1] 打开被拒绝：%2").arg(id).arg(*error));
        return false;
    }
    // 按稳定标识复制完整值，在异步事件中不访问 QVector 元素地址。
    for (const ConversionTask &task : m_conversion->tasks()) {
        // beginPreview 已验证标识存在；复制该项以初始化独立会话。
        if (task.id == id) {
            m_task = task;
            break;
        }
    }
    // 探测信息与播放器能力分开记录，避免将 FFmpeg 可解码误当作系统能播放。
    const MediaInfo &media = m_task.media;
    m_conversion->recordPreviewDiagnostic(
        QStringLiteral(
            "[预览][任务 %1][%2] ffprobe：container=%3；video=%4；audio=%5；duration=%6s；"
            "display=%7x%8；rotation=%9；range=%10/%11-%12ms")
            .arg(id)
            .arg(m_task.input, media.container, media.videoCodec,
                 media.audioIndex >= 0 ? media.audioCodec : QStringLiteral("<无音轨>"))
            .arg(media.duration, 0, 'f', 3)
            .arg(media.displaySize.width())
            .arg(media.displaySize.height())
            .arg(media.rotation)
            .arg(m_task.range.enabled)
            .arg(m_task.range.startMs)
            .arg(m_task.range.endMs));
    // 下一事件再加载，让入口先绑定新窗口的视频表面，避免重建已加载的播放图。
    m_state = PlaybackState{};
    m_generation = 0;
    m_open = true;
    const quint64 token = ++m_sessionToken;
    QTimer::singleShot(0, this, [this, token] {
        // 关闭或重新打开使旧的待加载请求失效，不允许旧输入进入新窗口。
        if (!m_open || token != m_sessionToken) {
            return;
        }
        m_generation = m_service->load(m_task.input);
        m_state = m_service->state();
        emit changed();
    });
    error->clear();
    emit changed();
    return true;
}

void PreviewController::close()
{
    // 先失效会话和计时器，再释放媒体，清理信号不能操作下一次会话。
    if (!m_open) {
        return;
    }
    m_open = false;
    ++m_sessionToken;
    m_generation = 0;
    cancelSelected();
    m_service->unload();
    m_state = PlaybackState{};
    m_conversion->endPreview();
}

ConversionTask PreviewController::task() const { return m_task; }
PlaybackState PreviewController::state() const { return m_state; }
bool PreviewController::editable() const { return m_open && m_task.state == TaskState::Waiting; }

QString PreviewController::rangeError() const
{
    // 终态仅看输入，不能利用预览重新加入批次。
    if (!editable()) {
        return QStringLiteral("此项仅供预览；重试请移除后重新添加。");
    }
    // 后端错误优先展示，不能保存新的片段或静默清除已有范围。
    if (m_state.failed) {
        return m_state.error;
    }
    // 未加载状态等待异步通知；未知输入时长不构造虚假的终点。
    if (!m_state.loaded) {
        return QStringLiteral("正在加载视频，请稍候……");
    }
    // 输入或播放后端缺失时长时仍保留可用播放，禁止构造片段。
    if (!std::isfinite(m_task.media.duration) || m_task.media.duration <= 0 ||
        m_state.durationMs <= 0) {
        return QStringLiteral("时长未知，只能使用可用播放能力与整段转换。");
    }
    // 无定位能力或显著时长差异时禁用片段，避免后端时间和转换时间错位。
    if (!m_state.seekable) {
        return QStringLiteral("当前视频不能定位，无法编辑片段；仍可整段转换。");
    }
    // 后端与探测时间轴显著不一致时拒绝保存，避免默默截错内容。
    if (std::abs(m_state.durationMs - std::llround(m_task.media.duration * 1000)) > 500) {
        return QStringLiteral("播放器与探测时长相差超过 0.5 秒，无法可靠编辑片段。");
    }
    // 当前等待项具备可靠时长和定位能力，可编辑草稿。
    return QString();
}

bool PreviewController::save(const MediaRange &range, QString *error)
{
    // 所有保存都要求当前有效播放能力，失败保留原选区。
    *error = rangeError();
    if (!error->isEmpty()) {
        return false;
    }
    // 队列控制器再次验证任务资格和时间范围，原子提交值副本。
    return m_conversion->saveRange(m_task.id, range, error);
}

void PreviewController::cancelSelected()
{
    // 草稿编辑或普通定位终止选区播放规则，避免旧终点意外暂停。
    m_seeking = false;
    m_selected = false;
    m_seekTimer.stop();
}
void PreviewController::togglePlayback()
{
    // 普通播放不受选区终点限制，暂停也结束待确认定位。
    cancelSelected();
    if (m_open && m_state.loaded && !m_state.failed) {
        // 按实际播放状态切换；不对加载失败的媒体提交播放请求。
        if (m_state.playing) {
            m_service->pause();
        } else {
            m_service->play();
        }
    }
}
void PreviewController::seek(qint64 milliseconds)
{
    // 手动拖动定位取消片段边界，位置限制在后端报告的媒体范围内。
    cancelSelected();
    if (m_open && m_state.seekable && !m_state.failed) {
        m_service->seek(qBound<qint64>(0, milliseconds, m_state.durationMs));
    }
}
void PreviewController::setVolume(int percent) { m_service->setVolume(percent); }

bool PreviewController::playSelected(const MediaRange &range, QString *error)
{
    // 播放草稿先校验能力与边界，不改变队列保存值。
    *error = rangeError();
    if (!error->isEmpty() || !range.enabled || !range.valid(m_task.media.duration)) {
        // 时间错误提供独立原因，后端错误保留原诊断。
        if (error->isEmpty()) {
            *error = QStringLiteral("请选择有效的起止时间后播放片段。");
        }
        return false;
    }
    // 暂停后异步确认定位，定时器防止不回报定位的后端永久等待。
    cancelSelected();
    m_service->pause();
    m_playRange = range;
    m_seeking = true;
    m_seekTimer.start();
    m_service->seek(range.startMs);
    onPlayback(m_generation, m_service->state());
    return true;
}

void PreviewController::onPlayback(quint64 generation, const PlaybackState &state)
{
    // 旧播放器已断开并销毁，业务层仍拒绝关闭期间和代次不符的回调。
    if (!m_open || generation != m_generation) {
        return;
    }
    m_state = state;
    // 后端失败立即清除待定位与片段规则，保存资格保持禁用。
    if (state.failed) {
        cancelSelected();
    } else if (m_seeking && std::abs(state.positionMs - m_playRange.startMs) <= 100) {
        // 起点位置确认后才开始播放，防止从定位前的旧位置播放。
        m_seeking = false;
        m_selected = true;
        m_seekTimer.stop();
        m_service->play();
    } else if (m_selected && state.positionMs >= m_playRange.endMs) {
        // 先撤销规则再暂停，暂停引发的同步通知不会重复执行边界处理。
        cancelSelected();
        m_service->pause();
    }
    // 对话框只观察当前会话状态，绘制与按钮锁定在 GUI 线程完成。
    emit changed();
}
}
