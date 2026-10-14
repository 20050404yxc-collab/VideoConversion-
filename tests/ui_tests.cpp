// 文件：ui_tests.cpp
// 层：tests
// 作用：验证真实窗口交互、编辑锁与关闭选择，生成界面验收截图。
// 说明：使用隔离偏好和临时输出，不改变开发者的实际队列或设置。
#include <memory>

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
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
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtTest>

#include "conversion_controller.h"
#include "main_window.h"

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
    m_window.reset(new MainWindow(m_controller.get()));
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
}

QTEST_MAIN(videoconversion::UiTests)
#include "ui_tests.moc"
