// 文件：conversion_tests.cpp
// 层：tests
// 作用：验证参数、文件事务与真实异步队列生命周期。
// 说明：测试中的同步工具等待只运行于测试进程，不用于产品事件线程。
#include <cmath>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "conversion_controller.h"
#include "ffmpeg_arguments.h"
#include "output_file_manager.h"
#include "progress_parser.h"
#include "settings_store.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace videoconversion {
namespace test_support {
// 同步工具用于生成测试断言数据；失败不能当作探测成功。
bool inspect(const QString &path, MediaInfo *media, QString *error)
{
    // 独立测试进程等待有时限，不阻塞产品进程。
    QProcess probe;
    probe.start(
        QDir(qEnvironmentVariable("VIDEOCONVERSION_TOOLS")).filePath(QStringLiteral("ffprobe.exe")),
        probeArguments(path));
    if (!probe.waitForFinished(20000) || probe.exitCode() != 0) {
        *error = probe.errorString() + QString::fromLocal8Bit(probe.readAllStandardError());
        return false;
    }
    return parseMediaInfo(probe.readAllStandardOutput(), media, error);
}

// 全部媒体帧可解码是输出有效性的补充证据，不代表人工播放器验收。
bool decode(const QString &path)
{
    // 将真实结果解码到空输出，捕捉探测成功却帧损坏的回归。
    QProcess ffmpeg;
    ffmpeg.start(
        QDir(qEnvironmentVariable("VIDEOCONVERSION_TOOLS")).filePath(QStringLiteral("ffmpeg.exe")),
        {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-i"), path,
         QStringLiteral("-f"), QStringLiteral("null"), QStringLiteral("-")});
    return ffmpeg.waitForFinished(30000) && ffmpeg.exitCode() == 0 &&
           ffmpeg.readAllStandardError().isEmpty();
}

// 将可重现样例复制到隔离目录，用于中文路径、重复与文件保护测试。
QString copyFixture(const QString &directory, const QString &fixture, const QString &name)
{
    // 路径由环境指定，无法复制时让调用方断言可见地失败。
    const QString path = QDir(directory).filePath(name);
    const QString source = QDir(qEnvironmentVariable("VIDEOCONVERSION_FIXTURES")).filePath(fixture);
    return QFile::copy(source, path) ? path : QString();
}
}

class ConversionTests : public QObject
{
    Q_OBJECT
   private slots:
    // 测试必须使用已准备的真实工具与素材，不允许静默跳过集成用例。
    void initTestCase();
    // 跨任意分块只在完整记录报告时间，微秒字段不得放大一千倍。
    void fragmentedProgress();
    // 缺失时长、封面流与 HDR 都有明确策略，不能误选封面转换。
    void metadataBoundaries();
    // 输出边界保留方向与比例，低分辨率不得放大。
    void scalingBoundaries();
    // 重名分配与提交不修改源文件或已有结果，失败清理只影响临时文件。
    void safeFileTransactions();
    // 无效持久设置回退，正确偏好往返且不保存任务。
    void preferencesValidation();
    // 枚举矩阵覆盖两种容器、三档画质与三档分辨率。
    void outputMatrix_data();
    // 真实转换结果通过探测和全部帧解码，终态不得重复开始。
    void outputMatrix();
    // 真实竖屏、旋转、奇数尺寸与非方像素输入符合输出约束。
    void mediaEdges_data();
    void mediaEdges();
    // 导入中跳过重复/目录，错误媒体不阻断其他有效任务。
    void importAndFailureContinuation();
    // 已探测输入随后丢失仅失败该项，后续任务仍成功。
    void missingInputContinues();
    // 转换取消清理真实临时文件并继续下一项，过期完成不得复活任务。
    void cancelCurrentContinues();
    // 输出验证可取消，不留下已经取消任务的正式结果。
    void cancelVerification();
    // 停止全部禁止调度待转换项，重复按钮触发也只产生一个汇总。
    void stopAllSuppressesScheduling();
    // 关闭请求异步停止正在探测或转换的进程，确认完成后无活动工作。
    void shutdown_data();
    void shutdown();
    // 工具缺失、无法启动和真实探测超时均产生一次失败并释放操作槽。
    void toolFailuresAndTimeout();
    // 不可写输出位置应明确失败而不是误报转换成功。
    void invalidOutputDirectory();
    // 外部占用导致临时清理失败时披露残留，但不能阻断后续有效视频。
    void cleanupResidueContinues();
    // 失败验证不允许提交编码、时长或容器错误的结果。
    void outputValidationRejectsMismatch();
};

