#include "aerosim/ui/intelligent/MonitorResultDataSource.h"

#include "MonitorLogReader.h"
#include "MonitorSeriesModel.h"

#include <QFileInfo>

namespace aerosim {
namespace {

// The axis is the iteration step, which is what a solver history is indexed by.
// Time_Iter cannot be used: a steady run keeps it constant, and MonitorSeriesModel
// refuses it for exactly that reason.
QString axisLabelFor(const monitor::MonitorSeriesModel &model)
{
    return model.axisLabel(monitor::MonitorAxis::IterationStep);
}

} // namespace

bool MonitorResultDataSource::load(const QString &path, ResultTable *table, QString *error)
{
    if (table == nullptr) {
        if (error)
            *error = QStringLiteral(u"内部错误：未提供结果表。");
        return false;
    }
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral(u"未指定结果文件。");
        return false;
    }

    monitor::MonitorLog log;
    QString message;
    if (!monitor::MonitorLogReader::read(path, &log, &message)) {
        if (error)
            *error = message.isEmpty()
                         ? QStringLiteral(u"无法读取结果文件：%1").arg(path)
                         : message;
        return false;
    }

    monitor::MonitorSeriesModel model;
    if (!model.setLog(log, &message)) {
        if (error)
            *error = message.isEmpty()
                         ? QStringLiteral(u"文件里没有可绘制的列：%1").arg(path)
                         : message;
        return false;
    }

    ResultTable result;
    result.sourcePath = log.sourcePath.isEmpty() ? path : log.sourcePath;
    result.datasetId = log.datasetId;
    result.axisLabel = axisLabelFor(model);
    result.axisValues.reserve(log.iterationStep.size());
    for (qint64 step : log.iterationStep)
        result.axisValues.append(static_cast<double>(step));

    if (model.axisAvailable(monitor::MonitorAxis::TimeIndex)) {
        ResultAxis time;
        time.label = model.axisLabel(monitor::MonitorAxis::TimeIndex);
        for (qint64 value : log.timeIndex)
            time.values.append(static_cast<double>(value));
        result.alternativeAxes.append(time);
    }

    // seriesColumns() is the file's own column order, filtered down to the
    // numeric columns; iteration and time columns are excluded there, which is
    // what keeps them out of the variable tree.
    const QStringList keys = model.seriesColumns();
    result.variables.reserve(keys.size());
    for (const QString &key : keys) {
        ResultVariable variable;
        variable.key = key;
        variable.unit = model.unit(key);
        variable.isResidual = model.isResidual(key);
        result.variables.append(variable);
        result.samples.insert(key, log.values.value(key));
    }

    if (result.isEmpty()) {
        if (error)
            *error = QStringLiteral(u"文件里没有可用的采样数据：%1").arg(path);
        return false;
    }

    result.warnings = log.warnings;
    *table = result;
    if (error)
        error->clear();
    return true;
}

QString MonitorResultDataSource::fileFilter() const
{
    // Only the formats the reader actually parses. Adding *.plt here would offer
    // a binary Tecplot file the reader rejects.
    return QStringLiteral(u"监控历史 (*.csv *.dat *.tec *.txt);;"
                          u"CSV (*.csv);;"
                          u"Tecplot ASCII (*.dat *.tec);;"
                          u"所有文件 (*)");
}

} // namespace aerosim
