// 文件：main_window.cpp
// 层：Application
// 作用：把用户操作转交控制器并刷新列表、进度和结果。
// 说明：日志错误使用非模态提示，关闭操作异步等待进程清理。
#include "main_window.h"

#include <algorithm>

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QUrl>

#include "preview_dialog.h"
#include "ui_main_window.h"

namespace videoconversion {
namespace presentation {
class CompactVideoDelegate : public QStyledItemDelegate
{
   public:
    // 由列表持有委托，销毁窗口时随控件释放。
    explicit CompactVideoDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

    // 行宽由视口决定，长名称通过标准委托省略，保留缩略图和多行信息的高度。
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // 沿用标准委托的纵向尺寸，避免内容宽度撑大内部行并绕过省略绘制。
        const QSize naturalSize = QStyledItemDelegate::sizeHint(option, index);
        return QSize(0, naturalSize.height());
    }
};

// 状态映射为用户能理解的文字，不暴露内部进程操作类型。
QString stateName(TaskState state)
{
    // 每个业务阶段均有独立显示名，终态保留直到用户移除。
    switch (state) {
    // 导入阶段尚未确认可转换媒体。
    case TaskState::Inspecting:
        return QStringLiteral("读取信息");
    // 有效媒体等待加入下一次批次。
    case TaskState::Waiting:
        return QStringLiteral("等待转换");
    // 实际编码进程正在运行。
    case TaskState::Converting:
        return QStringLiteral("转换中");
    // 编码结束但尚未完成结果验证和文件提交。
    case TaskState::Verifying:
        return QStringLiteral("验证结果");
    // 取消请求已受理，进程停止和清理尚未结束。
    case TaskState::Cancelling:
        return QStringLiteral("取消中");
    // 已验证且不覆盖提交正式文件。
    case TaskState::Succeeded:
        return QStringLiteral("成功");
    // 具体错误原因保留于项提示和日志。
    case TaskState::Failed:
        return QStringLiteral("失败");
    // 取消与清理已经结算。
    case TaskState::Cancelled:
        return QStringLiteral("已取消");
    }
    return QStringLiteral("未知");
}

// 信息未确定时明确显示未知，大小按 MiB 展示。
QString mediaDescription(const ConversionTask &task)
{
    // 时长可能未知，不把它显示为 0 秒；分辨率采用显示方向。
    const QString duration = task.media.duration > 0
                                 ? QStringLiteral("%1 秒").arg(task.media.duration, 0, 'f', 1)
                                 : QStringLiteral("时长未知");
    const QString dimensions = task.media.displaySize.isValid()
                                   ? QStringLiteral("%1 × %2")
                                         .arg(task.media.displaySize.width())
                                         .arg(task.media.displaySize.height())
                                   : QStringLiteral("分辨率未知");
    const QString video =
        task.media.videoCodec.isEmpty() ? QStringLiteral("视频编码未知") : task.media.videoCodec;
    const QString audio = task.media.videoIndex < 0   ? QStringLiteral("音频未知")
                          : task.media.audioIndex < 0 ? QStringLiteral("无音轨")
                                                      : task.media.audioCodec;
    return QStringLiteral("%1  ·  %2  ·  %3 MiB\n%4 / %5  ·  %6")
        .arg(duration, dimensions)
        .arg(task.bytes / (1024.0 * 1024), 0, 'f', 2)
        .arg(video, audio, stateName(task.state));
}
}