void ConversionTests::initTestCase()
{
    // 注册信号值对象，测试监听不会因未知类型丢失结果。
    qRegisterMetaType<BatchSummary>();
    qRegisterMetaType<ProcessResult>();
    QVERIFY(QFileInfo::exists(QDir(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"))
                                  .filePath(QStringLiteral("ffmpeg.exe"))));
    QVERIFY(QFileInfo::exists(QDir(qEnvironmentVariable("VIDEOCONVERSION_FIXTURES"))
                                  .filePath(QStringLiteral("中文 sample.mp4"))));
}

void ConversionTests::fragmentedProgress()
{
    // 每字节输入与 CRLF 共同覆盖行和记录分块。
    ProgressParser parser;
    QVector<double> records;
    const QByteArray input(
        "frame=1\r\nout_time_ms=1234000\r\nprogress=continue\r\nout_time_us=2000000\nprogress="
        "end\n");
    for (char byte : input) {
        records += parser.feed(QByteArray(1, byte));
    }
    QCOMPARE(records.size(), 2);
    QCOMPARE(records.at(0), 1.234);
    QCOMPARE(records.at(1), 2.0);
    // 半行不产生记录，重置必须删除上一任务的残片。
    QVERIFY(parser.feed("out_time_us=99").isEmpty());
    parser.reset();
    QVERIFY(parser.feed("progress=end\n").isEmpty());
    QVERIFY(parser.feed("out_time_us=nan\nprogress=end\n").isEmpty());
    QCOMPARE(parser.feed("out_time=00:01:02.500\nprogress=continue\n"), QVector<double>{62.5});
}

void ConversionTests::metadataBoundaries()
{
    // 封面流应跳过，缺失时长与音轨保持未知/无音轨语义。
    const QByteArray json = R"({"streams":[
      {"index":0,"codec_type":"video","width":200,"height":200,"disposition":{"attached_pic":1}},
      {"index":2,"codec_type":"video","codec_name":"h264","width":320,"height":180,"sample_aspect_ratio":"1:1"}],
      "format":{"format_name":"mov,mp4","duration":"N/A"}})";
    MediaInfo info;
    QString error;
    QVERIFY2(parseMediaInfo(json, &info, &error), qPrintable(error));
    QCOMPARE(info.videoIndex, 2);
    QCOMPARE(info.audioIndex, -1);
    QCOMPARE(info.duration, -1.0);
    QVERIFY(!parseMediaInfo("broken json", &info, &error));
    QVERIFY(!parseMediaInfo("{\"streams\":[]}", &info, &error));
    // HDR 和错误比例拒绝转换，不伪造 SDR 或方像素保证。
    QByteArray hdr = json;
    hdr.replace("\"codec_name\":\"h264\"",
                "\"codec_name\":\"h264\",\"color_transfer\":\"smpte2084\"");
    QVERIFY(!parseMediaInfo(hdr, &info, &error));
    QVERIFY(error.contains(QStringLiteral("HDR")));
    QByteArray invalidAspect = json;
    invalidAspect.replace("1:1", "bad:0");
    QVERIFY(!parseMediaInfo(invalidAspect, &info, &error));
}

