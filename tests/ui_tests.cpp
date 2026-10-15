// 文件：ui_tests.cpp
// 层：tests
// 作用：验证真实窗口交互、编辑锁与关闭选择，生成界面验收截图。
// 说明：使用隔离偏好和临时输出，不改变开发者的实际队列或设置。
#include <memory>

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtTest>

#include "conversion_controller.h"
#include "main_window.h"
#include "preview_dialog.h"

namespace videoconversion {
class UiTests : public QObject
{
    Q_OBJECT
   private slots:
    // 每个用例创建独立服务、设置与窗口，防止上一用例状态污染。
    void init();
    // 退出前必须异步停止进程和清理，随后按所有权顺序释放对象。
    void cleanup();
    // 文件拖入、多选移除、损坏提示、日志复制与偏好实际恢复。
    void importAndPresentation();
    // 实际转换期间按钮锁定；结束后结果、汇总与截图符合真实状态。
    void conversionAndScreenshots();
    // 默认、宽窗口和最小窗口的两页均保持可操作，长文件名不撑宽列表。
    void minimumLayout();
    // 查看日志期间进度持续更新，取消当前仍继续下一项且不会丢失页面状态。
    void logPageCancellation();
    // 继续转换保持窗口，停止并退出等到真正停止后才关闭。
    void guardedClose();
    // 原生预览草稿保存、非法输入、取消/恢复、片段播放及终态只读。
    void previewTransactions();
    // 多选入口、模态锁、连续加载关闭与错误会话不串回调或占用文件。
    void previewLifecycle();
    // 能力状态模拟只验证业务拒绝和旧代次隔离，不代替真实后端测试。
    void previewCapabilityGuards();
    // 真实加载失败的底层错误和输入上下文进入可复制日志。
    void previewDiagnostics();
    // 轮询日志不随位置刷屏，关闭后能重新打开独立会话。
    void previewDiagnosticRouting();
    // H.264/AAC MP4 与指定用户素材实际播放、定位和释放，不依赖 DirectShow。
    void previewMp4();
    // 暂停与播放中的真实鼠标拖动、轨道点击及键盘定位均应改变媒体位置。
    void previewTimeline_data();
    void previewTimeline();

   private:
    // 复制样例为隔离中文路径，失败返回空字符串。
    QString fixture(const QString &name, const QString &outputName);
    // 用本地 URL 模拟 Qt 文件拖入事件，不绕过窗口导入接口。
    void dropFiles(const QStringList &paths);
    // 记录实际渲染窗口；保存失败为测试失败。
    void screenshot(const QString &name);

