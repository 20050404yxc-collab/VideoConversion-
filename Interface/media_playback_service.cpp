// 文件：media_playback_service.cpp
// 层：Interface
// 作用：通过 libVLC C API 管理独立播放会话。
// 说明：动态加载 VLC DLL，避免将 MSVC 导入库带入 Qt/MinGW 工程。
#include "media_playback_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QWidget>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <functional>
#include <windows.h>

namespace videoconversion {
using Instance = void;
using Media = void;
using Player = void;
using Time = std::int64_t;

enum class PlayerState : int
{
    NothingSpecial = 0,
    Opening = 1,
    Buffering = 2,
    Playing = 3,
    Paused = 4,
    Stopped = 5,
    Ended = 6,
    Error = 7
};
struct LibVlcApi
{
    using NewInstance = Instance *(__cdecl *)(int, const char *const *);
    using ReleaseInstance = void(__cdecl *)(Instance *);
    using NewMedia = Media *(__cdecl *)(Instance *, const char *);
    using ReleaseMedia = void(__cdecl *)(Media *);
    using NewPlayer = Player *(__cdecl *)(Media *);
    using ReleasePlayer = void(__cdecl *)(Player *);
    using SetWindow = void(__cdecl *)(Player *, HWND);
    using Play = int(__cdecl *)(Player *);
    using Pause = void(__cdecl *)(Player *, int);
    using Stop = void(__cdecl *)(Player *);
    using SetTime = void(__cdecl *)(Player *, Time);
    using GetTime = Time(__cdecl *)(Player *);
    using GetLength = Time(__cdecl *)(Player *);
    using IsSeekable = int(__cdecl *)(Player *);
    using GetState = PlayerState(__cdecl *)(Player *);
    using SetVolume = int(__cdecl *)(Player *, int);
    using ErrorMessage = const char *(__cdecl *)();
    using ClearError = void(__cdecl *)();
    using Version = const char *(__cdecl *)();
    using LogCallback = void(__cdecl *)(void *, int, const void *, const char *, va_list);
    using SetLog = void(__cdecl *)(Instance *, LogCallback, void *);
    using UnsetLog = void(__cdecl *)(Instance *);

    HMODULE core = nullptr;
    HMODULE library = nullptr;
    NewInstance newInstance = nullptr;
    ReleaseInstance releaseInstance = nullptr;
    NewMedia newMedia = nullptr;
    ReleaseMedia releaseMedia = nullptr;
    NewPlayer newPlayer = nullptr;
    ReleasePlayer releasePlayer = nullptr;
    SetWindow setWindow = nullptr;
    Play play = nullptr;
    Pause pause = nullptr;
    Stop stop = nullptr;
    SetTime setTime = nullptr;
    GetTime getTime = nullptr;
    GetLength getLength = nullptr;
    IsSeekable isSeekable = nullptr;
    GetState getState = nullptr;
    SetVolume setVolume = nullptr;
    ErrorMessage errorMessage = nullptr;
    ClearError clearError = nullptr;
    Version version = nullptr;
    SetLog setLog = nullptr;
    UnsetLog unsetLog = nullptr;
    std::atomic<int> logCount{0};
    std::function<void(const QString &)> logSink;
    QString directory;
    QString error;

    ~LibVlcApi()
    {
        // 先释放依赖 libvlccore 的入口库，再卸载核心库。
        if (library) {
            FreeLibrary(library);
        }
        // 核心库可在入口库加载失败时单独存在，仍必须释放。
        if (core) {
            FreeLibrary(core);
        }
    }

    template <typename Function>
    bool resolve(Function *target, const char *name)
    {
        // 获取 VLC 3 C ABI 的入口，缺失符号时不允许继续使用不完整的接口表。
        *target = reinterpret_cast<Function>(GetProcAddress(library, name));
        if (!*target) {
            error = QStringLiteral("libVLC 缺少导出函数：%1").arg(QString::fromLatin1(name));
            return false;
        }
        // 符号有效后保留函数指针，供服务串行调用。
        return true;
    }