void ConversionTests::scalingBoundaries()
{
    // 横屏、竖屏分别约束长短边，低分辨率只做偶数对齐。
    MediaInfo media;
    media.displaySize = QSize(3840, 2160);
    QCOMPARE(outputSize(media, Resolution::FullHd), QSize(1920, 1080));
    QCOMPARE(outputSize(media, Resolution::Hd), QSize(1280, 720));
    media.displaySize = QSize(2160, 3840);
    QCOMPARE(outputSize(media, Resolution::FullHd), QSize(1080, 1920));
    QCOMPARE(outputSize(media, Resolution::Hd), QSize(720, 1280));
    media.displaySize = QSize(321, 181);
    QCOMPARE(outputSize(media, Resolution::FullHd), QSize(320, 180));
    // 无音轨参数不强加音频；含空格路径作为独立参数保持原值。
    const QStringList arguments = conversionArguments(
        QStringLiteral("中文 a.mp4"), QStringLiteral("temp.mkv"), media, EncodingOptions{});
    QVERIFY(arguments.contains(QStringLiteral("-an")));
    QVERIFY(arguments.contains(QStringLiteral("中文 a.mp4")));
    QVERIFY(arguments.contains(QStringLiteral("-n")));
}

void ConversionTests::safeFileTransactions()
{
    // 已有结果作为哨兵，两个事务准备同一源名并分别提交。
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString source = test_support::copyFixture(
        directory.path(), QStringLiteral("中文 sample.mp4"), QStringLiteral("视频.mp4"));
    const QString existing = QDir(directory.path()).filePath(QStringLiteral("视频_converted.mp4"));
    QFile sentinel(existing);
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    QCOMPARE(sentinel.write("keep"), qint64(4));
    sentinel.close();
    OutputFileManager first;
    OutputFileManager second;
    QString error;
    QVERIFY(first.prepare(source, directory.path(), Container::Mp4, &error));
    QVERIFY(second.prepare(source, directory.path(), Container::Mp4, &error));
    // 两个临时路径必须独立，提交阶段出现的新结果也不能覆盖。
    QVERIFY(first.temporaryPath() != second.temporaryPath());
    QVERIFY(QFile::copy(source, first.temporaryPath()));
    QVERIFY(QFile::copy(source, second.temporaryPath()));
    QString firstResult;
    QString secondResult;
    QVERIFY(first.commit(&firstResult, &error));
    QVERIFY(second.commit(&secondResult, &error));
    QVERIFY(firstResult.endsWith(QStringLiteral("_1.mp4")));
    QVERIFY(secondResult.endsWith(QStringLiteral("_2.mp4")));
    QVERIFY(first.cleanup(&error));
    QVERIFY(QFileInfo::exists(firstResult));
    QVERIFY(sentinel.open(QIODevice::ReadOnly));
    QCOMPARE(sentinel.readAll(), QByteArray("keep"));
    QVERIFY(QFileInfo::exists(source));
    // 失败事务清理后只移除拥有的临时路径，保留正式结果。
    QVERIFY(first.prepare(source, directory.path(), Container::Mp4, &error));
    const QString temporary = first.temporaryPath();
    QVERIFY(QFile::copy(source, temporary));
    QVERIFY(first.cleanup(&error));
    QVERIFY(!QFileInfo::exists(temporary));
    QVERIFY(QFileInfo::exists(firstResult));
}

void ConversionTests::preferencesValidation()
{
    // 隔离 INI 注入坏枚举与空目录，恢复默认且提供警告。
    QTemporaryDir directory;
    const QString ini = QDir(directory.path()).filePath(QStringLiteral("settings.ini"));
    QSettings corrupt(ini, QSettings::IniFormat);
    corrupt.setValue(QStringLiteral("container"), 99);
    corrupt.setValue(QStringLiteral("quality"), QStringLiteral("invalid"));
    corrupt.setValue(QStringLiteral("resolution"), -1);
    corrupt.setValue(QStringLiteral("outputDirectory"), QString());
    corrupt.sync();
    SettingsStore store(ini);
    QString warnings;
    ConversionSettings settings = store.load(&warnings);
    QCOMPARE(settings.encoding.container, Container::Mp4);
    QCOMPARE(settings.encoding.quality, Quality::Standard);
    QCOMPARE(settings.encoding.resolution, Resolution::Original);
    QVERIFY(!settings.outputDirectory.isEmpty());
    QVERIFY(!warnings.isEmpty());
    // 正常偏好持久化后重新实例化读取，队列键不出现。
    settings.encoding.container = Container::Mkv;
    settings.encoding.quality = Quality::High;
    settings.encoding.resolution = Resolution::Hd;
    settings.outputDirectory = directory.path();
    QVERIFY(store.save(settings, &warnings));
    SettingsStore restarted(ini);
    const ConversionSettings restored = restarted.load(&warnings);
    QCOMPARE(restored.encoding.container, Container::Mkv);
    QCOMPARE(restored.encoding.quality, Quality::High);
    QCOMPARE(restored.encoding.resolution, Resolution::Hd);
    QCOMPARE(restored.outputDirectory, directory.path());
    QVERIFY(warnings.isEmpty());
    QCOMPARE(QSettings(ini, QSettings::IniFormat).allKeys().size(), 4);
}