    std::unique_ptr<QTemporaryDir> m_directory;
    std::unique_ptr<FfmpegService> m_service;
    std::unique_ptr<SettingsStore> m_store;
    std::unique_ptr<ConversionController> m_controller;
    std::unique_ptr<MediaPlaybackService> m_playback;
    std::unique_ptr<PreviewController> m_preview;
    std::unique_ptr<MainWindow> m_window;
};

void UiTests::init()
{
    // 对象按服务、偏好、控制器、窗口顺序创建，清理时按相反顺序。
    m_directory.reset(new QTemporaryDir);
    QVERIFY(m_directory->isValid());
    m_service.reset(new FfmpegService(qEnvironmentVariable("VIDEOCONVERSION_TOOLS")));
    m_store.reset(
        new SettingsStore(QDir(m_directory->path()).filePath(QStringLiteral("settings.ini"))));
    m_controller.reset(new ConversionController(m_service.get(), m_store.get()));
    m_playback.reset(new MediaPlaybackService);
    m_preview.reset(new PreviewController(m_playback.get(), m_controller.get()));
    m_window.reset(new MainWindow(m_controller.get(), m_preview.get()));
    connect(m_window.get(), &MainWindow::videoOutputChanged, m_playback.get(),
            &MediaPlaybackService::setVideoOutput);
    m_window->show();
    m_controller->initialize();
}

void UiTests::cleanup()
{
    // 测试失败也先停止活动工作，不让进程或临时文件泄漏到下个用例。
    QSignalSpy stopped(m_controller.get(), &ConversionController::shutdownReady);
    // 已完成安全退出的窗口不重复要求一次性通知；其他活动工作先停止。
    if (m_controller->hasActiveWork()) {
        m_controller->requestShutdown();
        QTRY_VERIFY_WITH_TIMEOUT(stopped.count() == 1, 10000);
    }
    QVERIFY(!m_service->busy());
    m_window.reset();
    m_preview.reset();
    m_playback.reset();
    m_controller.reset();
    m_store.reset();
    m_service.reset();
    m_directory.reset();
}

QString UiTests::fixture(const QString &name, const QString &outputName)
{
    // 测试只处理临时副本，不修改可重现素材原件。
    const QString input = QDir(qEnvironmentVariable("VIDEOCONVERSION_FIXTURES")).filePath(name);
    const QString output = QDir(m_directory->path()).filePath(outputName);
    return QFile::copy(input, output) ? output : QString();
}

void UiTests::dropFiles(const QStringList &paths)
{
    // 先发送拖入认可事件，再发送同一组 URL 的实际落下事件。
    QMimeData mime;
    QList<QUrl> urls;
    for (const QString &path : paths) {
        urls.append(QUrl::fromLocalFile(path));
    }
    mime.setUrls(urls);
    QDragEnterEvent enter(QPoint(100, 200), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(m_window.get(), &enter);
    QDropEvent drop(QPointF(100, 200), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(m_window.get(), &drop);
}

void UiTests::screenshot(const QString &name)
{
    // 截图输出目录必须由测试调用者指定，避免生成位置不明的交付材料。
    const QString directory = qEnvironmentVariable("VIDEOCONVERSION_SCREENSHOTS");
    QVERIFY(!directory.isEmpty());
    QVERIFY(QDir().mkpath(directory));
    QTest::qWait(50);
    QVERIFY(m_window->grab().save(QDir(directory).filePath(name)));
}

void UiTests::importAndPresentation()
{
    // 通过真实窗口的 drop 处理导入两项、重复项与目录。
    const QString valid =
        fixture(QStringLiteral("中文 sample.mp4"), QStringLiteral("中文 示例.mp4"));
    const QString invalid = fixture(QStringLiteral("damaged.mp4"), QStringLiteral("损坏.mp4"));
    QVERIFY(!valid.isEmpty() && !invalid.isEmpty());
    dropFiles({valid, invalid, valid, m_directory->path()});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    auto *list = m_window->findChild<QListWidget *>(QStringLiteral("videoList"));
    auto *start = m_window->findChild<QPushButton *>(QStringLiteral("startButton"));
    auto *remove = m_window->findChild<QPushButton *>(QStringLiteral("removeButton"));
    auto *logs = m_window->findChild<QPushButton *>(QStringLiteral("logsButton"));
    auto *view = m_window->findChild<QPlainTextEdit *>(QStringLiteral("logView"));
    auto *tabs = m_window->findChild<QTabWidget *>(QStringLiteral("pageTabs"));
    QVERIFY(list && start && remove && logs && view && tabs);
    QCOMPARE(list->count(), 2);
    QVERIFY(start->isEnabled());
    QVERIFY(list->item(0)->text().contains(QStringLiteral("h264")));
    QVERIFY(list->item(1)->text().contains(QStringLiteral("失败")));
    QVERIFY(!list->item(1)->toolTip().isEmpty());
    // 导入失败不强制切页，转换页隐藏的日志仍累积具体失败原因。
    QCOMPARE(tabs->currentIndex(), 0);
    QVERIFY(!view->isVisible());
    QVERIFY(view->toPlainText().contains(QStringLiteral("损坏.mp4")));
    // 查看日志入口切到整页并复制；切回后原有选择和设置继续有效。
    QTest::mouseClick(logs, Qt::LeftButton);
    QVERIFY(view->isVisible());
    auto *copy = m_window->findChild<QPushButton *>(QStringLiteral("copyButton"));
    QVERIFY(copy);
    QTest::mouseClick(copy, Qt::LeftButton);
    QCOMPARE(QApplication::clipboard()->text(), m_controller->sessionLog());
    tabs->setCurrentIndex(0);
    // 多选删除一次移除两项，空队列开始按钮禁用。
    list->item(0)->setSelected(true);
    list->item(1)->setSelected(true);
    QVERIFY(remove->isEnabled());
    QTest::mouseClick(remove, Qt::LeftButton);
    QCOMPARE(list->count(), 0);
    QVERIFY(!start->isEnabled());
    // 设置控件变化持久化，重新加载偏好后使用相同选项。
    auto *format = m_window->findChild<QComboBox *>(QStringLiteral("formatCombo"));
    QVERIFY(format);
    format->setCurrentIndex(1);
    QString warnings;
    QCOMPARE(m_store->load(&warnings).encoding.container, Container::Mkv);
}

void UiTests::conversionAndScreenshots()
{
    // 实际素材导入后保存就绪界面，再开始真实转换。
    const QString input =
        fixture(QStringLiteral("中文 sample.mp4"), QStringLiteral("旅行 示例.mp4"));
    QVERIFY(!input.isEmpty());
    dropFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    auto *directory = m_window->findChild<QLineEdit *>(QStringLiteral("directoryEdit"));
    auto *start = m_window->findChild<QPushButton *>(QStringLiteral("startButton"));
    auto *settings = m_window->findChild<QGroupBox *>(QStringLiteral("settingsGroup"));
    auto *add = m_window->findChild<QPushButton *>(QStringLiteral("addButton"));
    QVERIFY(directory && start && settings && add);
    directory->setText(QDir(m_directory->path()).filePath(QStringLiteral("输出")));
    screenshot(QStringLiteral("01-ready.png"));
    QTest::mouseClick(start, Qt::LeftButton);
    QVERIFY(m_controller->batchActive());
    QVERIFY(!settings->isEnabled());
    QVERIFY(!add->isEnabled());
    QVERIFY(!start->isEnabled());
    // 结束后控件恢复编辑，结果状态与汇总对应成功。
    QTRY_VERIFY_WITH_TIMEOUT(!m_controller->batchActive(), 30000);
    QCOMPARE(m_controller->tasks().first().state, TaskState::Succeeded);
    QVERIFY(settings->isEnabled() && add->isEnabled());
    QVERIFY(!start->isEnabled());
    auto *summary = m_window->findChild<QLabel *>(QStringLiteral("summaryLabel"));
    auto *progress = m_window->findChild<QProgressBar *>(QStringLiteral("progressBar"));
    QVERIFY(summary && progress);
    QVERIFY(summary->text().contains(QStringLiteral("成功 1")));
    QCOMPARE(progress->value(), 100);
    screenshot(QStringLiteral("02-completed.png"));
}

void UiTests::minimumLayout()
{
    // 导入长名称，默认高度保留列表纵向空间，名称不会要求横向滚动。
    const QString longName = QString(100, QLatin1Char('a')) + QStringLiteral("_长文件名.mp4");
    const QString input = fixture(QStringLiteral("中文 sample.mp4"), longName);
    QVERIFY(!input.isEmpty());
    dropFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    auto *list = m_window->findChild<QListWidget *>(QStringLiteral("videoList"));
    auto *tabs = m_window->findChild<QTabWidget *>(QStringLiteral("pageTabs"));
    auto *view = m_window->findChild<QPlainTextEdit *>(QStringLiteral("logView"));
    QVERIFY(list && tabs && view);
    // 内部列表不包含分组标题、边距和底部提示，保持不少于原有约 350 的浏览高度。
    QVERIFY(list->height() >= 350);
    QVERIFY(list->item(0)->toolTip().contains(longName));
    QCOMPARE(list->horizontalScrollBar()->maximum(), 0);
    QVERIFY(list->visualItemRect(list->item(0)).width() <= list->viewport()->width());
    screenshot(QStringLiteral("07-long-filename.png"));
    // 宽窗口仍限制列表宽度，保留日志页自由扩展的空间。
    m_window->resize(1400, 820);
    QTest::qWait(50);
    QVERIFY(list->parentWidget()->width() <= 620);
    // 最小尺寸日志占满主体，共享控制仍可见且处于窗口范围内。
    m_window->resize(m_window->minimumSize());
    auto *logs = m_window->findChild<QPushButton *>(QStringLiteral("logsButton"));
    auto *start = m_window->findChild<QPushButton *>(QStringLiteral("startButton"));
    QVERIFY(logs && start);
    QTest::mouseClick(logs, Qt::LeftButton);
    QTest::qWait(50);
    QCOMPARE(tabs->currentIndex(), 1);
    QVERIFY(view->isVisible());
    QVERIFY(view->height() >= 300);
    const QRect startBounds(start->mapTo(m_window.get(), QPoint()), start->size());
    QVERIFY(m_window->rect().contains(startBounds));
    screenshot(QStringLiteral("03-minimum-with-log.png"));
    // 切回转换页后设置和队列仍存在，页签切换不会替换控件。
    QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier,
                      tabs->tabBar()->tabRect(0).center());
    QCOMPARE(tabs->currentIndex(), 0);
    QCOMPARE(list->count(), 1);
    // 右侧空间不足时可滚动到全部设置，控件不重叠或截断提示内容。
    auto *scroll = m_window->findChild<QScrollArea *>(QStringLiteral("settingsScroll"));
    auto *hint = m_window->findChild<QLabel *>(QStringLiteral("encodingHint"));
    auto *quality = m_window->findChild<QComboBox *>(QStringLiteral("qualityCombo"));
    auto *resolution = m_window->findChild<QComboBox *>(QStringLiteral("resolutionCombo"));
    QVERIFY(scroll && hint && quality && resolution);
    QVERIFY(quality->geometry().bottom() < resolution->geometry().top());
    QVERIFY(hint->height() >= 92);
    scroll->ensureWidgetVisible(hint);
    QTest::qWait(50);
    const QRect hintBounds(hint->mapTo(scroll->viewport(), QPoint()), hint->size());
    QVERIFY(scroll->viewport()->rect().contains(hintBounds));
    screenshot(QStringLiteral("05-minimum-settings-scroll.png"));
}

// 真实转换时日志页共享同一批次，取消当前不会阻止下一项成功。
void UiTests::logPageCancellation()
{
    // 两项使用隔离文件和输出目录，长素材保证切页时仍有活动编码。
    const QString first = fixture(QStringLiteral("long.mp4"), QStringLiteral("第一个.mp4"));
    const QString second = fixture(QStringLiteral("中文 sample.mp4"), QStringLiteral("第二个.mp4"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());
    dropFiles({first, second});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    auto *directory = m_window->findChild<QLineEdit *>(QStringLiteral("directoryEdit"));
    auto *start = m_window->findChild<QPushButton *>(QStringLiteral("startButton"));
    auto *cancel = m_window->findChild<QPushButton *>(QStringLiteral("cancelButton"));
    auto *stop = m_window->findChild<QPushButton *>(QStringLiteral("stopButton"));
    auto *tabs = m_window->findChild<QTabWidget *>(QStringLiteral("pageTabs"));
    auto *view = m_window->findChild<QPlainTextEdit *>(QStringLiteral("logView"));
    auto *progress = m_window->findChild<QProgressBar *>(QStringLiteral("progressBar"));
    auto *list = m_window->findChild<QListWidget *>(QStringLiteral("videoList"));
    QVERIFY(directory && start && cancel && stop && tabs && view && progress && list);
    directory->setText(m_directory->path());
    list->item(0)->setSelected(true);
    const quint64 selectedId = list->item(0)->data(Qt::UserRole).toULongLong();
    QSignalSpy updates(m_controller.get(), &ConversionController::currentProgress);
    QTest::mouseClick(start, Qt::LeftButton);
    // 通过实际页签操作进入日志页，后续进度与诊断仍来自同一控制器。
    QTest::mouseClick(tabs->tabBar(), Qt::LeftButton, Qt::NoModifier,
                      tabs->tabBar()->tabRect(1).center());
    QTRY_VERIFY_WITH_TIMEOUT(updates.count() > 0, 5000);
    QVERIFY(m_controller->batchActive());
    QVERIFY(view->isVisible() && progress->isVisible());
    QVERIFY(view->toPlainText().contains(QStringLiteral("开始批次")));
    QVERIFY(cancel->isVisible() && cancel->isEnabled());
    QVERIFY(stop->isVisible() && stop->isEnabled());
    screenshot(QStringLiteral("06-converting-log.png"));
    QTest::mouseClick(cancel, Qt::LeftButton);
    // 取消仅结算第一项，第二项成功，用户所选页签和列表选择都保留。
    QTRY_VERIFY_WITH_TIMEOUT(!m_controller->batchActive(), 30000);
    QCOMPARE(m_controller->tasks().at(0).state, TaskState::Cancelled);
    QCOMPARE(m_controller->tasks().at(1).state, TaskState::Succeeded);
    QCOMPARE(tabs->currentIndex(), 1);
    tabs->setCurrentIndex(0);
    QCOMPARE(list->selectedItems().size(), 1);
    QCOMPARE(list->selectedItems().first()->data(Qt::UserRole).toULongLong(), selectedId);
    QCOMPARE(directory->text(), m_directory->path());
}

void UiTests::guardedClose()
{
    // 长素材保证关闭发生在真实活动转换中。
    const QString input = fixture(QStringLiteral("long.mp4"), QStringLiteral("long.mp4"));
    QVERIFY(!input.isEmpty());
    dropFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    auto *directory = m_window->findChild<QLineEdit *>(QStringLiteral("directoryEdit"));
    auto *start = m_window->findChild<QPushButton *>(QStringLiteral("startButton"));
    QVERIFY(directory && start);
    directory->setText(m_directory->path());
    QTest::mouseClick(start, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(m_service->busy(), 5000);
    bool continued = false;
    QTimer::singleShot(50, m_window.get(), [&continued] {
        // 模态框仍运行 Qt 事件循环，按钮选择模拟用户继续转换。
        auto *question = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (question) {
            for (QAbstractButton *button : question->buttons()) {
                if (button->text() == QStringLiteral("继续转换")) {
                    continued = true;
                    button->click();
                    break;
                }
            }
        }
    });
    m_window->close();
    QVERIFY(continued);
    QVERIFY(m_window->isVisible());
    QVERIFY(m_controller->batchActive());
    screenshot(QStringLiteral("04-converting.png"));
    bool exited = false;
    QTimer::singleShot(50, m_window.get(), [&exited] {
        // 第二次选择停止并退出，窗口必须等实际清理后才隐藏。
        auto *question = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (question) {
            for (QAbstractButton *button : question->buttons()) {
                if (button->text() == QStringLiteral("停止并退出")) {
                    exited = true;
                    button->click();
                    break;
                }
            }
        }
    });
    m_window->close();
    QVERIFY(exited);
    QTRY_VERIFY_WITH_TIMEOUT(!m_window->isVisible(), 10000);
    QVERIFY(!m_service->busy());
    QCOMPARE(m_controller->tasks().first().state, TaskState::Cancelled);
}
// 真实可播放样例覆盖草稿到队列的提交边界，拒绝非法输入与隐式整段回退。
void UiTests::previewTransactions()
{
    // 使用 MJPEG/PCM 中文路径验证既有片段操作兼容。
    const QString input = fixture(QStringLiteral("preview.avi"), QStringLiteral("预览 样例.avi"));
    m_controller->addFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    const quint64 id = m_controller->tasks().first().id;
    QString error;
    QVERIFY(m_preview->open(id, &error));
    PreviewDialog dialog(m_preview.get(), m_window.get());
    m_playback->setVideoOutput(dialog.videoWidget());
    dialog.show();
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->rangeError().isEmpty(), 10000);
    auto *start = dialog.findChild<QLineEdit *>(QStringLiteral("startEdit"));
    auto *end = dialog.findChild<QLineEdit *>(QStringLiteral("endEdit"));
    auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
    QVERIFY(start && end && buttons);
    // 手动输入保持原文，等于终点和超出时长都不能保存。
    start->selectAll();
    QTest::keyClicks(start, "00:00:01.350");
    end->selectAll();
    QTest::keyClicks(end, "00:00:01.350");
    QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());
    QVERIFY(!m_controller->tasks().first().range.enabled);
    end->selectAll();
    QTest::keyClicks(end, "00:00:07.000");
    QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());
    end->selectAll();
    QTest::keyClicks(end, "00:00:03.450");
    QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());
    // 片段播放从非关键帧起点定位，播放后停在终点附近并保持暂停。
    QTest::mouseClick(dialog.findChild<QPushButton *>(QStringLiteral("selectedButton")),
                      Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->state().playing, 3000);
    QVERIFY(m_preview->state().positionMs >= 1250);
    QTRY_VERIFY_WITH_TIMEOUT(!m_preview->state().playing, 5000);
    QVERIFY(m_preview->state().positionMs >= 3450 && m_preview->state().positionMs < 3750);
    // 最小窗口截图记录横向显示和毫秒文本；保存才提交到队列。
    dialog.resize(dialog.minimumSize());
    QTest::qWait(100);
    QVERIFY(QApplication::primaryScreen()
                ->grabWindow(0)
                .copy(QRect(dialog.mapToGlobal(QPoint(0, 0)), dialog.size()))
                .save(QDir(qEnvironmentVariable("VIDEOCONVERSION_SCREENSHOTS"))
                          .filePath(QStringLiteral("08-preview-range.png"))));
    QTest::mouseClick(buttons->button(QDialogButtonBox::Save), Qt::LeftButton);
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    m_playback->setVideoOutput(nullptr);
    QCOMPARE(m_controller->tasks().first().range.startMs, qint64(1350));
    QCOMPARE(m_controller->tasks().first().range.endMs, qint64(3450));
    QVERIFY(!m_controller->previewActive());
    // 重开后恢复整段再取消，先前保存的片段仍在。
    QVERIFY(m_preview->open(id, &error));
    PreviewDialog cancelled(m_preview.get(), m_window.get());
    m_playback->setVideoOutput(cancelled.videoWidget());
    cancelled.show();
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->rangeError().isEmpty(), 10000);
    QTest::mouseClick(cancelled.findChild<QPushButton *>(QStringLiteral("resetButton")),
                      Qt::LeftButton);
    QTest::keyClick(&cancelled, Qt::Key_Escape);
    m_playback->setVideoOutput(nullptr);
    QVERIFY(m_controller->tasks().first().range.enabled);
    // 恢复整段并保存后清除范围；整段转换成功的终态只读。
    QVERIFY(m_preview->open(id, &error));
    PreviewDialog reset(m_preview.get(), m_window.get());
    reset.show();
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->rangeError().isEmpty(), 10000);
    QTest::mouseClick(reset.findChild<QPushButton *>(QStringLiteral("resetButton")),
                      Qt::LeftButton);
    QTest::mouseClick(reset.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"))
                          ->button(QDialogButtonBox::Save),
                      Qt::LeftButton);
    QVERIFY(!m_controller->tasks().first().range.enabled);
    ConversionSettings settings;
    settings.outputDirectory = m_directory->path();
    QVERIFY(m_controller->startBatch(settings));
    QTRY_VERIFY_WITH_TIMEOUT(!m_controller->batchActive(), 30000);
    QCOMPARE(m_controller->tasks().first().state, TaskState::Succeeded);
    QVERIFY(m_preview->open(id, &error));
    QVERIFY(!m_preview->editable());
    QVERIFY(!m_preview->save({true, 100, 500}, &error));
    m_preview->close();
}