MainWindow::MainWindow(ConversionController *controller, PreviewController *preview,
                       QWidget *parent)
    : QMainWindow(parent), m_ui(new Ui::MainWindow), m_controller(controller), m_preview(preview)
{
    // 控件归窗口所有，先恢复偏好再连接编辑信号，避免启动时无意义保存。
    m_ui->setupUi(this);
    setupPresentation();
    connectActions();
    connectPresentation();
    refreshTasks();
    refreshState();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupPresentation()
{
    // 启用整个窗口的文件拖入；默认显示转换页，两个页面共用控制器与日志。
    setAcceptDrops(true);
    m_ui->videoList->setAcceptDrops(false);
    m_ui->pageTabs->setCurrentWidget(m_ui->conversionPage);
    // 收窄列表后省略长名称；完整路径留在提示中，避免横向滚动占用空间。
    m_ui->videoList->setTextElideMode(Qt::ElideRight);
    m_ui->videoList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_ui->videoList->setItemDelegate(new presentation::CompactVideoDelegate(m_ui->videoList));
    // 日志控件随页签切换保持存活，隐藏期间也继续接收有界诊断。
    m_ui->logView->setMaximumBlockCount(2000);
    m_ui->logView->setPlainText(m_controller->sessionLog());
    // 在连接用户改动前恢复偏好，避免启动时重复保存。
    const ConversionSettings settings = m_controller->preferences();
    m_ui->formatCombo->setCurrentIndex(static_cast<int>(settings.encoding.container));
    m_ui->qualityCombo->setCurrentIndex(static_cast<int>(settings.encoding.quality));
    m_ui->resolutionCombo->setCurrentIndex(static_cast<int>(settings.encoding.resolution));
    m_ui->directoryEdit->setText(QDir::toNativeSeparators(settings.outputDirectory));
    // 无缩略图时显示确定的占位图，而不是留下空白或断图。
    m_placeholder = QPixmap(128, 80);
    m_placeholder.fill(QColor(QStringLiteral("#e6edf8")));
    QPainter painter(&m_placeholder);
    painter.setPen(QColor(QStringLiteral("#597296")));
    painter.drawText(m_placeholder.rect(), Qt::AlignCenter, QStringLiteral("VIDEO"));
    // 使用原生布局加轻量样式，高 DPI 下控件尺寸可随字体调整。
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f4f6fa; }"
        "QWidget { font-family: 'Microsoft YaHei UI'; font-size: 13px; color: #24324a; }"
        "QLabel#titleLabel { font-size: 25px; font-weight: 600; }"
        "QLabel#introLabel, QLabel#encodingHint, QLabel#queueHint { color: #65748c; }"
        "QLabel#noticeLabel { padding: 8px; background: #eaf0fc; border-radius: 5px; }"
        "QGroupBox { background: white; border: 1px solid #dce3ed; border-radius: 8px;"
        "margin-top: 12px; padding: 16px 10px 10px; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 5px; }"
        "QPushButton { background: white; border: 1px solid #d4ddea; border-radius: 5px;"
        "padding: 8px 14px; }"
        "QPushButton:hover { background: #edf3ff; border-color: #8baceb; }"
        "QPushButton:disabled { color: #9ba8bb; background: #f1f3f7; }"
        "QPushButton#startButton { background: #346ce5; color: white; font-weight: 600; }"
        "QPushButton#startButton:disabled { background: #a9bce8; }"
        "QComboBox, QLineEdit { background: white; border: 1px solid #d4ddea;"
        "border-radius: 4px; padding: 7px; min-height: 20px; }"
        "QListWidget { border: 0; background: white; }"
        "QScrollArea, QWidget#settingsContents { background: white; }"
        "QListWidget::item { padding: 10px; border-radius: 5px; }"
        "QListWidget::item:selected { background: #e5edff; color: #1e407a; }"
        "QTabWidget::pane { border: 0; }"
        "QTabBar::tab { background: #e9eef7; border-radius: 5px; padding: 9px 20px;"
        "margin-right: 8px; }"
        "QTabBar::tab:selected { background: #346ce5; color: white; }"
        "QPlainTextEdit { background: white; border: 1px solid #dce3ed; border-radius: 5px;"
        "padding: 8px; font-family: Consolas, 'Microsoft YaHei UI'; }"
        "QProgressBar { border: 1px solid #dce3ed; border-radius: 5px; background: white;"
        "text-align: center; min-height: 18px; }"
        "QProgressBar::chunk { background: #73a0f7; border-radius: 4px; }"));
}