void ConversionTests::outputMatrix_data()
{
    // 数据行穷举首版所有独立输出选项，而不是只覆盖默认值。
    QTest::addColumn<int>("container");
    QTest::addColumn<int>("quality");
    QTest::addColumn<int>("resolution");
    for (int container = 0; container < 2; ++container) {
        for (int quality = 0; quality < 3; ++quality) {
            for (int resolution = 0; resolution < 3; ++resolution) {
                const QByteArray name = QStringLiteral("c%1-q%2-r%3")
                                            .arg(container)
                                            .arg(quality)
                                            .arg(resolution)
                                            .toLatin1();
                QTest::newRow(name.constData()) << container << quality << resolution;
            }
        }
    }
}

void ConversionTests::outputMatrix()
{
    // 使用真实控制器执行参数组合，等待完整异步管线和文件提交。
    QFETCH(int, container);
    QFETCH(int, quality);
    QFETCH(int, resolution);
    QTemporaryDir directory;
    const QString input = test_support::copyFixture(
        directory.path(), QStringLiteral("中文 sample.mp4"), QStringLiteral("中文 空格.mp4"));
    QVERIFY(!input.isEmpty());
    const qint64 sourceSize = QFileInfo(input).size();
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("preferences.ini")));
    ConversionController controller(&service, &store);
    controller.initialize();
    controller.addFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
    QVERIFY(!controller.tasks().first().thumbnail.isNull());
    ConversionSettings settings;
    settings.outputDirectory = QDir(directory.path()).filePath(QStringLiteral("输出 结果"));
    settings.encoding.container = static_cast<Container>(container);
    settings.encoding.quality = static_cast<Quality>(quality);
    settings.encoding.resolution = static_cast<Resolution>(resolution);
    QSignalSpy finished(&controller, &ConversionController::batchFinished);
    QVERIFY(controller.startBatch(settings));
    QVERIFY(!controller.startBatch(settings));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
    const ConversionTask result = controller.tasks().first();
    QCOMPARE(result.state, TaskState::Succeeded);
    QVERIFY2(QFileInfo::exists(result.output), qPrintable(result.detail));
    QCOMPARE(result.percent, 100);
    QCOMPARE(QFileInfo(input).size(), sourceSize);
    MediaInfo output;
    QString error;
    QVERIFY2(test_support::inspect(result.output, &output, &error), qPrintable(error));
    QVERIFY2(validateOutput(result.media, output, settings.encoding, &error), qPrintable(error));
    QVERIFY(test_support::decode(result.output));
    QVERIFY(!controller.canStart());
    QCOMPARE(QDir(settings.outputDirectory)
                 .entryList({QStringLiteral(".vc-*")}, QDir::Files | QDir::Hidden)
                 .size(),
             0);
}

void ConversionTests::mediaEdges_data()
{
    // 单路静音竖屏、实际旋转、奇数尺寸和非方像素均使用真实编码素材。
    QTest::addColumn<QString>("fixture");
    QTest::newRow("portrait-silent") << QStringLiteral("portrait.mp4");
    QTest::newRow("rotation") << QStringLiteral("rotated.mp4");
    QTest::newRow("odd") << QStringLiteral("odd.mkv");
    QTest::newRow("sample-aspect") << QStringLiteral("anamorphic.mp4");
}

