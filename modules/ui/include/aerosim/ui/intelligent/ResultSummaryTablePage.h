#pragma once

#include "aerosim/ui/intelligent/ResultDataSource.h"
#include "aerosim/ui/intelligent/CsvResultReader.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace aerosim {
class ResultForecastPanel;

// Displays CSV fields, optional wall comparisons, or history statistics.
class ResultSummaryTablePage final : public QWidget {
    Q_OBJECT
public:
    explicit ResultSummaryTablePage(QWidget *parent = nullptr);

    // Not owned; see ResultDataTablePage::setDataSource.
    void setDataSource(ResultDataSource *source);
    ResultDataSource *dataSource() const { return source_; }

    bool loadFile(const QString &path, QString *error);
    bool loadCsvFiles(const QStringList &paths, QString *error);
    // Compatibility entry point for existing callers.
    bool loadForceFiles(const QStringList &paths, QString *error);
    void clearData();

    const ResultTable &table() const { return table_; }
    QTableWidget *tableWidget() const { return tableWidget_; }
    QLabel *statusLabel() const { return status_; }

signals:
    void fileLoaded(const QString &path);

private:
    struct ForceCase {
        QString name;
        QString reference;
        QString mach;
        QString beta;
        QString alpha;
        QHash<QString, QHash<QString, QString>> valuesByWall;
    };

    void buildUi();
    void populateTable();
    void populateForceTable();
    void populateCsvTable();
    void showCsvView();
    bool loadHistoryFile(const QString &path, QString *error);
    void updateStatus();
    void chooseFile();
    void updateAnalysisData();

    ResultDataSource *source_ = nullptr;
    ResultTable table_;
    QVector<ForceCase> forceCases_;
    QStringList forceWalls_;
    QStringList csvColumns_;
    QStringList comparisonFields_;
    QVector<CsvResultTable> csvTables_;
    QWidget *forceControls_ = nullptr;
    QComboBox *coefficient_ = nullptr;
    QComboBox *csvView_ = nullptr;

    QPushButton *openButton_ = nullptr;
    QLabel *sourceLabel_ = nullptr;
    QLabel *status_ = nullptr;
    QTableWidget *tableWidget_ = nullptr;
    ResultForecastPanel *forecastPanel_ = nullptr;
};

} // namespace aerosim
