// 文件：output_file_manager.h
// 层：Control
// 作用：管理单次转换拥有的临时文件和不覆盖提交。
// 说明：只删除本对象分配的临时文件，正式结果永不回滚。
#ifndef VIDEOCONVERSION_OUTPUT_FILE_MANAGER_H
#define VIDEOCONVERSION_OUTPUT_FILE_MANAGER_H

#include <QString>
#include <QStringList>

#include "ffmpeg_arguments.h"

namespace videoconversion {
class OutputFileManager
{
   public:
    // 验证输出目录可写并分配唯一临时路径；调用前应已清理上一任务。
    bool prepare(const QString &input, const QString &directory, Container container,
                 QString *error);
    // 获取本次独占临时路径，转换在该路径上使用 -n。
    QString temporaryPath() const;
    // 已验证结果通过同目录不覆盖重命名提交，重名时重新分配。
    bool commit(QString *finalPath, QString *error);
    // 仅在进程结束后调用；清理失败记录残留路径并释放当前槽，后续任务仍可运行。
    bool cleanup(QString *error);
    // 返回已经披露但未清理的本会话临时路径，不能视为正式转换结果。
    QStringList residualPaths() const;

   private:
    QString m_temporary;
    QString m_stem;
    QString m_extension;
    QString m_directory;
    QStringList m_residuals;
};
}
#endif