    bool load()
    {
        // 优先使用程序旁的独立部署，缺失时才使用显式开发路径。
        directory = QCoreApplication::applicationDirPath();
        if (!QFileInfo::exists(QDir(directory).filePath(QStringLiteral("libvlc.dll")))) {
            directory = qEnvironmentVariable("VIDEOCONVERSION_VLC");
        }
        // 空开发路径不回落到当前工作目录，明确提示用户缺少运行时。
        if (directory.isEmpty()) {
            error = QStringLiteral(
                "未找到 libVLC 运行时，请部署 libvlc.dll、libvlccore.dll 和 plugins");
            return false;
        }
        directory = QFileInfo(directory).absoluteFilePath();
        const QString corePath = QDir(directory).filePath(QStringLiteral("libvlccore.dll"));
        const QString libraryPath = QDir(directory).filePath(QStringLiteral("libvlc.dll"));
        // 缺失任一核心组件时报告目录，避免用系统安装掩盖发布包遗漏。
        if (!QFileInfo::exists(corePath) || !QFileInfo::exists(libraryPath) ||
            !QFileInfo::exists(QDir(directory).filePath(QStringLiteral("plugins")))) {
            error = QStringLiteral("未找到 libVLC 运行时或 plugins 目录：%1").arg(directory);
            return false;
        }
        // 使用绝对 DLL 路径寻找同目录依赖，不改进程全局 PATH。
        core = LoadLibraryExW(reinterpret_cast<LPCWSTR>(corePath.utf16()), nullptr,
                              LOAD_WITH_ALTERED_SEARCH_PATH);
        // 核心库无法加载时保留 Windows 错误，部分资源由析构统一释放。
        if (!core) {
            error =
                QStringLiteral("无法加载 libvlccore.dll（Windows 错误 %1）").arg(GetLastError());
            return false;
        }
        // 核心库成功后加载入口库；不完整入口库不会进入播放器创建。
        library = LoadLibraryExW(reinterpret_cast<LPCWSTR>(libraryPath.utf16()), nullptr,
                                 LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!library) {
            error = QStringLiteral("无法加载 libvlc.dll（Windows 错误 %1）").arg(GetLastError());
            return false;
        }
        // 所需导出全部有效才完成加载，绕开工具链专用导入库。
        return resolve(&newInstance, "libvlc_new") && resolve(&releaseInstance, "libvlc_release") &&
               resolve(&newMedia, "libvlc_media_new_path") &&
               resolve(&releaseMedia, "libvlc_media_release") &&
               resolve(&newPlayer, "libvlc_media_player_new_from_media") &&
               resolve(&releasePlayer, "libvlc_media_player_release") &&
               resolve(&setWindow, "libvlc_media_player_set_hwnd") &&
               resolve(&play, "libvlc_media_player_play") &&
               resolve(&pause, "libvlc_media_player_set_pause") &&
               resolve(&stop, "libvlc_media_player_stop") &&
               resolve(&setTime, "libvlc_media_player_set_time") &&
               resolve(&getTime, "libvlc_media_player_get_time") &&
               resolve(&getLength, "libvlc_media_player_get_length") &&
               resolve(&isSeekable, "libvlc_media_player_is_seekable") &&
               resolve(&getState, "libvlc_media_player_get_state") &&
               resolve(&setVolume, "libvlc_audio_set_volume") &&
               resolve(&errorMessage, "libvlc_errmsg") && resolve(&clearError, "libvlc_clearerr") &&
               resolve(&version, "libvlc_get_version") && resolve(&setLog, "libvlc_log_set") &&
               resolve(&unsetLog, "libvlc_log_unset");
    }

    // 解码线程只格式化有限数量的警告并转交排队函数，不访问窗口或业务状态。
    static void __cdecl receiveLog(void *context, int level, const void *, const char *format,
                                   va_list arguments)
    {
        // 忽略调试信息，避免正常播放产生大量底层日志。
        if (level < 3) {
            return;
        }
        auto *api = static_cast<LibVlcApi *>(context);
        const int index = api->logCount.fetch_add(1);
        // 第 101 条只发出截断说明，此后不再向 Qt 队列追加消息。
        if (index > 100) {
            return;
        }
        const QString message = index == 100
                                    ? QStringLiteral("libVLC 后端消息超过 100 条，后续省略")
                                    : QStringLiteral("libVLC 后端 level=%1：%2")
                                          .arg(level)
                                          .arg(QString::vasprintf(format, arguments).left(2048));
        api->logSink(message);
    }