void ConversionTests::mediaEdges()
{
    // 强制 720p 上限，检查方向和音轨保留策略以及完整解码。
    QFETCH(QString, fixture);
    QTemporaryDir directory;
    const QString input = test_support::copyFixture(directory.path(), fixture, fixture);
    QVERIFY(!input.isEmpty());
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("settings.ini")));
    ConversionController controller(&service, &store);
    controller.initialize();
    controller.addFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
    ConversionSettings settings;
    settings.outputDirectory = directory.path();
    settings.encoding.resolution = Resolution::Hd;
    QVERIFY(controller.startBatch(settings));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.batchActive(), 30000);
    const ConversionTask result = controller.tasks().first();
    QVERIFY2(result.state == TaskState::Succeeded, qPrintable(result.detail));
    MediaInfo output;
    QString error;
    QVERIFY2(test_support::inspect(result.output, &output, &error), qPrintable(error));
    QCOMPARE(output.displaySize, outputSize(result.media, Resolution::Hd));
    QCOMPARE(output.audioIndex >= 0, result.media.audioIndex >= 0);
    QVERIFY(test_support::decode(result.output));
}

void ConversionTests::importAndFailureContinuation()
{
    // 重复文件和目录不新增条目，损坏和 HDR 视频单独失败。
    QTemporaryDir directory;
    const QString valid = test_support::copyFixture(
        directory.path(), QStringLiteral("中文 sample.mp4"), QStringLiteral("valid.mp4"));
    const QString damaged = test_support::copyFixture(
        directory.path(), QStringLiteral("damaged.mp4"), QStringLiteral("damaged.mp4"));
    const QString hdr = test_support::copyFixture(directory.path(), QStringLiteral("hdr.mp4"),
                                                  QStringLiteral("hdr.mp4"));
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("settings.ini")));
    ConversionController controller(&service, &store);
    QSignalSpy notices(&controller, &ConversionController::notice);
    controller.initialize();
    controller.addFiles({damaged, valid, valid, directory.path(), hdr});
    QCOMPARE(controller.tasks().size(), 3);
    QCOMPARE(notices.count(), 2);
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
    QCOMPARE(controller.tasks().at(0).state, TaskState::Failed);
    QCOMPARE(controller.tasks().at(1).state, TaskState::Waiting);
    QCOMPARE(controller.tasks().at(2).state, TaskState::Failed);
    QVERIFY(controller.tasks().at(2).detail.contains(QStringLiteral("HDR")));
}

void ConversionTests::missingInputContinues()
{
    // 探测后删掉中间输入，转换阶段失败不阻断后一项。
    QTemporaryDir directory;
    QStringList paths;
    for (int index = 0; index < 3; ++index) {
        paths.append(test_support::copyFixture(directory.path(), QStringLiteral("中文 sample.mp4"),
                                               QStringLiteral("sample%1.mp4").arg(index)));
    }
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("settings.ini")));
    ConversionController controller(&service, &store);
    controller.initialize();
    controller.addFiles(paths);
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
    QVERIFY(QFile::remove(paths.at(1)));
    ConversionSettings settings;
    settings.outputDirectory = directory.path();
    QSignalSpy finished(&controller, &ConversionController::batchFinished);
    QVERIFY(controller.startBatch(settings));
    // 批次设置冻结且运行期间不能导入、移除或清空。
    controller.clearTasks();
    controller.addFiles({paths.at(0)});
    QCOMPARE(controller.tasks().size(), 3);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
    QCOMPARE(controller.tasks().at(0).state, TaskState::Succeeded);
    QCOMPARE(controller.tasks().at(1).state, TaskState::Failed);
    QCOMPARE(controller.tasks().at(2).state, TaskState::Succeeded);
    const BatchSummary summary = qvariant_cast<BatchSummary>(finished.first().first());
    QCOMPARE(summary.succeeded, 2);
    QCOMPARE(summary.failed, 1);
    QCOMPARE(summary.cancelled, 0);
}

