// 文件：preview_dialog.cpp
// 层：Application
// 作用：连接播放器展示、完整毫秒输入及保存取消交互。
// 说明：草稿仅在窗口内生效，关闭前由控制器释放媒体。
#include "preview_dialog.h"

#include <cmath>

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>

#include "ui_preview_dialog.h"

namespace videoconversion {
namespace preview_presentation {
// 严格接受完整时分秒及三位毫秒；避免自动截断和非法值归零。
bool parseTime(const QString &text, qint64 *milliseconds)
{
    // 小时限制九位以确保后续乘法在 qint64 范围内。
    const QRegularExpression expression(
        QStringLiteral("^(\\d{1,9}):([0-5]\\d):([0-5]\\d)\\.(\\d{3})$"));
    const auto match = expression.match(text.trimmed());
    // 格式不符保持原输出值不变，调用方展示错误。
    if (!match.hasMatch()) {
        return false;
    }
    // 已校验的各段转换为毫秒，成功时才写入输出。
    *milliseconds = ((match.captured(1).toLongLong() * 60 + match.captured(2).toInt()) * 60 +
                     match.captured(3).toInt()) *
                        1000 +
                    match.captured(4).toInt();
    return true;
}
}

QString rangeTime(qint64 milliseconds)
{
    // 非负时间以完整小时数显示，长视频不在一天处回绕。
    const qint64 value = qMax<qint64>(0, milliseconds);
    return QStringLiteral("%1:%2:%3.%4")
        .arg(value / 3600000, 2, 10, QLatin1Char('0'))
        .arg(value / 60000 % 60, 2, 10, QLatin1Char('0'))
        .arg(value / 1000 % 60, 2, 10, QLatin1Char('0'))
        .arg(value % 1000, 3, 10, QLatin1Char('0'));
}
QString rangeDescription(const MediaRange &range)
{
    // 整段不展示虚构终点，片段显示完整边界和目标时长。
    if (!range.enabled) {
        return QStringLiteral("整段");
    }
    return QStringLiteral("%1 ～ %2 · %3 秒")
        .arg(rangeTime(range.startMs), rangeTime(range.endMs))
        .arg((range.endMs - range.startMs) / 1000.0, 0, 'f', 3);
}

PreviewDialog::PreviewDialog(PreviewController *controller, QWidget *parent)
    : QDialog(parent), m_ui(new Ui::PreviewDialog), m_controller(controller)
{
    // 草稿从会话值副本初始化，显示控件归窗口所有。
    m_ui->setupUi(this);
    setModal(true);
    setWindowTitle(
        QStringLiteral("视频预览：%1").arg(QFileInfo(controller->task().input).fileName()));
    m_ui->videoWidget->setAttribute(Qt::WA_NativeWindow);
    m_ui->videoWidget->setStyleSheet(QStringLiteral("background: #111827;"));
    m_ui->buttonBox->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    // 保存范围与全长分离，未知时长不构造终点。
    const ConversionTask task = controller->task();
    m_whole = !task.range.enabled;
    m_ui->startEdit->setText(rangeTime(task.range.enabled ? task.range.startMs : 0));
    m_ui->endEdit->setText(task.media.duration > 0
                               ? rangeTime(task.range.enabled
                                               ? task.range.endMs
                                               : std::llround(task.media.duration * 1000))
                               : QString());
    connectActions();
    refresh();
}
PreviewDialog::~PreviewDialog() { m_controller->close(); }
QWidget *PreviewDialog::videoWidget() const { return m_ui->videoWidget; }

void PreviewDialog::done(int result)
{
    // 媒体先停止卸载，QDialog 关闭后不遗留声音或文件句柄。
    m_controller->close();
    QDialog::done(result);
}

bool PreviewDialog::draft(MediaRange *range, QString *error) const
{
    // 时间必须完整合法，即使默认整段也不能静默忽略用户已改坏的输入。
    MediaRange value;
    value.enabled = !m_whole;
    if (!preview_presentation::parseTime(m_ui->startEdit->text(), &value.startMs) ||
        !preview_presentation::parseTime(m_ui->endEdit->text(), &value.endMs) ||
        value.startMs < 0 || value.startMs >= value.endMs ||
        value.endMs > std::llround(m_controller->task().media.duration * 1000)) {
        *error = QStringLiteral("请输入 时:分:秒.毫秒，满足 0 ≤ 起点 < 终点 ≤ 输入时长。");
        return false;
    }
    // 合法草稿返回值副本，保存前仍由业务层复核能力与范围。
    *range = value;
    error->clear();
    return true;
}

bool PreviewDialog::eventFilter(QObject *watched, QEvent *event)
{
    // 只接管可定位进度条的鼠标交互，音量及其他控件保留原有事件处理。
    auto *slider = m_ui->positionSlider;
    if (watched != slider || !slider->isEnabled() ||
        (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseMove &&
         event->type() != QEvent::MouseButtonRelease)) {
        return QDialog::eventFilter(watched, event);
    }
    const auto *mouse = static_cast<QMouseEvent *>(event);
    // 非左键按下和未开始的拖动不改变播放位置。
    if ((event->type() == QEvent::MouseButtonPress && mouse->button() != Qt::LeftButton) ||
        (event->type() != QEvent::MouseButtonPress && !slider->isSliderDown()) ||
        (event->type() == QEvent::MouseButtonRelease && mouse->button() != Qt::LeftButton)) {
        return QDialog::eventFilter(watched, event);
    }
    // 按下立即锁住用户滑块位置，播放轮询在松开前不能覆盖它。
    if (event->type() == QEvent::MouseButtonPress) {
        slider->setSliderDown(true);
    }
    // 按当前样式的实际轨道与滑块尺寸换算，边缘拖出时钳制到视频首尾。
    QStyleOptionSlider option;
    option.initFrom(slider);
    option.orientation = slider->orientation();
    option.minimum = slider->minimum();
    option.maximum = slider->maximum();
    option.sliderPosition = slider->sliderPosition();
    option.sliderValue = slider->value();
    option.upsideDown = slider->invertedAppearance() != slider->isRightToLeft();
    const QRect groove = slider->style()->subControlRect(QStyle::CC_Slider, &option,
                                                         QStyle::SC_SliderGroove, slider);
    const QRect handle = slider->style()->subControlRect(QStyle::CC_Slider, &option,
                                                         QStyle::SC_SliderHandle, slider);
    const int span = qMax(1, groove.width() - handle.width());
    const int position = qBound(0, mouse->pos().x() - groove.left() - handle.width() / 2, span);
    slider->setSliderPosition(QStyle::sliderValueFromPosition(slider->minimum(), slider->maximum(),
                                                              position, span, option.upsideDown));
    // 最终位置先写入再松开，sliderReleased 仅提交这次定位且不自动播放。
    if (event->type() == QEvent::MouseButtonRelease) {
        slider->setSliderDown(false);
    }
    event->accept();
    return true;
}

void PreviewDialog::connectActions()
{
    // 播放和音量经预览控制器处理，窗口不调用底层播放器。
    connect(m_ui->playButton, &QPushButton::clicked, m_controller,
            &PreviewController::togglePlayback);
    connect(m_ui->volumeSlider, &QSlider::valueChanged, m_controller,
            &PreviewController::setVolume);
    connect(m_ui->positionSlider, &QSlider::sliderReleased, this, [this] {
        // 使用比例避免超长视频超过 QSlider 的 int 范围。
        m_controller->seek(m_controller->state().durationMs *
                           m_ui->positionSlider->sliderPosition() / 10000);
    });
    // 轨道点击可直接开始拖动；键盘及滚轮变更也提交定位，拖动中等松开再提交。
    m_ui->positionSlider->installEventFilter(this);
    connect(m_ui->positionSlider, &QSlider::valueChanged, this, [this](int value) {
        // 后端刷新已阻断信号，只有非拖动的用户变更在此请求定位。
        if (!m_ui->positionSlider->isSliderDown()) {
            m_controller->seek(m_controller->state().durationMs * value / 10000);
        }
    });
    // 任意时间编辑都会取消旧片段播放，草稿在保存前不进入队列。
    const auto edited = [this] {
        // 标记为用户片段，即使输入暂时非法也保留文字供修正。
        m_whole = false;
        m_controller->cancelSelected();
        refreshDraft();
    };
    connect(m_ui->startEdit, &QLineEdit::textEdited, this, edited);
    connect(m_ui->endEdit, &QLineEdit::textEdited, this, edited);
    connect(m_ui->setStartButton, &QPushButton::clicked, this, [this] {
        // 从当前位置取起点，后续统一校验，不自动修改终点掩盖错误。
        m_whole = false;
        m_controller->cancelSelected();
        m_ui->startEdit->setText(rangeTime(m_controller->state().positionMs));
        refreshDraft();
    });
    connect(m_ui->setEndButton, &QPushButton::clicked, this, [this] {
        // 从当前位置取终点，非法顺序只提示，不覆盖先前保存值。
        m_whole = false;
        m_controller->cancelSelected();
        m_ui->endEdit->setText(rangeTime(m_controller->state().positionMs));
        refreshDraft();
    });
    connect(m_ui->resetButton, &QPushButton::clicked, this, &PreviewDialog::resetDraft);
    connect(m_ui->selectedButton, &QPushButton::clicked, this, [this] {
        // 整段草稿也可临时作为全长选区播放，不改变保存语义。
        MediaRange range;
        QString error;
        if (draft(&range, &error)) {
            range.enabled = true;
            m_controller->playSelected(range, &error);
        }
        // 播放拒绝时保留草稿和窗口，明确展示原因。
        if (!error.isEmpty()) {
            m_ui->noticeLabel->setText(error);
        }
    });
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, [this] {
        // 保存成功才关闭；失败继续显示草稿，不清除上次范围。
        MediaRange range;
        QString error;
        if (draft(&range, &error) && m_controller->save(range, &error)) {
            accept();
        } else {
            // 控制层拒绝保留原值，用户可修正输入或取消。
            m_ui->noticeLabel->setText(error);
        }
    });
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_controller, &PreviewController::changed, this, &PreviewDialog::refresh);
    connect(m_controller, &PreviewController::notice, m_ui->noticeLabel, &QLabel::setText);
}

