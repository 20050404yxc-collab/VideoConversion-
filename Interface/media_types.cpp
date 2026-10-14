// 文件：media_types.cpp
// 层：Interface
// 作用：将 ffprobe JSON 转为显示方向与可编码的媒体信息。
// 说明：异常字段必须报错或保留未知，不把缺失时长当作零。
#include "media_types.h"

#include <cmath>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

namespace videoconversion {
namespace media_parsing {

// 有限且正的秒数才可作为输入时长。
double durationOf(const QJsonValue &value)
{
    // ffprobe 常将数值编码为字符串，兼容原生数字字段。
    bool valid = false;
    const double duration = value.isDouble() ? value.toDouble() : value.toString().toDouble(&valid);
    // 无效、无限和非正值统一视为未知。
    if ((!value.isDouble() && !valid) || !std::isfinite(duration) || duration <= 0) {
        return -1.0;
    }
    return duration;
}

// 检查显示方向、像素比例和首版色彩边界。
bool readVideo(const QJsonObject &stream, MediaInfo *info, QString *error)
{
    // 先读取原始流字段，后续转换按实际流索引映射。
    info->videoIndex = stream.value(QStringLiteral("index")).toInt(-1);
    info->videoCodec = stream.value(QStringLiteral("codec_name")).toString();
    info->codedSize = QSize(stream.value(QStringLiteral("width")).toInt(),
                            stream.value(QStringLiteral("height")).toInt());
    const QString transfer = stream.value(QStringLiteral("color_transfer")).toString();
    const QString primaries = stream.value(QStringLiteral("color_primaries")).toString();
    // 不进行未实现的 HDR 色调映射，避免宣告错误的色彩转换成功。
    if (transfer == QStringLiteral("smpte2084") || transfer == QStringLiteral("arib-std-b67") ||
        primaries == QStringLiteral("bt2020")) {
        *error = QStringLiteral("首版暂不支持 HDR/BT.2020 视频，请先转换为 SDR。");
        return false;
    }
    // 无意义或极端尺寸不交给缩放与编码器。
    if (info->videoIndex < 0 || info->codedSize.width() < 2 || info->codedSize.height() < 2 ||
        info->codedSize.width() > 32768 || info->codedSize.height() > 32768) {
        *error = QStringLiteral("视频流索引或尺寸无效。");
        return false;
    }
    // 有明确像素比例时进行严格解析；未指定保留方形像素默认值。
    const QString aspect = stream.value(QStringLiteral("sample_aspect_ratio")).toString();
    if (!aspect.isEmpty() && aspect != QStringLiteral("N/A") && aspect != QStringLiteral("0:1")) {
        const QStringList parts = aspect.split(QLatin1Char(':'));
        const double denominator = parts.size() == 2 ? parts.at(1).toDouble() : 0.0;
        info->sampleAspect = denominator > 0 ? parts.at(0).toDouble() / denominator : 0.0;
        // 错误比例不能默默改成方形像素。
        if (!std::isfinite(info->sampleAspect) || info->sampleAspect <= 0 ||
            info->sampleAspect > 100) {
            *error = QStringLiteral("视频像素比例无效或超出首版支持范围。");
            return false;
        }
    }
    // 新版探测结果优先使用旋转边数据，旧素材兼容 rotate 标签。
    double rotation = stream.value(QStringLiteral("tags"))
                          .toObject()
                          .value(QStringLiteral("rotate"))
                          .toString()
                          .toDouble();
    for (const QJsonValue &side : stream.value(QStringLiteral("side_data_list")).toArray()) {
        // 只有包含旋转值的边数据才会替换标签值。
        if (side.toObject().contains(QStringLiteral("rotation"))) {
            rotation = side.toObject().value(QStringLiteral("rotation")).toDouble();
        }
    }
    // 仅接受直角旋转；非直角的自动旋转会改变包围框，首版不承诺处理。
    if (!std::isfinite(rotation) || std::abs(rotation) > 3600 ||
        std::abs(rotation / 90.0 - std::round(rotation / 90.0)) > 0.001) {
        *error = QStringLiteral("首版仅支持 0/90/180/270 度的视频旋转。");
        return false;
    }
    info->rotation = static_cast<int>(std::round(rotation));
    const int displayWidth =
        static_cast<int>(std::round(info->codedSize.width() * info->sampleAspect));
    info->displaySize = QSize(displayWidth, info->codedSize.height());
    // 直角旋转交换显示轴，输出设置按此方向选取横屏或竖屏上限。
    if (std::abs(info->rotation) % 180 == 90) {
        info->displaySize.transpose();
    }
    // 像素比例导致的极端显示尺寸同样拒绝。
    if (info->displaySize.width() < 2 || info->displaySize.height() < 2 ||
        info->displaySize.width() > 32768 || info->displaySize.height() > 32768) {
        *error = QStringLiteral("视频显示尺寸超出首版支持范围。");
        return false;
    }
    return true;
}
}

bool parseMediaInfo(const QByteArray &json, MediaInfo *info, QString *error)
{
    // 校验 JSON 完整性，避免把工具错误输出当作可转换视频。
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("无法解析视频信息：%1").arg(parseError.errorString());
        return false;
    }
    // 每次重新初始化，禁止缺失字段沿用前一个文件的信息。
    *info = MediaInfo{};
    const QJsonObject root = document.object();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();
    info->duration = media_parsing::durationOf(format.value(QStringLiteral("duration")));
    info->container = format.value(QStringLiteral("format_name")).toString();
    // 依探测顺序选择实际视频，封面图与额外音轨不参与首版转换。
    for (const QJsonValue &value : root.value(QStringLiteral("streams")).toArray()) {
        const QJsonObject stream = value.toObject();
        const QString type = stream.value(QStringLiteral("codec_type")).toString();
        const bool attached = stream.value(QStringLiteral("disposition"))
                                  .toObject()
                                  .value(QStringLiteral("attached_pic"))
                                  .toInt() != 0;
        // 首路实际视频必须通过尺寸、比例和色彩检查。
        if (type == QStringLiteral("video") && !attached && info->videoIndex < 0) {
            // 首路实际视频不满足首版约束时保留错误并结束，不擅自切换其他流。
            if (!media_parsing::readVideo(stream, info, error)) {
                return false;
            }
            // 容器时长缺失时允许采用所选视频流时长。
            if (info->duration < 0) {
                info->duration =
                    media_parsing::durationOf(stream.value(QStringLiteral("duration")));
            }
        }
        // 音频只记录第一路，无音轨继续保持 -1。
        if (type == QStringLiteral("audio") && info->audioIndex < 0) {
            info->audioIndex = stream.value(QStringLiteral("index")).toInt(-1);
            info->audioCodec = stream.value(QStringLiteral("codec_name")).toString();
        }
    }
    // 没有实际视频的音频文件或封面媒体不能参与转换。
    if (info->videoIndex < 0) {
        *error = QStringLiteral("文件中没有可转换的视频流（封面图不算视频）。");
        return false;
    }
    error->clear();
    return true;
}
}