void ConversionTests::cancelCurrentContinues()
{
    // 真实长素材在有进度时取消，下一短素材仍应完成。
    QTemporaryDir directory;
    const QString first = test_support::copyFixture(directory.path(), QStringLiteral("long.mp4"),
                                                    QStringLiteral("first.mp4"));
    const QString second = test_support::copyFixture(
        directory.path(), QStringLiteral("中文 sample.mp4"), QStringLiteral("second.mp4"));
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("settings.ini")));
    ConversionController controller(&service, &store);
    controller.initialize();
    controller.addFiles({first, second});
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
    bool cancelled = false;
    connect(&controller, &ConversionController::currentProgress, &controller,
            [&controller, &cancelled](quint64, int percent, int position, int) {
                // 仅取消第一项一次，回调重入不影响下一任务。
                if (!cancelled && position == 1 && percent > 0) {
                    cancelled = true;
                    controller.cancelCurrent();
                    controller.cancelCurrent();
                }
            });
    ConversionSettings settings;
    settings.outputDirectory = directory.path();
    QSignalSpy finished(&controller, &ConversionController::batchFinished);
    QVERIFY(controller.startBatch(settings));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
    QVERIFY(cancelled);
    QCOMPARE(controller.tasks().at(0).state, TaskState::Cancelled);
    QCOMPARE(controller.tasks().at(1).state, TaskState::Succeeded);
    QVERIFY(controller.tasks().first().output.isEmpty());
    QCOMPARE(QDir(directory.path())
                 .entryList({QStringLiteral(".vc-*")}, QDir::Files | QDir::Hidden)
                 .size(),
             0);
    QVERIFY(!service.busy());
}

void ConversionTests::cancelVerification()
{
    // 正常编码退出后取消结果探测，不能因已完成进度而提交正式文件。
    QTemporaryDir directory;
    const QString input = test_support::copyFixture(
        directory.path(), QStringLiteral("中文 sample.mp4"), QStringLiteral("sample.mp4"));
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("settings.ini")));
    ConversionController controller(&service, &store);
    controller.initialize();
    controller.addFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
    bool cancelled = false;
    connect(&controller, &ConversionController::tasksChanged, &controller,
            [&controller, &cancelled] {
                // 状态变为验证时取消；先写本地标志防止 tasksChanged 递归。
                if (!cancelled && controller.tasks().first().state == TaskState::Verifying) {
                    cancelled = true;
                    controller.cancelCurrent();
                }
            });
    ConversionSettings settings;
    settings.outputDirectory = directory.path();
    QVERIFY(controller.startBatch(settings));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.batchActive(), 30000);
    QVERIFY(cancelled);
    QCOMPARE(controller.tasks().first().state, TaskState::Cancelled);
    QVERIFY(controller.tasks().first().output.isEmpty());
    QCOMPARE(QDir(directory.path())
                 .entryList({QStringLiteral(".vc-*")}, QDir::Files | QDir::Hidden)
                 .size(),
             0);
}

void ConversionTests::stopAllSuppressesScheduling()
{
    // 取消第一项的验证后停止全部，第二项必须从未进入转换。
    QTemporaryDir directory;
    QStringList paths;
    for (int index = 0; index < 2; ++index) {
        paths.append(test_support::copyFixture(directory.path(), QStringLiteral("中文 sample.mp4"),
                                               QStringLiteral("sample%1.mp4").arg(index)));
    }
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("settings.ini")));
    ConversionController controller(&service, &store);
    controller.initialize();
    controller.addFiles(paths);
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
    bool stopped = false;
    bool secondStarted = false;
    connect(&controller, &ConversionController::tasksChanged, &controller,
            [&controller, &stopped, &secondStarted] {
                // 检查下一项从未启动，同时在第一项验证阶段触发重复停止。
                secondStarted =
                    secondStarted || controller.tasks().at(1).state == TaskState::Converting;
                if (!stopped && controller.tasks().first().state == TaskState::Verifying) {
                    stopped = true;
                    controller.stopAll();
                    controller.stopAll();
                }
            });
    ConversionSettings settings;
    settings.outputDirectory = directory.path();
    QSignalSpy finished(&controller, &ConversionController::batchFinished);
    QVERIFY(controller.startBatch(settings));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
    QVERIFY(stopped);
    QVERIFY(!secondStarted);
    QCOMPARE(controller.tasks().at(0).state, TaskState::Cancelled);
    QCOMPARE(controller.tasks().at(1).state, TaskState::Cancelled);
    QCOMPARE(qvariant_cast<BatchSummary>(finished.first().first()).cancelled, 2);
}

