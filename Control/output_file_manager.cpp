// 文件：output_file_manager.cpp
// 层：Control
// 作用：分配临时路径，检查写权限并安全提交结果。
// 说明：Windows MoveFile 不回退复制，避免跨路径或竞争时覆盖已有文件。
#include "output_file_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QUuid>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace videoconversion {
bool OutputFileManager::prepare(const QString &input, const QString &directory, Container container,
                                QString *error)
{
    // 尚未结算的当前事务不能被新的准备覆盖。
    if (!m_temporary.isEmpty()) {
        *error = QStringLiteral("上次文件事务尚未结算：%1").arg(m_temporary);
        return false;
    }
    // 真实源文件与输出目录必须有效，目录不存在时创建。
    const QFileInfo source(input);
    if (!source.isFile() || !source.isReadable() || directory.trimmed().isEmpty() ||
        !QDir().mkpath(directory)) {
        *error = QStringLiteral("源文件不可读或输出目录无法创建。");
        return false;
    }
    m_directory = QDir(directory).absolutePath();
    // 用实际临时文件打开测试写权限，不依赖 isWritable 推断 ACL。
    QTemporaryFile writable(QDir(m_directory).filePath(QStringLiteral(".vc-write-XXXXXX")));
    if (!writable.open()) {
        *error = QStringLiteral("输出目录不可写：%1").arg(m_directory);
        return false;
    }
    // UUID 临时路径只归本任务所有，编码器 -n 可防止极小概率外部竞争。
    m_extension = extensionFor(container);
    m_stem = source.completeBaseName() + QStringLiteral("_converted");
    m_temporary =
        QDir(m_directory)
            .filePath(QStringLiteral(".vc-%1.%2")
                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces), m_extension));
    error->clear();
    return true;
}

QString OutputFileManager::temporaryPath() const { return m_temporary; }

bool OutputFileManager::commit(QString *finalPath, QString *error)
{
    // 空或缺失输出绝不提交，即使外部进程退出码为零。
    if (m_temporary.isEmpty() || QFileInfo(m_temporary).size() <= 0) {
        *error = QStringLiteral("转换没有生成有效的非空输出。");
        return false;
    }
    // 同目录分配新名字；遇到竞争仅重试名字，不复制或覆盖。
    for (int suffix = 0; suffix < 100000; ++suffix) {
        const QString numbered = suffix == 0 ? m_stem : m_stem + QStringLiteral("_%1").arg(suffix);
        const QString candidate =
            QDir(m_directory).filePath(numbered + QLatin1Char('.') + m_extension);
        // 已有结果先跳过，最终仍由原子重命名检查竞争。
        if (QFileInfo::exists(candidate)) {
            continue;
        }
#ifdef Q_OS_WIN
        // Qt UTF-16 到 Win32 宽字符串的边界转换；不转换所有权，也不修改缓冲区。
        const bool moved = MoveFileW(reinterpret_cast<LPCWSTR>(m_temporary.utf16()),
                                     reinterpret_cast<LPCWSTR>(candidate.utf16())) != 0;
#else
        const bool moved = QFile::rename(m_temporary, candidate);
#endif
        // 提交成功后放弃临时所有权，后续取消不能删除正式结果。
        if (moved) {
            m_temporary.clear();
            *finalPath = candidate;
            error->clear();
            return true;
        }
        // 不是重名竞争的失败停止重试，保留临时路径供清理。
        if (!QFileInfo::exists(candidate)) {
            *error = QStringLiteral("无法提交结果到：%1").arg(candidate);
            return false;
        }
    }
    *error = QStringLiteral("输出目录重名过多，无法分配结果文件名。");
    return false;
}

bool OutputFileManager::cleanup(QString *error)
{
    // 清理仅覆盖本任务拥有的临时路径；未存在的临时文件也视为已清理。
    if (!m_temporary.isEmpty() && QFileInfo::exists(m_temporary) && !QFile::remove(m_temporary)) {
        *error = QStringLiteral("临时文件清理失败，残留位置：%1").arg(m_temporary);
        // 保留残留清单和错误位置，释放当前槽以免坏文件阻断后续项。
        m_residuals.append(m_temporary);
        m_temporary.clear();
        return false;
    }
    // 已删除或从未创建时释放所有权，不影响正式结果。
    m_temporary.clear();
    error->clear();
    return true;
}

QStringList OutputFileManager::residualPaths() const { return m_residuals; }
}
