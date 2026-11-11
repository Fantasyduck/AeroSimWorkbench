#include "MonitorSeriesModel.h"

#include <cmath>

namespace aerosim {
namespace monitor {
namespace {

QString axisText(MonitorAxis axis)
{
    return axis == MonitorAxis::IterationStep ? QStringLiteral("迭代步") : QStringLiteral("时间");
}

bool allSame(const QVector<qint64> &values)
{
    if (values.size() < 2)
        return true;
    for (const qint64 value : values)
        if (value != values.first())
            return false;
    return true;
}

} // namespace

bool MonitorSeriesModel::setLog(const MonitorLog &log, QString *error)
{
    if (error)
        error->clear();
    valid_ = false;
    log_ = MonitorLog();

    const int rows = log.iterationStep.size();
    if (rows <= 0) {
        if (error)
            *error = QStringLiteral("监控日志没有数据行，无法建立曲线。");
        return false;
    }
    if (!log.timeIndex.isEmpty() && log.timeIndex.size() != rows) {
        if (error)
            *error = QStringLiteral("时间索引列长度与迭代步列不一致，文件可能已损坏。");
        return false;
    }
    for (auto it = log.values.constBegin(); it != log.values.constEnd(); ++it) {
        if (it.value().size() != rows) {
            if (error)
                *error = QStringLiteral("列 %1 的数据行数与迭代步不一致。").arg(it.key());
            return false;
        }
    }
    log_ = log;
    valid_ = true;
    return true;
}

bool MonitorSeriesModel::axisAvailable(MonitorAxis axis) const
{
    return axisAvailable(axis, nullptr);
}

bool MonitorSeriesModel::axisAvailable(MonitorAxis axis, QString *reason) const
{
    if (!valid_) {
        if (reason)
            *reason = QStringLiteral("尚未导入监控日志。");
        return false;
    }
    if (axis == MonitorAxis::TimeIndex) {
        if (log_.timeIndex.isEmpty()) {
            if (reason)
                *reason = QStringLiteral("该文件没有 Time_Iter 列，无法按时间索引显示。");
            return false;
        }
        if (allSame(log_.timeIndex)) {
            if (reason)
                *reason = QStringLiteral("该文件为定常推进，时间索引恒为 %1，不能作为横轴；请改用迭代步。")
                              .arg(log_.timeIndex.value(0));
            return false;
        }
        if (reason)
            reason->clear();
        return true;
    }
    if (reason)
        reason->clear();
    return !log_.iterationStep.isEmpty();
}

QString MonitorSeriesModel::axisLabel(MonitorAxis axis) const
{
    if (axis == MonitorAxis::TimeIndex) {
        if (log_.timeIndex.isEmpty())
            return axisText(axis);
        return QStringLiteral("时间索引 (Time_Iter)");
    }
    if (log_.outerIterationProgression > 0)
        return QStringLiteral("迭代步 (Outer_Iter)");
    if (log_.innerIterationProgression > 0)
        return QStringLiteral("迭代步 (Inner_Iter)");
    return QStringLiteral("迭代步");
}

QVector<QPointF> MonitorSeriesModel::points(const QString &columnKey, MonitorAxis axis) const
{
    QVector<QPointF> result;
    if (!valid_)
        return result;
    const auto it = log_.values.constFind(columnKey);
    if (it == log_.values.constEnd())
        return result;

    const QVector<double> &rows = it.value();
    const bool useTime = axis == MonitorAxis::TimeIndex;
    if (useTime && !axisAvailable(MonitorAxis::TimeIndex))
        return result;

    const QVector<qint64> &x = useTime ? log_.timeIndex : log_.iterationStep;
    result.reserve(rows.size());
    for (int index = 0; index < rows.size() && index < x.size(); ++index) {
        const double value = rows.at(index);
        if (!std::isfinite(value))
            continue;                     // keep the curve contiguous instead of plotting NaN
        result.append(QPointF(static_cast<double>(x.at(index)), value));
    }
    return result;
}

bool MonitorSeriesModel::isResidual(const QString &columnKey) const
{
    if (log_.values.contains(columnKey) && MonitorLogReader::looksLikeResidual(columnKey))
        return true;
    for (const MonitorColumn &column : log_.columns)
        if (column.key == columnKey)
            return column.isResidual;
    return MonitorLogReader::looksLikeResidual(columnKey);
}

QString MonitorSeriesModel::unit(const QString &columnKey) const
{
    for (const MonitorColumn &column : log_.columns)
        if (column.key == columnKey)
            return column.unit;
    return QString();
}

MonitorScale MonitorSeriesModel::suggestedScale(const QString &columnKey) const
{
    return isResidual(columnKey) ? MonitorScale::Logarithmic : MonitorScale::Linear;
}

MonitorScale MonitorSeriesModel::suggestedScale(const QStringList &columnKeys) const
{
    // A group is drawn on one axis, so it follows the residual rule as soon as any
    // member is a residual: that is the group whose dynamic range needs the log axis.
    for (const QString &key : columnKeys)
        if (isResidual(key))
            return MonitorScale::Logarithmic;
    return MonitorScale::Linear;
}

QStringList MonitorSeriesModel::residualColumns() const
{
    QStringList result;
    for (const MonitorColumn &column : log_.columns)
        if (!column.isAxis && column.isResidual)
            result.append(column.key);
    return result;
}

QStringList MonitorSeriesModel::coefficientColumns() const
{
    QStringList result;
    for (const MonitorColumn &column : log_.columns)
        if (!column.isAxis && !column.isResidual)
            result.append(column.key);
    return result;
}

QStringList MonitorSeriesModel::seriesColumns() const
{
    QStringList result;
    for (const MonitorColumn &column : log_.columns)
        if (!column.isAxis)
            result.append(column.key);
    return result;
}

} // namespace monitor
} // namespace aerosim