void ConversionTests::shutdown_data()
{
    // 关闭覆盖启动检查、导入探测和实际转换三种活动资源。
    QTest::addColumn<int>("phase");
    QTest::newRow("startup") << 0;
    QTest::newRow("inspection") << 1;
    QTest::newRow("conversion") << 2;
}

void ConversionTests::shutdown()
{
    // 明确 shutdownReady 后无实际进程与活动队列。
    QFETCH(int, phase);
    QTemporaryDir directory;
    const QString input = test_support::copyFixture(directory.path(), QStringLiteral("long.mp4"),
                                                    QStringLiteral("sample.mp4"));
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("settings.ini")));
    ConversionController controller(&service, &store);
    QSignalSpy ready(&controller, &ConversionController::shutdownReady);
    controller.initialize();
    controller.addFiles({input});
    // 非初始化阶段等工具校验结束，并确认实际进入被测阶段。
    if (phase > 0) {
        QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
        if (phase == 1) {
            controller.removeTasks({controller.tasks().first().id});
            controller.addFiles({input});
            QTRY_VERIFY_WITH_TIMEOUT(service.busy(), 5000);
        } else {
            ConversionSettings settings;
            settings.outputDirectory = directory.path();
            QVERIFY(controller.startBatch(settings));
            QTRY_VERIFY_WITH_TIMEOUT(service.busy(), 5000);
        }
    }
    controller.requestShutdown();
    QTRY_VERIFY_WITH_TIMEOUT(ready.count() >= 1, 10000);
    QVERIFY(!service.busy());
    QVERIFY(!controller.hasActiveWork());
    QCOMPARE(QDir(directory.path())
                 .entryList({QStringLiteral(".vc-*")}, QDir::Files | QDir::Hidden)
                 .size(),
             0);
}

void ConversionTests::toolFailuresAndTimeout()
{
    // 不存在工具路径必须异步通知一次，并恢复空闲。
    QTemporaryDir directory;
    FfmpegService missing(directory.path());
    QSignalSpy failures(&missing, &FfmpegService::completed);
    QVERIFY(missing.start(1, Operation::Probe, {QStringLiteral("-version")}, 500));
    QTRY_COMPARE_WITH_TIMEOUT(failures.count(), 1, 5000);
    QVERIFY(!missing.busy());
    QVERIFY(!qvariant_cast<ProcessResult>(failures.first().at(1)).success);
    // 真实 ffmpeg 实时输入配合极短期限，验证超时终止与错误分类。
    FfmpegService timed(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    QSignalSpy timeout(&timed, &FfmpegService::completed);
    QVERIFY(timed.start(
        2, Operation::Thumbnail,
        {QStringLiteral("-re"), QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
         QStringLiteral("testsrc2=size=320x180:rate=1"), QStringLiteral("-t"), QStringLiteral("20"),
         QStringLiteral("-f"), QStringLiteral("null"), QStringLiteral("-")},
        100));
    QTRY_COMPARE_WITH_TIMEOUT(timeout.count(), 1, 6000);
    const ProcessResult result = qvariant_cast<ProcessResult>(timeout.first().at(1));
    QVERIFY(!result.success);
    QVERIFY(!result.cancelled);
    QVERIFY(result.error.contains(QStringLiteral("超时")));
    QVERIFY(!timed.busy());
}

void ConversionTests::invalidOutputDirectory()
{
    // 输出位置指向已有文件使目录创建失败，源文件不得受影响。
    QTemporaryDir directory;
    const QString input = test_support::copyFixture(
        directory.path(), QStringLiteral("中文 sample.mp4"), QStringLiteral("sample.mp4"));
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("settings.ini")));
    ConversionController controller(&service, &store);
    controller.initialize();
    controller.addFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
    ConversionSettings settings;
    settings.outputDirectory = input;
    QVERIFY(controller.startBatch(settings));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.batchActive(), 10000);
    QCOMPARE(controller.tasks().first().state, TaskState::Failed);
    QVERIFY(controller.tasks().first().detail.contains(QStringLiteral("目录")));
    QVERIFY(test_support::decode(input));
}

