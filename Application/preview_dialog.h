// 文件：preview_dialog.h
// 层：Application
// 作用：展示视频预览与事务式片段草稿编辑。
// 说明：播放与保存只调用 Control，视频表面由入口绑定。
#ifndef VIDEOCONVERSION_PREVIEW_DIALOG_H
#define VIDEOCONVERSION_PREVIEW_DIALOG_H

#include <memory>

#include <QDialog>

#include "preview_controller.h"

class QWidget;
namespace Ui {
class PreviewDialog;
}
namespace videoconversion {
class PreviewDialog : public QDialog
{
    Q_OBJECT
   public:
    // 从当前预览会话初始化独立草稿与模态窗口。
    explicit PreviewDialog(PreviewController *controller, QWidget *parent = nullptr);
    // 异常销毁路径也先结束媒体会话。
    ~PreviewDialog() override;
    // 提供入口装配使用的显示表面，所有权仍归窗口。
    QWidget *videoWidget() const;

   protected:
    // 保存、取消、Esc 和关闭均先释放媒体，随后退出模态循环。
    void done(int result) override;
    // 将进度轨道上的左键点击和拖动统一为直接定位，其他事件保留默认行为。
    bool eventFilter(QObject *watched, QEvent *event) override;

   private:
    // 严格解析小时/分钟/秒/毫秒，非法输入保留原保存值。
    bool draft(MediaRange *range, QString *error) const;
    // 将编辑与播放操作连接到会话控制和草稿校验。
    void connectActions();
    // 刷新播放状态和资格，保留用户正在编辑的文字。
    void refresh();
    // 校验草稿并更新保存按钮、提示和时间标记。
    void refreshDraft();
    // 恢复整段草稿，保存前不修改任务范围。
    void resetDraft();
    std::unique_ptr<Ui::PreviewDialog> m_ui;
    PreviewController *m_controller;
    bool m_whole = true;
};
// 显示无限制小时数和完整毫秒，不使用会在 24 小时回绕的 QTime。
QString rangeTime(qint64 milliseconds);
// 主列表与提示共享完整选区文案。
QString rangeDescription(const MediaRange &range);
}
#endif
