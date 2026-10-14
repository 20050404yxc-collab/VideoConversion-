// 文件：settings_store.cpp
// 层：Control
// 作用：校验并保存输出选项。
// 说明：使用 QSettings，不恢复上次任务队列。
#include "settings_store.h"

#include <QDir>
#include <QStandardPaths>

namespace videoconversion {
namespace preferences {
// 超出取值范围或不是整数的保存值回退到默认值并披露。
int validated(QSettings *store, const QString &key, int fallback, int maximum, QString *warnings)
{
    // 整数校验防止旧版本或人为编辑配置带来无效枚举。
    bool valid = false;
    const int value = store->value(key, fallback).toInt(&valid);
    if (!valid || value < 0 || value > maximum) {
        *warnings += QStringLiteral("设置 %1 无效，已恢复默认值。\n").arg(key);
        return fallback;
    }
    return value;
}
}
SettingsStore::SettingsStore() = default;
SettingsStore::SettingsStore(const QString &iniPath) : m_settings(iniPath, QSettings::IniFormat) {}

ConversionSettings SettingsStore::load(QString *warnings)
{
    // 首次使用系统视频目录下的独立子目录，视频目录不可用时回退用户目录。
    warnings->clear();
    QString videos = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (videos.isEmpty()) {
        videos = QDir::homePath();
    }
    ConversionSettings settings;
    settings.outputDirectory = m_settings
                                   .value(QStringLiteral("outputDirectory"),
                                          QDir(videos).filePath(QStringLiteral("VideoConversion")))
                                   .toString();
    // 空目录无意义，回退到首次默认目录。
    if (settings.outputDirectory.trimmed().isEmpty()) {
        settings.outputDirectory = QDir(videos).filePath(QStringLiteral("VideoConversion"));
        *warnings += QStringLiteral("保存的输出目录为空，已恢复默认值。\n");
    }
    settings.encoding.container = static_cast<Container>(
        preferences::validated(&m_settings, QStringLiteral("container"), 0, 1, warnings));
    settings.encoding.quality = static_cast<Quality>(
        preferences::validated(&m_settings, QStringLiteral("quality"), 1, 2, warnings));
    settings.encoding.resolution = static_cast<Resolution>(
        preferences::validated(&m_settings, QStringLiteral("resolution"), 0, 2, warnings));
    // 配置损坏或不可读需明确提醒，不能表示设置已正确恢复。
    if (m_settings.status() != QSettings::NoError) {
        *warnings += QStringLiteral("偏好配置读取失败，请检查设置文件权限或格式。\n");
    }
    return settings;
}

bool SettingsStore::save(const ConversionSettings &settings, QString *error)
{
    // 只写持久偏好，队列、日志和进度都留在本次会话。
    m_settings.setValue(QStringLiteral("outputDirectory"), settings.outputDirectory);
    m_settings.setValue(QStringLiteral("container"), static_cast<int>(settings.encoding.container));
    m_settings.setValue(QStringLiteral("quality"), static_cast<int>(settings.encoding.quality));
    m_settings.setValue(QStringLiteral("resolution"),
                        static_cast<int>(settings.encoding.resolution));
    m_settings.sync();
    // 写入失败使界面可披露，不能宣称下次启动一定恢复。
    if (m_settings.status() != QSettings::NoError) {
        *error = QStringLiteral("无法保存输出偏好，请检查用户配置目录权限。");
        return false;
    }
    error->clear();
    return true;
}
}