void ConversionTests::cleanupResidueContinues()
{
#ifdef Q_OS_WIN
    // 在第一项验证时以不共享删除权限的 Win32 句柄占用临时输出。
    QTemporaryDir directory;
    QStringList paths;
    for (int index = 0; index < 2; ++index) {
        paths.append(test_support::copyFixture(directory.path(), QStringLiteral("中文 sample.mp4"),
                                               QStringLiteral("sample%1.mp4").arg(index)));
    }
    FfmpegService service(qEnvironmentVariable("VIDEOCONVERSION_TOOLS"));
    SettingsStore store(QDir(directory.path()).filePath(QStringLiteral("settings.ini")));
    ConversionController controller(&service, &store);
    controller.initialize();
    controller.addFiles(paths);
    QTRY_VERIFY_WITH_TIMEOUT(controller.canStart(), 30000);
    HANDLE locked = INVALID_HANDLE_VALUE;
    QString residual;
    bool attempted = false;
    connect(&controller, &ConversionController::tasksChanged, &controller,
            [&controller, &attempted, &locked, &residual, &directory] {
                // 只锁住第一项的临时文件，防止状态通知重入重复获取句柄。
                if (!attempted && controller.tasks().first().state == TaskState::Verifying) {
                    attempted = true;
                    const QStringList temporary =
                        QDir(directory.path())
                            .entryList({QStringLiteral(".vc-*.mp4")}, QDir::Files | QDir::Hidden);
                    // 找不到临时文件时交由外层断言失败，不构造无效路径。
                    if (!temporary.isEmpty()) {
                        residual = QDir(directory.path()).filePath(temporary.first());
                        // Qt UTF-16 到 Win32 宽字符串的边界转换，不改变缓冲区内容或所有权。
                        locked =
                            CreateFileW(reinterpret_cast<LPCWSTR>(residual.utf16()), GENERIC_READ,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
                        controller.cancelCurrent();
                    }
                }
            });
    ConversionSettings settings;
    settings.outputDirectory = directory.path();
    QVERIFY(controller.startBatch(settings));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.batchActive(), 30000);
    // 先释放测试句柄，即使后续断言失败也不让文件锁泄漏。
    const bool didLock = locked != INVALID_HANDLE_VALUE;
    if (didLock) {
        CloseHandle(locked);
    }
    QVERIFY(didLock);
    QCOMPARE(controller.tasks().first().state, TaskState::Cancelled);
    QVERIFY(controller.tasks().first().detail.contains(QStringLiteral("清理失败")));
    QVERIFY(controller.tasks().first().detail.contains(residual));
    QCOMPARE(controller.tasks().at(1).state, TaskState::Succeeded);
    QVERIFY(QFileInfo::exists(residual));
    QVERIFY(QFile::remove(residual));
#else
    // 首版面向 Windows，本测试依赖 Windows 的删除共享权限。
    QSKIP("Windows handle-sharing test");
#endif
}

void ConversionTests::outputValidationRejectsMismatch()
{
    // 基线符合设置，然后逐项引入不可信结果，禁止误报成功。
    MediaInfo input;
    input.videoIndex = 0;
    input.audioIndex = 1;
    input.displaySize = QSize(320, 180);
    input.duration = 10;
    MediaInfo output = input;
    output.videoCodec = QStringLiteral("h264");
    output.audioCodec = QStringLiteral("aac");
    output.container = QStringLiteral("mov,mp4,m4a,3gp,3g2,mj2");
    QString error;
    QVERIFY(validateOutput(input, output, EncodingOptions{}, &error));
    output.videoCodec = QStringLiteral("hevc");
    QVERIFY(!validateOutput(input, output, EncodingOptions{}, &error));
    output.videoCodec = QStringLiteral("h264");
    output.duration = 2;
    QVERIFY(!validateOutput(input, output, EncodingOptions{}, &error));
    output.duration = 10;
    output.container = QStringLiteral("matroska,webm");
    QVERIFY(!validateOutput(input, output, EncodingOptions{}, &error));
}
}

QTEST_GUILESS_MAIN(videoconversion::ConversionTests)
#include "conversion_tests.moc"