// 进度条的各种输入必须实际定位媒体，不能只改变滑块外观。
void UiTests::previewTimeline_data()
{
    // 分别保护拖动、轨道点击和键盘入口，播放中还要抵抗位置轮询。
    QTest::addColumn<QString>("interaction");
    QTest::addColumn<bool>("playing");
    QTest::newRow("drag-paused") << QStringLiteral("drag") << false;
    QTest::newRow("drag-playing") << QStringLiteral("drag") << true;
    QTest::newRow("click-paused") << QStringLiteral("click") << false;
    QTest::newRow("track-drag-paused") << QStringLiteral("track-drag") << false;
    QTest::newRow("keyboard-paused") << QStringLiteral("keyboard") << false;
    QTest::newRow("start-edge-paused") << QStringLiteral("start-edge") << false;
    QTest::newRow("end-edge-paused") << QStringLiteral("end-edge") << false;
}

// 对真实 VLC 会话发送控件事件，确认松开位置及暂停状态并保留保存范围。
void UiTests::previewTimeline()
{
    // 使用带索引的六秒素材，加载完成后再按实际样式取得滑块轨道。
    QFETCH(QString, interaction);
    QFETCH(bool, playing);
    const QString input = fixture(QStringLiteral("preview.avi"), QStringLiteral("定位.avi"));
    QVERIFY(!input.isEmpty());
    dropFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    QString error;
    QVERIFY(m_preview->open(m_controller->tasks().first().id, &error));
    PreviewDialog dialog(m_preview.get(), m_window.get());
    m_playback->setVideoOutput(dialog.videoWidget());
    dialog.show();
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->rangeError().isEmpty(), 10000);
    auto *slider = dialog.findChild<QSlider *>(QStringLiteral("positionSlider"));
    QVERIFY(slider && slider->isEnabled());
    // 播放用例等待后端状态稳定，拖动过程中不得被轮询拉回。
    if (playing) {
        m_preview->togglePlayback();
        QTRY_VERIFY_WITH_TIMEOUT(m_preview->state().playing, 3000);
    }
    QStyleOptionSlider option;
    option.initFrom(slider);
    option.orientation = Qt::Horizontal;
    option.minimum = slider->minimum();
    option.maximum = slider->maximum();
    option.sliderPosition = slider->sliderPosition();
    option.sliderValue = slider->value();
    const QRect handle = slider->style()->subControlRect(QStyle::CC_Slider, &option,
                                                         QStyle::SC_SliderHandle, slider);
    const QPoint target(slider->width() * 3 / 5, slider->height() / 2);
    // 拖动先按滑块再移动，持有期间等待多次播放轮询，最后松开提交。
    if (interaction == QStringLiteral("drag") || interaction == QStringLiteral("track-drag")) {
        const QPoint press = interaction == QStringLiteral("drag")
                                 ? handle.center()
                                 : QPoint(slider->width() / 4, slider->height() / 2);
        QTest::mousePress(slider, Qt::LeftButton, Qt::NoModifier, press);
        QVERIFY(slider->isSliderDown());
        QMouseEvent move(QEvent::MouseMove, target, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(slider, &move);
        const int dragged = slider->sliderPosition();
        QVERIFY(dragged > 5000 && dragged < 7000);
        QTest::qWait(250);
        QCOMPARE(slider->sliderPosition(), dragged);
        QTest::mouseRelease(slider, Qt::LeftButton, Qt::NoModifier, target);
    } else if (interaction == QStringLiteral("click")) {
        // 轨道点击应直接到目标，不能仅移动一页后被播放刷新覆盖。
        QTest::mouseClick(slider, Qt::LeftButton, Qt::NoModifier, target);
    } else if (interaction == QStringLiteral("start-edge") ||
               interaction == QStringLiteral("end-edge")) {
        // 首尾点击钳制到完整媒体边界，不能因滑块宽度留下不可到达区间。
        const int edge = interaction == QStringLiteral("start-edge") ? 0 : slider->width() - 1;
        QTest::mouseClick(slider, Qt::LeftButton, Qt::NoModifier,
                          QPoint(edge, slider->height() / 2));
    } else {
        // 每次翻页移动十分之一，键盘提交也应定位而非仅改变显示。
        slider->setFocus();
        for (int step = 0; step < 6; ++step) {
            QTest::keyClick(slider, Qt::Key_PageUp);
        }
    }
    // 真实后端应到目标附近；手动定位不能自动开始播放或保存转换选区。
    const qint64 duration = m_preview->state().durationMs;
    const qint64 expected = interaction == QStringLiteral("start-edge") ? 0
                            : interaction == QStringLiteral("end-edge") ? duration
                                                                        : duration * 3 / 5;
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(m_preview->state().positionMs - expected) < 500, 3000);
    // 原生暂停为异步状态变化，等待确认后再判定定位是否改变播放意图。
    QTRY_COMPARE_WITH_TIMEOUT(m_preview->state().playing, playing, 3000);
    QVERIFY(!m_controller->tasks().first().range.enabled);
    dialog.reject();
    m_playback->setVideoOutput(nullptr);
}

