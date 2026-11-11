#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace aerosim {

// One numeric column of a result file, as the two result pages present it.
struct ResultVariable {
    QString key;                  // canonical, trimmed name, e.g. "CD"
    QString unit;                 // empty when the file says nothing
    bool isResidual = false;      // drives the tree grouping and the axis hint
};

struct ResultAxis {
    QString label;
    QVector<double> values;
};

// One opened result file: a shared horizontal axis plus, for every variable, its
// whole vector of samples.
//
// The storage is column-major because that is how the file is laid out and how
// the reader produces it, but the pages read it row-wise -- one row is one
// variable carrying all of its samples -- which is the orientation the result
// table was asked for and the transpose of the file.
struct ResultTable {
    QString sourcePath;
    QString datasetId;
    QString axisLabel;                    // e.g. "迭代步 (Inner_Iter)"
    QVector<double> axisValues;           // X coordinate shared by every variable
    QVector<ResultAxis> alternativeAxes; // independent coordinates present in the file
    QVector<ResultVariable> variables;
    QHash<QString, QVector<double>> samples;   // key -> one value per axis point
    QStringList warnings;

    bool isEmpty() const { return variables.isEmpty() || axisValues.isEmpty(); }
    int sampleCount() const { return axisValues.size(); }

    void clear()
    {
        sourcePath.clear();
        datasetId.clear();
        axisLabel.clear();
        axisValues.clear();
        alternativeAxes.clear();
        variables.clear();
        samples.clear();
        warnings.clear();
    }
};

// Port that turns a file on disk into a ResultTable.
//
// The pages stay free of both the monitor reader and the charting library on
// purpose. Those live under modules/visualization, which a headless build does
// not configure at all, and the workbench still has to build, lay out and test
// these pages without it. The assembly layer injects the concrete reader.
class ResultDataSource {
public:
    virtual ~ResultDataSource() = default;

    // Reads one file. Returns false and fills *error when the file cannot be
    // read; *table is left untouched in that case so a failed open keeps the
    // previously loaded result on screen.
    virtual bool load(const QString &path, ResultTable *table, QString *error) = 0;

    // QFileDialog filter string, e.g. "监控历史 (*.csv *.dat *.tec *.txt)".
    virtual QString fileFilter() const = 0;
};

// Port for the curve half of the merged page.
//
// Same reasoning as ResultDataSource: the page never plots, so it never links
// Qwt. replaceSeries hands over the whole table plus the keys the user selected,
// in tree order, and the implementation decides how to group and draw them.
class ResultCurveView : public QWidget {
public:
    using QWidget::QWidget;
    virtual void replaceSeries(const ResultTable &table, const QStringList &keys,
                               const QString &axisLabel) = 0;
    virtual void clearSeries() = 0;

    // Optional widget for the page to lay out beside the data and plot.
    // The curve view manages its lifetime after the page reparents it.
    virtual QWidget *settingsPanel() const { return nullptr; }
};

} // namespace aerosim