void MainWindow::connectActions()
{
    // 文件选择支持多选与所有文件，具体格式有效性由异步探测判断。
    connect(m_ui->addButton, &QPushButton::clicked, this, [this] {
        // 选择完成后交给控制器异步导入，取消选择不添加任何项。
        const QStringList paths = QFileDialog::getOpenFileNames(
            this, QStringLiteral("添加视频"), QString(),
            QStringLiteral(
                "视频文件 (*.mp4 *.mkv *.mov *.avi *.webm *.m4v *.ts *.wmv);;所有文件 (*)"));
        m_controller->addFiles(paths);
    });
    // 将选择转成稳定任务标识，避免界面索引影响控制层队列。
    connect(m_ui->removeButton, &QPushButton::clicked, this, [this] {
        // 收集全部选择的稳定标识，一次移除，保留其他任务顺序。
        QVector<quint64> ids;
        for (const QListWidgetItem *item : m_ui->videoList->selectedItems()) {
            ids.append(item->data(Qt::UserRole).toULongLong());
        }
        m_controller->removeTasks(ids);
    });
    connect(m_ui->clearButton, &QPushButton::clicked, m_controller,
            &ConversionController::clearTasks);
    connect(m_ui->startButton, &QPushButton::clicked, this, [this] {
        // 开始前传递设置值对象，控制器负责保存与冻结。
        if (m_controller->startBatch(selectedSettings())) {
            showNotice(
                QStringLiteral("正在转换本批视频。取消当前会继续下一项；停止全部会取消剩余任务。"));
        }
    });
    connect(m_ui->cancelButton, &QPushButton::clicked, m_controller,
            &ConversionController::cancelCurrent);
    connect(m_ui->stopButton, &QPushButton::clicked, m_controller, &ConversionController::stopAll);
    connect(m_ui->browseButton, &QPushButton::clicked, this, [this] {
        // 用户取消目录选择时保留当前目录。
        const QString directory = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择输出目录"), m_ui->directoryEdit->text());
        if (!directory.isEmpty()) {
            m_ui->directoryEdit->setText(QDir::toNativeSeparators(directory));
            m_controller->savePreferences(selectedSettings());
        }
    });
    // 空闲编辑立即保存偏好，运行时由 settingsGroup 锁定并由控制器再次防护。
    const auto save = [this] { m_controller->savePreferences(selectedSettings()); };
    connect(m_ui->formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, save);
    connect(m_ui->qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, save);
    connect(m_ui->resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, save);
    connect(m_ui->directoryEdit, &QLineEdit::editingFinished, this, save);
    connect(m_ui->logsButton, &QPushButton::clicked, this, [this] {
        // 只切换已有页面，不重建日志控件或打断正在转换的批次。
        m_ui->pageTabs->setCurrentWidget(m_ui->logsPage);
    });
    connect(m_ui->copyButton, &QPushButton::clicked, this, [this] {
        // 复制控制器的当前会话有界日志，包含转换页期间积累的诊断。
        QApplication::clipboard()->setText(m_controller->sessionLog());
        showNotice(QStringLiteral("当前会话日志已复制。"));
    });
    connect(m_ui->openButton, &QPushButton::clicked, this, [this] {
        // 只打开已存在目录，不把错误目录伪装成结果可用。
        const QString directory = m_ui->directoryEdit->text();
        if (!QDir(directory).exists() ||
            !QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(directory).absolutePath()))) {
            showNotice(QStringLiteral("无法打开输出目录，请检查目录是否存在。"));
        }
    });
}

void MainWindow::connectPresentation()
{
    // 单击入口和双击都按当前单选标识打开同一事务式预览窗口。
    connect(m_ui->previewButton, &QPushButton::clicked, this, &MainWindow::openPreview);
    connect(m_ui->videoList, &QListWidget::itemDoubleClicked, this, &MainWindow::openPreview);
    // 控制层通知决定展示内容，窗口不推断工具完成是否成功。
    connect(m_controller, &ConversionController::tasksChanged, this, &MainWindow::refreshTasks);
    connect(m_controller, &ConversionController::stateChanged, this, &MainWindow::refreshState);
    connect(m_controller, &ConversionController::notice, this, &MainWindow::showNotice);
    connect(m_controller, &ConversionController::currentProgress, this, &MainWindow::showProgress);
    connect(m_ui->videoList, &QListWidget::itemSelectionChanged, this, &MainWindow::refreshState);
    connect(m_controller, &ConversionController::logAdded, this, [this](const QString &text) {
        // 日志控件限制块数，不让隐藏日志无限占用内存。
        m_ui->logView->appendPlainText(text.trimmed());
    });
    connect(
        m_controller, &ConversionController::batchFinished, this,
        [this](const BatchSummary &summary) {
            // 汇总反映本批终态，导入时的错误仍留在各项展示。
            m_ui->summaryLabel->setText(QStringLiteral("完成汇总：成功 %1，失败 %2，取消 %3")
                                            .arg(summary.succeeded)
                                            .arg(summary.failed)
                                            .arg(summary.cancelled));
            m_ui->currentLabel->setText(QStringLiteral("本批转换结束。重试请移除该项后重新添加。"));
            m_ui->progressBar->setRange(0, 100);
            m_ui->progressBar->setValue(summary.failed == 0 && summary.cancelled == 0 ? 100 : 0);
            showNotice(QStringLiteral("本批已结束：成功 %1，失败 %2，取消 %3。")
                           .arg(summary.succeeded)
                           .arg(summary.failed)
                           .arg(summary.cancelled));
        });
    connect(m_controller, &ConversionController::shutdownReady, this, [this] {
        // 实际进程已停止且清理结束，第二次 close 不再询问。
        m_allowClose = true;
        close();
    });
}

