#pragma once

#include "aerosim/ui/intelligent/ResultDataSource.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace aerosim {

// Displays wall force coefficients or statistics for a history file.
class ResultSummaryTablePage final : public QWidget {
    Q_OBJECT
public:
    explicit ResultSummaryTablePage(QWidget *parent = nullptr);

    // Not owned; see ResultDataTablePage::setDataSource.
    void setDataSource(ResultDataSource *source);
    ResultDataSource *dataSource() const { return source_; }

    bool loadFile(const QString &path, QString *error);
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
        QHash<QString, QStringList> coefficientsByWall;
    };

    void buildUi();
    void populateTable();
    void populateForceTable();
    void updateStatus();
    void chooseFile();

    ResultDataSource *source_ = nullptr;
    ResultTable table_;
    QVector<ForceCase> forceCases_;
    QStringList forceWalls_;
    QWidget *forceControls_ = nullptr;
    QComboBox *coefficient_ = nullptr;

    QPushButton *openButton_ = nullptr;
    QLabel *sourceLabel_ = nullptr;
    QLabel *status_ = nullptr;
    QTableWidget *tableWidget_ = nullptr;
};

} // namespace aerosim
