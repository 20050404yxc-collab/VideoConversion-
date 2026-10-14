// 文件：main.cpp
// 层：Application
// 作用：装配服务、控制器和主窗口，启动事件循环。
// 说明：按逆序销毁窗口与业务对象，工具目录可用于本地开发配置。
#include <QApplication>
#include <QDir>
#include <QTimer>

#include "conversion_controller.h"
#include "ffmpeg_service.h"
#include "main_window.h"
#include "settings_store.h"

// 全局入口仅做运行装配，产品类型位于 videoconversion 命名空间。
int main(int argc, char *argv[])
{
    // 在 QApplication 创建前启用高 DPI，避免系统缩放下控件过小。
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("VideoConversion"));
    QCoreApplication::setApplicationName(QStringLiteral("VideoConversion"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    // 依赖对象在栈上有确定生命周期；发布时默认使用程序旁的 tools。
    const QString tools = qEnvironmentVariable("VIDEOCONVERSION_TOOLS");
    videoconversion::FfmpegService service(tools);
    videoconversion::SettingsStore store;
    videoconversion::ConversionController controller(&service, &store);
    videoconversion::MainWindow window(&controller);
    window.show();
    // 事件循环开始后再检查工具，使启动错误可展示在已创建窗口中。
    QTimer::singleShot(0, &controller, &videoconversion::ConversionController::initialize);
    return application.exec();
}
