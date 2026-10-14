// 文件：settings_store.h
// 层：Control
// 作用：加载与保存首版偏好。
// 说明：不持久化任务历史；非法枚举回退默认并通知调用方。
#ifndef VIDEOCONVERSION_SETTINGS_STORE_H
#define VIDEOCONVERSION_SETTINGS_STORE_H

#include <QSettings>

#include "conversion_types.h"

namespace videoconversion {
class SettingsStore
{
   public:
    // 通过应用组织与名称选择设置位置。
    SettingsStore();
    // 可指定独立 INI，用于可重复验证，避免污染用户偏好。
    explicit SettingsStore(const QString &iniPath);
    // 返回有效设置，warnings 接收无效偏好或读取错误提示。
    ConversionSettings load(QString *warnings);
    // 同步写出四项偏好，失败返回具体错误。
    bool save(const ConversionSettings &settings, QString *error);

   private:
    QSettings m_settings;
};
}
#endif