void PreviewDialog::resetDraft()
{
    // 恢复只改草稿，点保存之前不写入队列。
    m_controller->cancelSelected();
    m_whole = true;
    m_ui->startEdit->setText(rangeTime(0));
    m_ui->endEdit->setText(rangeTime(std::llround(m_controller->task().media.duration * 1000)));
    refreshDraft();
}

void PreviewDialog::refresh()
{
    // 后端通知只刷新显示和能力，不覆盖用户正在输入的草稿文字。
    const PlaybackState state = m_controller->state();
    m_ui->playButton->setText(state.playing ? QStringLiteral("暂停") : QStringLiteral("播放"));
    m_ui->playButton->setEnabled(state.loaded && !state.failed);
    m_ui->volumeSlider->setEnabled(state.loaded && !state.failed);
    m_ui->positionSlider->setEnabled(state.loaded && state.seekable && !state.failed);
    m_ui->positionLabel->setText(QStringLiteral("%1 / %2").arg(
        rangeTime(state.positionMs),
        state.durationMs > 0 ? rangeTime(state.durationMs) : QStringLiteral("未知")));
    // 正在拖动时保持用户位置，后端回报不抢走滑块。
    if (!m_ui->positionSlider->isSliderDown()) {
        const QSignalBlocker blocker(m_ui->positionSlider);
        m_ui->positionSlider->setValue(
            state.durationMs > 0 ? static_cast<int>(state.positionMs * 10000 / state.durationMs)
                                 : 0);
    }
    refreshDraft();
}