// 不同预览会话的加载错误、关闭与定位通知必须隔离，媒体释放后可以重命名输入。
void UiTests::previewLifecycle()
{
    // 两个方向的可播放视频用于入口多选与连续会话检查。
    const QString input = fixture(QStringLiteral("preview.avi"), QStringLiteral("A.avi"));
    const QString portrait =
        fixture(QStringLiteral("preview-portrait.avi"), QStringLiteral("B.avi"));
    m_controller->addFiles({input, portrait});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    auto *list = m_window->findChild<QListWidget *>(QStringLiteral("videoList"));
    auto *entry = m_window->findChild<QPushButton *>(QStringLiteral("previewButton"));
    QVERIFY(list && entry);
    list->item(0)->setSelected(true);
    list->item(1)->setSelected(true);
    QVERIFY(!entry->isEnabled());
    list->item(1)->setSelected(false);
    QVERIFY(entry->isEnabled());
    // 实际按钮进入模态循环，取消后恢复主窗口并解除表面引用。
    bool opened = false;
    QTimer::singleShot(0, m_window.get(), [this, &opened] {
        // 对话框已由按钮入口创建，检查互斥后取消，避免测试留下模态窗口。
        auto *dialog = m_window->findChild<PreviewDialog *>();
        if (dialog) {
            opened =
                m_controller->previewActive() && !m_controller->canStart() && dialog->isModal();
            m_window->close();
            opened = opened && m_window->isVisible();
            dialog->reject();
        }
    });
    QTest::mouseClick(entry, Qt::LeftButton);
    QVERIFY(opened);
    QVERIFY(!m_controller->previewActive());
    QString error;
    // 立即关闭加载及定位中的会话；后续输入不能被旧事件污染。
    for (int i = 0; i < 4; ++i) {
        QVERIFY(m_preview->open(m_controller->tasks()[i % 2].id, &error));
        m_preview->close();
        QTest::qWait(30);
        QVERIFY(!m_preview->state().loaded);
    }
    QVERIFY(QFile::rename(input, input + QStringLiteral(".released")));
    QVERIFY(QFile::rename(input + QStringLiteral(".released"), input));
    // 竖屏预览保存最小布局截图，取消不改变队列。
    QVERIFY(m_preview->open(m_controller->tasks().last().id, &error));
    PreviewDialog dialog(m_preview.get(), m_window.get());
    m_playback->setVideoOutput(dialog.videoWidget());
    dialog.resize(dialog.minimumSize());
    dialog.show();
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->rangeError().isEmpty(), 10000);
    m_preview->togglePlayback();
    QTest::qWait(200);
    QVERIFY(QApplication::primaryScreen()
                ->grabWindow(0)
                .copy(QRect(dialog.mapToGlobal(QPoint(0, 0)), dialog.size()))
                .save(QDir(qEnvironmentVariable("VIDEOCONVERSION_SCREENSHOTS"))
                          .filePath(QStringLiteral("09-preview-portrait.png"))));
    dialog.reject();
    m_playback->setVideoOutput(nullptr);
    // 打开检查后、异步加载前删除输入，后端必须报告实际资源错误。
    QVERIFY(m_preview->open(m_controller->tasks().first().id, &error));
    QVERIFY(QFile::remove(input));
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->state().failed, 11000);
    QVERIFY(!m_preview->rangeError().isEmpty());
    QVERIFY(!m_preview->save({true, 100, 500}, &error));
    m_preview->close();
    QVERIFY(m_controller->canStart());
}
// 能力不足、时长分歧和旧代次不能提交草稿或覆盖已有范围。
void UiTests::previewCapabilityGuards()
{
    // 真实加载得到当前代次，再显式模拟后端能力变化以覆盖罕见状态。
    const QString input =
        fixture(QStringLiteral("preview.avi"), QStringLiteral("capabilities.avi"));
    m_controller->addFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    quint64 generation = 0;
    QObject listener;
    connect(m_playback.get(), &MediaPlaybackService::updated, &listener,
            [&generation](quint64 value, const PlaybackState &) { generation = value; });
    QString error;
    const quint64 id = m_controller->tasks().first().id;
    QVERIFY(m_preview->open(id, &error));
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->rangeError().isEmpty(), 10000);
    const PlaybackState original = m_preview->state();
    QVERIFY(generation > 0);
    const quint64 activeGeneration = generation;
    QVERIFY(m_preview->save({true, 1350, 3450}, &error));
    // 旧代次错误不能影响当前有效会话。
    PlaybackState changed = original;
    changed.failed = true;
    changed.error = QStringLiteral("模拟错误");
    m_playback->updated(activeGeneration + 100, changed);
    QVERIFY(!m_preview->state().failed);
    // 未知时长、不可定位和时长偏差均拒绝新的保存，原范围不变。
    changed = original;
    changed.durationMs = 0;
    m_playback->updated(activeGeneration, changed);
    QVERIFY(!m_preview->save({}, &error));
    changed = original;
    changed.seekable = false;
    m_playback->updated(activeGeneration, changed);
    QVERIFY(!m_preview->save({}, &error));
    changed = original;
    changed.durationMs += 1000;
    m_playback->updated(activeGeneration, changed);
    QVERIFY(!m_preview->save({}, &error));
    QCOMPARE(m_controller->tasks().first().range.startMs, qint64(1350));
    // 当前会话真实代次的错误禁用保存；关闭后同一通知也不能复活状态。
    changed = original;
    changed.failed = true;
    changed.error = QStringLiteral("模拟错误");
    m_playback->updated(activeGeneration, changed);
    QVERIFY(!m_preview->save({}, &error));
    m_preview->close();
    m_playback->updated(activeGeneration, changed);
    QVERIFY(!m_preview->state().failed);
    QVERIFY(m_controller->canStart());
}

}

