#pragma once

#include "aerosim/ui/intelligent/ResultDataSource.h"

#include <QWidget>

class QHBoxLayout;
class QLabel;
class QComboBox;
class QPushButton;
class QSplitter;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

namespace aerosim {

// "结果数据表": the table and the curves of one opened result file on one page.
//
// The tree filters the table. Selecting table rows chooses Y series, while the
// X selector offers the file's coordinates and numeric variables. The plot sits
// below the table so changing either selection needs no page switch.
class ResultDataTablePage final : public QWidget {
    Q_OBJECT
public:
    explicit ResultDataTablePage(QWidget *parent = nullptr);

    // Not owned. A null source leaves the page in its empty state, which is
    // exactly what a build without the visualization module shows.
    void setDataSource(ResultDataSource *source);
    ResultDataSource *dataSource() const { return source_; }

    // Takes ownership on success. Returns false when a curve view is already
    // installed, when the widget is null, or when it does not live in this
    // widget's thread; mirrors ComputeMonitorPage::insertCurveWidget.
    bool setCurveView(ResultCurveView *view);
    ResultCurveView *curveView() const { return curve_; }

    // Reads a file through the installed source. Public so a test can drive the
    // page without a native file dialog.
    bool loadFile(const QString &path, QString *error);
    void clearData();

    const ResultTable &table() const { return table_; }
    // Keys of the ticked variables, in tree order.
    QStringList selectedKeys() const;

    // Exposed for tests and for the assembly layer that wants to describe the
    // page; never null after construction.
    QTableWidget *tableWidget() const { return tableWidget_; }
    QTreeWidget *variableTree() const { return tree_; }
    QLabel *statusLabel() const { return status_; }

signals:
    void fileLoaded(const QString &path);

private:
    void buildUi();
    void populateTree();
    void populateTable();
    void populateAxes();
    QStringList curveKeys() const;
    void updateCurveView();
    void updateStatus();
    void chooseFile();

    ResultDataSource *source_ = nullptr;
    ResultCurveView *curve_ = nullptr;
    ResultTable table_;

    QPushButton *openButton_ = nullptr;
    QLabel *sourceLabel_ = nullptr;
    QLabel *axisLabelView_ = nullptr;
    QComboBox *xAxis_ = nullptr;
    QLabel *status_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QTableWidget *tableWidget_ = nullptr;
    QWidget *curveHost_ = nullptr;
    QLabel *curveHint_ = nullptr;
    QHBoxLayout *contentLayout_ = nullptr;
    QVBoxLayout *curveLayout_ = nullptr;
};

} // namespace aerosim