ConversionSettings MainWindow::selectedSettings() const
{
    // 枚举值与固定选项顺序一致，目录从界面获取独立字符串。
    ConversionSettings settings;
    settings.encoding.container = static_cast<Container>(m_ui->formatCombo->currentIndex());
    settings.encoding.quality = static_cast<Quality>(m_ui->qualityCombo->currentIndex());
    settings.encoding.resolution = static_cast<Resolution>(m_ui->resolutionCombo->currentIndex());
    settings.outputDirectory = m_ui->directoryEdit->text().trimmed();
    return settings;
}

void MainWindow::refreshTasks()
{
    // 重建前记住稳定标识和滚动位置，批量状态变化不丢用户选择。
    QSet<quint64> selected;
    for (const QListWidgetItem *item : m_ui->videoList->selectedItems()) {
        selected.insert(item->data(Qt::UserRole).toULongLong());
    }
    const int scroll = m_ui->videoList->verticalScrollBar()->value();
    m_ui->videoList->blockSignals(true);
    m_ui->videoList->clear();
    // 列表顺序完全来自控制器；详细错误和完整路径同时放到提示中。
    for (const ConversionTask &task : m_controller->tasks()) {
        const QString text = QFileInfo(task.input).fileName() + QLatin1Char('\n') +
                             presentation::mediaDescription(task) + QLatin1Char('\n') +
                             rangeDescription(task.range);
        const QPixmap thumbnail =
            task.thumbnail.isNull() ? m_placeholder : QPixmap::fromImage(task.thumbnail);
        auto *item = new QListWidgetItem(QIcon(thumbnail), text, m_ui->videoList);
        item->setData(Qt::UserRole, QVariant::fromValue(task.id));
        item->setToolTip(task.input + QLatin1Char('\n') + rangeDescription(task.range) +
                         QLatin1Char('\n') + task.detail);
        item->setSelected(selected.contains(task.id));
        // 失败和取消颜色区分，文字状态仍保证不依赖颜色识别。
        if (task.state == TaskState::Failed) {
            item->setForeground(QColor(QStringLiteral("#b03d43")));
        } else if (task.state == TaskState::Succeeded) {
            // 成功色只用于已提交的正式结果。
            item->setForeground(QColor(QStringLiteral("#278261")));
        }
    }
    m_ui->videoList->blockSignals(false);
    m_ui->videoList->verticalScrollBar()->setValue(scroll);
    m_ui->queueHint->setText(
        m_controller->tasks().isEmpty()
            ? QStringLiteral("尚未添加视频。支持多选文件与拖拽，不导入目录。")
            : QStringLiteral("共 %1 项 · 失败原因或结果路径可将鼠标移到该项查看。")
                  .arg(m_controller->tasks().size()));
    refreshState();
}

void MainWindow::refreshState()
{
    // 批次锁定所有编辑，等待取消完成期间继续保持锁定。
    const bool editable =
        !m_controller->batchActive() && !m_controller->previewActive() && !m_closing;
    m_ui->addButton->setEnabled(editable);
    m_ui->removeButton->setEnabled(editable && !m_ui->videoList->selectedItems().isEmpty());
    m_ui->clearButton->setEnabled(editable && !m_controller->tasks().isEmpty());
    m_ui->settingsGroup->setEnabled(editable);
    m_ui->startButton->setEnabled(m_controller->canStart() && !m_closing);
    // 多选不任意挑一个输入；有效终态仍可只读查看原视频。
    const auto selected = m_ui->videoList->selectedItems();
    bool previewable = false;
    if (selected.size() == 1) {
        const quint64 id = selected.first()->data(Qt::UserRole).toULongLong();
        // 队列刷新后按标识确认信息有效，原文件访问由控制层打开时复核。
        for (const ConversionTask &task : m_controller->tasks()) {
            // 完整媒体校验通过才可预览，不能把探测失败的半成品当成有效信息。
            if (task.id == id && task.state != TaskState::Inspecting && task.mediaValid) {
                previewable = true;
                break;
            }
        }
    }
    m_ui->previewButton->setEnabled(editable && m_preview && previewable);
    m_ui->previewButton->setToolTip(
        QStringLiteral("请选择一个已读取信息的视频；多选或转换期间不能预览。"));
    bool currentCancelable = false;
    // 只有实际转换或验证中的项启用取消，重复点击保持无效。
    for (const ConversionTask &task : m_controller->tasks()) {
        currentCancelable = currentCancelable || task.state == TaskState::Converting ||
                            task.state == TaskState::Verifying;
    }
    m_ui->cancelButton->setEnabled(currentCancelable && !m_closing);
    m_ui->stopButton->setEnabled(m_controller->batchActive() && !m_closing);
    // 成功就绪后替换启动检查提示，保留已出现的具体错误提示。
    if (m_ui->noticeLabel->text() == QStringLiteral("正在检查转换工具……") &&
        m_controller->toolsReady()) {
        m_ui->noticeLabel->setText(
            QStringLiteral("转换工具已就绪。添加视频，选择输出设置后开始转换。"));
    }
}

