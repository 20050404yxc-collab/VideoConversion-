// 文件：main_window.h
// 层：Application
// 作用：展示视频列表、统一输出设置与任务结果。
// 说明：窗口只调用控制层，不构造工具参数或直接管理进程。
#ifndef VIDEOCONVERSION_MAIN_WINDOW_H
#define VIDEOCONVERSION_MAIN_WINDOW_H

#include <memory>

#include <QMainWindow>
#include <QPixmap>

#include "conversion_controller.h"
#include "preview_controller.h"

class QWidget;

namespace Ui {
class MainWindow;
}

namespace videoconversion {
class MainWindow : public QMainWindow
{
    Q_OBJECT
   public:
    // 控制器为非拥有引用，生命周期由入口保证。
    explicit MainWindow(ConversionController *controller, PreviewController *preview = nullptr,
                        QWidget *parent = nullptr);
    // 释放 UI 包装对象，控件由 QMainWindow 父子关系释放。
    ~MainWindow() override;
   signals:
    // 入口负责将当前显示表面绑定到底层播放器；空指针解除绑定。
    void videoOutputChanged(QWidget *output);

   protected:
    // 活动工作必须经过询问与异步停止后才能退出。
    void closeEvent(QCloseEvent *event) override;
    // 仅接收本地文件 URL，转换期间禁止接受导入。
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

   private:
    // 连接操作按钮与状态通知，所有 UI 更新都在 GUI 线程。
    void connectActions();
    void connectPresentation();
    // 重建展示并保留多选与滚动位置，不改变控制层顺序。
    void refreshTasks();
    // 根据批次和探测状态更新编辑锁与按钮。
    void refreshState();
    // 从窗口获取独立值设置，开始时由控制器取得快照。
    ConversionSettings selectedSettings() const;
    // 初始化偏好和轻量样式，在连用户改动信号前调用。
    void setupPresentation();
    // 更新当前项的进度与批次位置。
    void showProgress(quint64 id, int percent, int position, int total);
    // 非模态错误提示不阻塞批次的失败继续流程。
    void showNotice(const QString &text);
    // 单选与双击共用入口，模态窗口关闭后释放表面引用。
    void openPreview();

    std::unique_ptr<Ui::MainWindow> m_ui;
    ConversionController *m_controller;
    PreviewController *m_preview;
    QPixmap m_placeholder;
    bool m_closing = false;
    bool m_allowClose = false;
};
}
#endif