namespace videoconversion {
void UiTests::previewDiagnostics()
{
    // 先完成探测，再在异步播放加载前删除输入，稳定触发真实资源错误。
    const QString input = fixture(QStringLiteral("preview.avi"), QStringLiteral("诊断 样例.avi"));
    m_controller->addFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    QString error;
    QVERIFY(m_preview->open(m_controller->tasks().first().id, &error));
    QVERIFY(QFile::remove(input));
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->state().failed, 11000);
    QTRY_VERIFY_WITH_TIMEOUT(
        m_controller->sessionLog().contains(QStringLiteral("libVLC 无法预览此视频")), 3000);
    // 诊断同时保留正常探测和真实打开时的文件检查，不能只显示笼统的错误编号。
    const QString log = m_controller->sessionLog();
    QVERIFY(log.contains(input));
    QVERIFY(log.contains(QStringLiteral("ffprobe：container=avi；video=mjpeg；audio=pcm_s16le")));
    QVERIFY(log.contains(QStringLiteral("打开：exists=0；readable=0")));
    QVERIFY(log.contains(QStringLiteral("后端=libVLC")));
    QVERIFY(log.contains(QStringLiteral("失败：libVLC 无法预览此视频")));
    QVERIFY(!m_controller->tasks().first().range.enabled);
    // 关闭模态会话后查看及复制现有日志页，文本不能丢失底层警告。
    m_preview->close();
    QTest::mouseClick(m_window->findChild<QPushButton *>(QStringLiteral("logsButton")),
                      Qt::LeftButton);
    auto *view = m_window->findChild<QPlainTextEdit *>(QStringLiteral("logView"));
    QVERIFY(view->toPlainText().contains(QStringLiteral("libVLC 无法预览此视频")));
    QTest::mouseClick(m_window->findChild<QPushButton *>(QStringLiteral("copyButton")),
                      Qt::LeftButton);
    QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("后端=libVLC")));
    QVERIFY(m_controller->sessionLog().contains(QStringLiteral("关闭并释放媒体")));
}