void MainWindow::showProgress(quint64 id, int percent, int position, int total)
{
    // 由稳定标识取得当前名称，未知时长使用不确定进度。
    QString name;
    for (const ConversionTask &task : m_controller->tasks()) {
        // 匹配当前任务名称，过期标识不会引用其他项。
        if (task.id == id) {
            name = QFileInfo(task.input).fileName();
            break;
        }
    }
    m_ui->currentLabel->setText(
        QStringLiteral("当前：%1   ·   第 %2 / %3 项").arg(name).arg(position).arg(total));
    m_ui->progressBar->setRange(0, percent < 0 ? 0 : 100);
    m_ui->progressBar->setValue(std::max(0, percent));
}

void MainWindow::showNotice(const QString &text)
{
    // 非模态提示保持事件循环可响应，详细全文保留在日志或项目提示中。
    m_ui->noticeLabel->setText(text.left(600));
    m_ui->noticeLabel->setToolTip(text);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // 转换锁与本地 URL 检查共同防止目录、网页或远程链接进入队列。
    if (m_controller->batchActive() || m_closing || !event->mimeData()->hasUrls()) {
        return;
    }
    for (const QUrl &url : event->mimeData()->urls()) {
        // 任一本地文件允许接收拖入，目录随后由控制层可见地拒绝。
        if (url.isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    // 锁定状态双重检查，防止拖入与批次开始发生在相邻事件。
    if (m_controller->batchActive() || m_closing) {
        return;
    }
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        // 网络 URL 不访问，也不下载作为本地输入。
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }
    m_controller->addFiles(paths);
    event->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 即使代码直接要求关闭主窗口，也须先结束当前模态预览。
    if (m_controller->previewActive()) {
        event->ignore();
        return;
    }
    // 停止清理已完成，或无活动工作时可以立即关闭。
    if (m_allowClose || !m_controller->hasActiveWork()) {
        m_controller->savePreferences(selectedSettings());
        event->accept();
        return;
    }
    event->ignore();
    // 已请求退出后重复关闭不再次弹框。
    if (m_closing) {
        return;
    }
    QMessageBox question(QMessageBox::Question, QStringLiteral("视频处理仍在进行"),
                         QStringLiteral("继续转换，还是停止任务并退出？临时输出会在停止后清理。"),
                         QMessageBox::NoButton, this);
    auto *keep = question.addButton(QStringLiteral("继续转换"), QMessageBox::RejectRole);
    auto *exit = question.addButton(QStringLiteral("停止并退出"), QMessageBox::DestructiveRole);
    question.setDefaultButton(keep);
    question.exec();
    // 用户授权停止后保持对象存活，等待 shutdownReady 再触发实际关闭。
    if (question.clickedButton() == exit) {
        m_controller->savePreferences(selectedSettings());
        m_closing = true;
        refreshState();
        m_controller->requestShutdown();
    }
}

void MainWindow::openPreview()
{
    // 两个入口共享单选与关闭状态检查，避免选区落到任意一个多选任务。
    const auto selected = m_ui->videoList->selectedItems();
    if (!m_preview || m_closing || selected.size() != 1) {
        showNotice(QStringLiteral("请先选择一个视频再打开预览。"));
        return;
    }
    QString error;
    const quint64 id = selected.first()->data(Qt::UserRole).toULongLong();
    // 控制层再次检查批次、任务和原文件访问，拒绝时不创建窗口。
    if (!m_preview->open(id, &error)) {
        showNotice(error);
        return;
    }
    // 显示表面由入口绑定，窗口不调用播放服务；模态循环期间主窗口操作被锁定。
    PreviewDialog dialog(m_preview, this);
    emit videoOutputChanged(dialog.videoWidget());
    dialog.exec();
    // 所有退出方式先卸载媒体，再解除表面引用，随后允许销毁显示控件。
    m_preview->close();
    emit videoOutputChanged(nullptr);
    refreshState();
}
}