void PreviewDialog::refreshDraft()
{
    // 能力和输入分别校验，错误永远不悄悄替换为整段。
    MediaRange range;
    QString error;
    const bool valid = draft(&range, &error);
    const QString capability =
        m_controller->state().failed ? m_controller->state().error : m_controller->rangeError();
    const bool editable = capability.isEmpty();
    m_ui->startEdit->setEnabled(editable);
    m_ui->endEdit->setEnabled(editable);
    m_ui->setStartButton->setEnabled(editable);
    m_ui->setEndButton->setEnabled(editable);
    m_ui->resetButton->setEnabled(editable);
    m_ui->selectedButton->setEnabled(editable && valid);
    m_ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(editable && valid);
    // 终态或播放失败解释优先于时间输入错误，已保存范围仍在队列中。
    m_ui->noticeLabel->setText(
        !capability.isEmpty() ? capability
        : !valid              ? error
                              : QStringLiteral("时间以输入正常播放起点为零；保存后用于本项转换。"));
    m_ui->durationLabel->setText(valid ? QStringLiteral("已选：%1").arg(rangeDescription(range))
                                       : error);
    // 标记通过进度条样式中的两条色带绘制，颜色位置与毫秒比例一致。
    const double duration = m_controller->task().media.duration * 1000;
    if (valid && duration > 0) {
        // 探测秒数与毫秒四舍五入可能差不到 1ms，色带比例限制在控件范围。
        const double start = qBound(0.0, range.startMs / duration, 1.0);
        const double end = qBound(start, range.endMs / duration, 1.0);
        m_ui->positionSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { height: 8px; border-radius: 3px;"
                           "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                           "stop:0 #cbd5e1,stop:%1 #cbd5e1,stop:%2 #4f83ed,"
                           "stop:%3 #4f83ed,stop:%4 #cbd5e1,stop:1 #cbd5e1); }"
                           "QSlider::handle:horizontal { width: 12px; margin: -5px 0;"
                           "background: #1e3a8a; border-radius: 5px; }")
                .arg(qMax(0.0, start - 0.00001), 0, 'f', 6)
                .arg(start, 0, 'f', 6)
                .arg(end, 0, 'f', 6)
                .arg(qMin(1.0, end + 0.00001), 0, 'f', 6));
        m_ui->positionSlider->setToolTip(rangeDescription(range));
    } else {
        // 非法草稿移除旧标记，避免让用户误认已保存或正在播放的范围。
        m_ui->positionSlider->setStyleSheet(QString());
        m_ui->positionSlider->setToolTip(QString());
    }
}
}