    QString lastError()
    {
        // 错误接口是线程局部信息，复制后清除，避免把旧错误当成当前故障。
        const char *message = errorMessage ? errorMessage() : nullptr;
        const QString result = message ? QString::fromUtf8(message) : QString();
        // 接口可用时清除旧状态；接口尚未加载不调用空指针。
        if (clearError) {
            clearError();
        }
        // 异步解码故障未必设置当前线程的错误，空值如实说明而不推测解码器原因。
        return result.isEmpty() ? QStringLiteral("libVLC 未提供错误详情") : result;
    }
};

// 将 VLC 状态转为稳定可读的日志文本。
QString playerStateName(PlayerState state)
{
    // 使用固定状态映射，未识别的值按尚未开始记录。
    switch (state) {
    case PlayerState::Opening:
        return QStringLiteral("Opening");
    case PlayerState::Buffering:
        return QStringLiteral("Buffering");
    case PlayerState::Playing:
        return QStringLiteral("Playing");
    case PlayerState::Paused:
        return QStringLiteral("Paused");
    case PlayerState::Stopped:
        return QStringLiteral("Stopped");
    case PlayerState::Ended:
        return QStringLiteral("Ended");
    case PlayerState::Error:
        return QStringLiteral("Error");
    default:
        return QStringLiteral("NothingSpecial");
    }
}
MediaPlaybackService::MediaPlaybackService(QObject *parent) : QObject(parent)
{
    // 播放状态由 GUI 线程轮询，解码线程只向 GUI 排队有界日志。
    m_pollTimer.setInterval(100);
    connect(&m_pollTimer, &QTimer::timeout, this, &MediaPlaybackService::publish);
    m_loadTimer.setSingleShot(true);
    m_loadTimer.setInterval(10000);
    connect(&m_loadTimer, &QTimer::timeout, this, [this] {
        // 活动播放器超过十秒仍未加载成功时固定失败并停止轮询。
        if (m_player && !m_state.loaded && !m_state.failed) {
            fail(QStringLiteral("libVLC 加载视频超时。"));
        }
    });
}

MediaPlaybackService::~MediaPlaybackService()
{
    // 先停媒体与日志回调，再释放仍由服务持有的 VLC 实例。
    unload();
    // 实例只在创建成功后存在，释放后才能卸载入口库和核心库。
    if (m_instance && m_api) {
        m_api->releaseInstance(static_cast<Instance *>(m_instance));
        m_instance = nullptr;
    }
}

quint64 MediaPlaybackService::load(const QString &path)
{
    // 新代次重建媒体与播放器，旧会话的排队消息无法影响新文件。
    unload();
    ++m_generation;
    m_input = path;
    m_lastDiagnosticState.clear();
    m_state = PlaybackState{};
    // 运行时加载或版本不符时保持失败状态，不再创建本次媒体。
    if (!initializeRuntime()) {
        return m_generation;
    }
    const QFileInfo file(path);
    logDiagnostic(QStringLiteral("后端=libVLC；version=%1；Qt=%2")
                      .arg(QString::fromUtf8(m_api->version()), QString::fromLatin1(qVersion())));
    logDiagnostic(
        QStringLiteral("打开：exists=%1；readable=%2；bytes=%3；suffix=%4；后端=libVLC；runtime=%5")
            .arg(file.exists())
            .arg(file.isReadable())
            .arg(file.size())
            .arg(file.suffix(), m_api->directory));
    // 文件在探测后可能被删除或改为目录；明确拒绝，不等待后端不一致的终态。
    if (!file.exists() || !file.isFile() || !file.isReadable()) {
        fail(QStringLiteral("输入文件不存在或不可读"));
        return m_generation;
    }
    // 媒体路径使用 VLC 要求的 UTF-8，播放器创建后持有自己的媒体引用。
    const QByteArray localPath = QDir::toNativeSeparators(file.absoluteFilePath()).toUtf8();
    m_media = m_api->newMedia(static_cast<Instance *>(m_instance), localPath.constData());
    // 创建媒体失败时回报后端原因，不启动轮询。
    if (!m_media) {
        fail(m_api->lastError());
        return m_generation;
    }
    m_player = m_api->newPlayer(static_cast<Media *>(m_media));
    m_api->releaseMedia(static_cast<Media *>(m_media));
    m_media = nullptr;
    // 创建播放器失败时媒体引用已释放，当前会话保持失败。
    if (!m_player) {
        fail(m_api->lastError());
        return m_generation;
    }
    // 启动成功才轮询能力；启动失败由辅助函数记录并保持可关闭的会话。
    startPlayer();
    return m_generation;
}

bool MediaPlaybackService::initializeRuntime()
{
    // 首次使用时加载 DLL，失败卸载部分库并向调用方报告。
    if (!m_api) {
        m_api.reset(new LibVlcApi);
        if (!m_api->load()) {
            const QString error = m_api->error;
            m_api.reset();
            fail(error);
            return false;
        }
    }
    // 适配器使用 VLC 3 的同步停止与定位 API，不能接入签名不同的 VLC 4。
    const QString version = QString::fromUtf8(m_api->version());
    if (!version.startsWith(QStringLiteral("3."))) {
        fail(QStringLiteral("需要 VLC 3.x 运行时，实际版本为 %1").arg(version));
        return false;
    }
    // 实例在服务生命周期内复用，媒体和播放器仍每次重建。
    if (!m_instance) {
        const QByteArray titleOption = QByteArrayLiteral("--no-video-title-show");
        const QByteArray osdOption = QByteArrayLiteral("--no-osd");
        const QByteArray libraryOption = QByteArrayLiteral("--no-media-library");
        const QByteArray configOption = QByteArrayLiteral("--ignore-config");
        const char *arguments[] = {configOption.constData(), titleOption.constData(),
                                   osdOption.constData(), libraryOption.constData()};
        m_instance = m_api->newInstance(4, arguments);
        // 无可用插件或实例创建失败时报告原始原因。
        if (!m_instance) {
            fail(m_api->lastError());
            return false;
        }
    }
    // API 和实例均有效，可继续创建当前媒体。
    return true;
}

bool MediaPlaybackService::startPlayer()
{
    // 日志注册先于播放；回调对象在 log_unset 等待结束前始终有效。
    const quint64 generation = m_generation;
    m_api->logCount.store(0);
    m_api->logSink = [this, generation](const QString &message) {
        QMetaObject::invokeMethod(
            this,
            [this, generation, message] {
                // 关闭或切换后的排队日志不能串入新会话。
                if (m_player && generation == m_generation) {
                    logDiagnostic(message);
                }
            },
            Qt::QueuedConnection);
    };
    m_api->setLog(static_cast<Instance *>(m_instance), &LibVlcApi::receiveLog, m_api.get());
    m_api->setVolume(static_cast<Player *>(m_player), 0);
    attachOutput();
    // 播放启动失败时停止后端并回报，保留资源直到关闭统一释放。
    if (m_api->play(static_cast<Player *>(m_player)) != 0) {
        fail(m_api->lastError());
        return false;
    }
    // 静音启动解码以取得时长和定位能力；首次进入播放后暂停，成功才发布能力。
    m_pauseAfterStart = true;
    m_loadTimer.start();
    m_pollTimer.start();
    publish();
    return true;
}

void MediaPlaybackService::unload()
{
    // 先停计时器和日志，再停止解码并解绑窗口，最后释放播放器。
    m_loadTimer.stop();
    m_pollTimer.stop();
    m_pauseAfterStart = false;
    m_pendingSeekMs = -1;
    // 即使文件预检失败、尚未创建播放器，关闭也在日志中明确结束本会话。
    if (!m_input.isEmpty()) {
        logDiagnostic(QStringLiteral("关闭并释放媒体；position=%1ms").arg(m_state.positionMs));
        m_input.clear();
    }
    // VLC 保证取消日志时等待正在执行的回调结束，再允许释放回调上下文。
    if (m_instance && m_api) {
        m_api->unsetLog(static_cast<Instance *>(m_instance));
        m_api->logSink = {};
    }
    // 活动播放器同步停止后才解绑 HWND，关闭后不会继续访问已销毁的视频窗口。
    if (m_player && m_api) {
        m_api->stop(static_cast<Player *>(m_player));
        m_api->setWindow(static_cast<Player *>(m_player), nullptr);
        m_api->releasePlayer(static_cast<Player *>(m_player));
        m_player = nullptr;
    }
    // 媒体创建后的异常路径也释放尚未转交播放器的引用。
    if (m_media && m_api) {
        m_api->releaseMedia(static_cast<Media *>(m_media));
        m_media = nullptr;
    }
    // 会话能力回到空状态，API 与实例仍可供下一次加载复用。
    m_state = PlaybackState{};
}

void MediaPlaybackService::publish()
{
    // 空会话或已固定失败的会话不再访问后端；调用与界面操作串行。
    if (!m_player || !m_api || m_state.failed) {
        return;
    }
    const auto player = static_cast<Player *>(m_player);
    const PlayerState backendState = m_api->getState(player);
    const qint64 time = m_api->getTime(player);
    const qint64 length = m_api->getLength(player);
    // VLC 到达末尾后可能返回 -1；保留已知时长并显示末尾，而不是跳回零。
    if (length > 0) {
        m_state.durationMs = length;
    }
    m_state.positionMs =
        backendState == PlayerState::Ended ? m_state.durationMs : qMax<qint64>(0, time);
    // Ended 时原生输入已经关闭，但本地媒体重播后仍可定位，保留之前的能力。
    if (backendState != PlayerState::Ended) {
        m_state.seekable = m_api->isSeekable(player) != 0;
    }
    m_state.playing = backendState == PlayerState::Playing;
    // 后端故障固定失败并停止轮询，不继续发布正常状态。
    if (backendState == PlayerState::Error) {
        fail(m_api->lastError());
        return;
    }
    // 首次进入可用状态才解除加载超时，并公布可操作的能力。
    if (backendState == PlayerState::Playing || backendState == PlayerState::Paused ||
        backendState == PlayerState::Ended) {
        m_state.loaded = true;
        m_loadTimer.stop();
        // 重启后先提交待定位目标，即使用户已主动恢复播放也不丢弃该请求。
        if (backendState == PlayerState::Playing && m_pendingSeekMs >= 0) {
            m_api->setTime(player, m_pendingSeekMs);
            m_pendingSeekMs = -1;
        }
        // 自动预热仅执行一次，先暂停再恢复用户音量，避免加载时发出声音。
        if (m_pauseAfterStart && backendState == PlayerState::Playing) {
            m_api->pause(player, 1);
            m_api->setVolume(player, m_volume);
            m_pauseAfterStart = false;
            m_state.playing = false;
        }
    }
    // 状态变化记日志，位置通知每次轮询都发出以驱动滑块和片段终点。
    logState(static_cast<int>(backendState));
    emit updated(m_generation, m_state);
}

void MediaPlaybackService::play()
{
    // 有效会话恢复用户音量并播放，主动播放取消静音预热暂停。
    if (m_player && m_api && !m_state.failed) {
        m_pauseAfterStart = false;
        m_api->setVolume(static_cast<Player *>(m_player), m_volume);
        // VLC 3 的 Ended 输入线程仍可能存在，先 stop 清除它才会重新创建输入。
        if (m_api->getState(static_cast<Player *>(m_player)) == PlayerState::Ended) {
            m_api->stop(static_cast<Player *>(m_player));
        }
        m_api->play(static_cast<Player *>(m_player));
    }
}

void MediaPlaybackService::pause()
{
    // 已创建播放器才提交暂停，空会话不访问原生接口。
    if (m_player && m_api) {
        m_api->pause(static_cast<Player *>(m_player), 1);
    }
}

void MediaPlaybackService::seek(qint64 milliseconds)
{
    // 后端允许定位时才提交请求，合法范围由控制层判断。
    if (m_player && m_api && m_state.seekable && !m_state.failed) {
        // 到末尾后的播放器没有活动输入，先静音重启，再由轮询提交待定位位置。
        if (m_api->getState(static_cast<Player *>(m_player)) == PlayerState::Ended) {
            m_api->stop(static_cast<Player *>(m_player));
            m_pendingSeekMs = milliseconds;
            m_pauseAfterStart = true;
            m_api->setVolume(static_cast<Player *>(m_player), 0);
            // 重启失败时固定错误，不能假装已定位成功。
            if (m_api->play(static_cast<Player *>(m_player)) != 0) {
                fail(m_api->lastError());
            }
            return;
        }
        m_api->setTime(static_cast<Player *>(m_player), milliseconds);
        publish();
    }
}

void MediaPlaybackService::setVolume(int percent)
{
    // 音量保存到服务状态，加载结束或暂停恢复不会覆盖用户选择。
    m_volume = qBound(0, percent, 100);
    // 首帧预热阶段保持静音，普通播放立即应用音量。
    if (m_player && m_api && !m_pauseAfterStart) {
        m_api->setVolume(static_cast<Player *>(m_player), m_volume);
    }
}

PlaybackState MediaPlaybackService::state() const { return m_state; }

void MediaPlaybackService::setVideoOutput(QWidget *output)
{
    // 窗口仅由 QPointer 观察，实际所有权仍归预览对话框。
    m_output = output;
    // 有窗口时先取得稳定的原生 HWND，再把显示目标交给 VLC。
    if (m_output) {
        m_output->setAttribute(Qt::WA_NativeWindow);
        m_output->winId();
    }
    // 已创建的播放器立即同步绑定或解绑；未加载时留到创建后处理。
    attachOutput();
}

void MediaPlaybackService::attachOutput()
{
    // 原生播放器存在时更新显示目标，入口通常在加载前完成绑定。
    if (m_player && m_api) {
        const HWND window = m_output ? reinterpret_cast<HWND>(m_output->winId()) : nullptr;
        m_api->setWindow(static_cast<Player *>(m_player), window);
    }
}

void MediaPlaybackService::fail(const QString &reason)
{
    // 当前会话固定为失败，停止加载与轮询，禁用播放和保存新范围。
    m_loadTimer.stop();
    m_pollTimer.stop();
    m_state.failed = true;
    m_state.loaded = false;
    m_state.playing = false;
    m_state.error = QStringLiteral("libVLC 无法预览此视频：%1。可取消预览并继续整段转换。")
                        .arg(reason.isEmpty() ? QStringLiteral("未提供错误详情") : reason);
    // 原生播放器存在时停止解码，资源留给 unload 统一释放。
    if (m_player && m_api) {
        m_api->stop(static_cast<Player *>(m_player));
    }
    // 失败原因进入现有日志并通知当前代次的控制器。
    logDiagnostic(QStringLiteral("失败：%1").arg(m_state.error));
    emit updated(m_generation, m_state);
}

void MediaPlaybackService::logDiagnostic(const QString &text)
{
    // 每条消息带输入路径和会话代次，主日志继续负责全局长度限制。
    emit diagnostic(QStringLiteral("[预览][会话 %1][%2] %3").arg(m_generation).arg(m_input, text));
}

void MediaPlaybackService::logState(int state)
{
    // 快照不包含不断变化的位置，避免轮询每一帧都产生相同状态日志。
    const QString summary = QStringLiteral("后端=libVLC；状态=%1；duration=%2ms；")
                                .arg(playerStateName(static_cast<PlayerState>(state)))
                                .arg(m_state.durationMs) +
                            QStringLiteral("seekable=%1；loaded=%2；playing=%3")
                                .arg(m_state.seekable)
                                .arg(m_state.loaded)
                                .arg(m_state.playing);
    // 只有状态或能力变化才记录，正常位置通知仍按轮询发出。
    if (summary != m_lastDiagnosticState) {
        m_lastDiagnosticState = summary;
        logDiagnostic(summary);
    }
}
}