void UiTests::previewDiagnosticRouting()
{
    // 正常播放位置更新不重复刷状态日志，重开会话仍可加载。
    const QString input = fixture(QStringLiteral("preview.avi"), QStringLiteral("路由.avi"));
    m_controller->addFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    QString error;
    const quint64 id = m_controller->tasks().first().id;
    QVERIFY(m_preview->open(id, &error));
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->rangeError().isEmpty(), 10000);
    m_preview->togglePlayback();
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->state().positionMs > 100, 3000);
    const int states = m_controller->sessionLog().count(QStringLiteral("后端=libVLC；状态="));
    QTest::qWait(200);
    QCOMPARE(m_controller->sessionLog().count(QStringLiteral("后端=libVLC；状态=")), states);
    m_preview->close();
    QVERIFY(m_preview->open(id, &error));
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->rangeError().isEmpty(), 10000);
    QVERIFY(m_preview->state().loaded);
    QVERIFY(!m_preview->state().failed);
    m_preview->close();
}

void UiTests::previewMp4()
{
    // 默认使用可重现的 H.264/AAC 样例，指定路径时额外覆盖用户的实际素材。
    const QString userInput = qEnvironmentVariable("VIDEOCONVERSION_PLAYBACK_INPUT");
    const QString input = userInput.isEmpty() ? fixture(QStringLiteral("中文 sample.mp4"),
                                                        QStringLiteral("预览 MP4.mp4"))
                                              : userInput;
    m_controller->addFiles({input});
    QTRY_VERIFY_WITH_TIMEOUT(m_controller->canStart(), 30000);
    QString error;
    QVERIFY(m_preview->open(m_controller->tasks().first().id, &error));
    PreviewDialog dialog(m_preview.get(), m_window.get());
    m_playback->setVideoOutput(dialog.videoWidget());
    dialog.show();
    // 加载暂停后必须获得可定位时长，随后能从普通播放切到选区播放。
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->rangeError().isEmpty(), 10000);
    m_preview->togglePlayback();
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->state().playing, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->state().positionMs > 100, 3000);
    m_preview->togglePlayback();
    QTRY_VERIFY_WITH_TIMEOUT(!m_preview->state().playing, 3000);
    const qint64 duration = m_preview->state().durationMs;
    QVERIFY(m_preview->playSelected({true, duration / 4, duration * 3 / 4}, &error));
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->state().playing, 3000);
    // 原生窗口截图用于独立检查视频画面；状态断言不能代替画面证据。
    QTest::qWait(150);
    QVERIFY(QApplication::primaryScreen()
                ->grabWindow(0)
                .copy(QRect(dialog.mapToGlobal(QPoint(0, 0)), dialog.size()))
                .save(QDir(qEnvironmentVariable("VIDEOCONVERSION_SCREENSHOTS"))
                          .filePath(QStringLiteral("10-preview-mp4.png"))));
    QTRY_VERIFY_WITH_TIMEOUT(!m_preview->state().playing, 8000);
    QVERIFY2(m_preview->state().positionMs >= duration * 3 / 4,
             qPrintable(m_controller->sessionLog()));
    // 播放到整段末尾后仍可回到中间重播片段，不因 VLC 输入关闭而锁死滑块。
    m_preview->togglePlayback();
    QTRY_VERIFY_WITH_TIMEOUT(m_preview->state().positionMs >= duration, 3000);
    QVERIFY(m_preview->rangeError().isEmpty());
    QVERIFY(m_preview->playSelected({true, duration / 4, duration * 3 / 4}, &error));
    QTRY_VERIFY2_WITH_TIMEOUT(m_preview->state().playing, qPrintable(m_controller->sessionLog()),
                              3000);
    // 关闭后播放器状态清空，不改变源文件或保存范围。
    dialog.reject();
    m_playback->setVideoOutput(nullptr);
    QVERIFY(!m_controller->previewActive());
    QVERIFY(!m_playback->state().loaded);
    QVERIFY(QFileInfo::exists(input));
}
}

QTEST_MAIN(videoconversion::UiTests)
#include "ui_tests.moc"
