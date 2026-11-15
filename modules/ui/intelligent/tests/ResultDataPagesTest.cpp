#include "aerosim/ui/intelligent/ResultDataTablePage.h"
#include "aerosim/ui/intelligent/ResultSummaryTablePage.h"

#include <QApplication>
#include <QHeaderView>
#include <QComboBox>
#include <QPushButton>
#include <QSplitter>
#include <QLabel>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTest>
#include <QTreeWidget>

#include <cmath>

namespace {

// A reader that answers from memory. The pages must not need a real file, the
// monitor reader, or the charting library to be exercised -- if they did, this
// test would only run in a build that has all three.
class StubSource final : public aerosim::ResultDataSource {
public:
    bool failNext = false;
    int loads = 0;

    bool load(const QString &path, aerosim::ResultTable *table, QString *error) override
    {
        ++loads;
        if (failNext) {
            if (error) *error = QStringLiteral("stub failure");
            return false;
        }
        aerosim::ResultTable loaded;
        loaded.sourcePath = path;
        loaded.datasetId = QStringLiteral("stub");
        loaded.axisLabel = QStringLiteral("迭代步");
        for (int i = 0; i < 4; ++i)
            loaded.axisValues.append(static_cast<double>(i * 10));

        aerosim::ResultVariable cd;
        cd.key = QStringLiteral("CD");
        cd.unit = QStringLiteral("-");
        loaded.variables.append(cd);
        loaded.samples.insert(cd.key, {0.08, 0.05, 0.03, 0.01});

        aerosim::ResultVariable rho;
        rho.key = QStringLiteral("rms[Rho]");
        rho.unit = QStringLiteral("-");
        rho.isResidual = true;
        loaded.variables.append(rho);
        loaded.samples.insert(rho.key, {-1.6, -3.0, -5.5, -8.0});

        *table = loaded;
        if (error) error->clear();
        return true;
    }

    QString fileFilter() const override { return QStringLiteral("stub (*.csv)"); }
};

// Records what the page handed over instead of drawing it.
class SpyCurveView final : public aerosim::ResultCurveView {
public:
    aerosim::ResultTable table;
    QStringList keys;
    QString axis;
    int replacements = 0;
    int clears = 0;

    void replaceSeries(const aerosim::ResultTable &t, const QStringList &k,
                       const QString &a) override
    {
        table = t;
        keys = k;
        axis = a;
        ++replacements;
    }
    void clearSeries() override { ++clears; }
};

} // namespace

class ResultDataPagesTest final : public QObject {
    Q_OBJECT
private slots:
    void emptyStateIsUsable();
    void tableIsTransposedToRowsPerVariable();
    void untickingAVariableDropsItsColumnAndItsCurve();
    void failedLoadKeepsThePreviousResult();
    void summaryReportsRangeAndLastValue();
    void curveSlotAcceptsExactlyOneView();
    void selectingColumnsDrawsImmediatelyBelowTable();
    void switchingXAxisUsesTheSelectedColumn();
    void clearingSelectionClearsCurves();
};

void ResultDataPagesTest::emptyStateIsUsable()
{
    aerosim::ResultDataTablePage page;
    QVERIFY(page.tableWidget());
    QCOMPARE(page.tableWidget()->rowCount(), 0);
    // No reader injected: the page must still build, show its hint and refuse to
    // pretend it can open something.
    QVERIFY(!page.statusLabel()->text().isEmpty());

    aerosim::ResultSummaryTablePage summary;
    QVERIFY(!summary.statusLabel()->text().isEmpty());
}

void ResultDataPagesTest::tableIsTransposedToRowsPerVariable()
{
    StubSource source;
    aerosim::ResultDataTablePage page;
    page.setDataSource(&source);
    QVERIFY(page.dataSource() == &source);

    QString error;
    QVERIFY2(page.loadFile(QStringLiteral("C:/tmp/stub.csv"), &error), qPrintable(error));
    QCOMPARE(source.loads, 1);

    auto *table = page.tableWidget();
    QCOMPARE(table->rowCount(), 4);
    QCOMPARE(table->columnCount(), 3);
    QCOMPARE(table->horizontalHeaderItem(1)->text(), QStringLiteral("CD"));
    QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("rms[Rho]"));
    QCOMPARE(table->item(3, 0)->text(), QStringLiteral("30"));
    QVERIFY(table->item(0, 1)->text().toDouble() > 0.07);
    QVERIFY(table->item(3, 1)->text().toDouble() < 0.02);

    // The tree groups residuals apart from everything else.
    QCOMPARE(page.variableTree()->topLevelItemCount(), 2);
    QCOMPARE(page.selectedKeys().size(), 2);
}

void ResultDataPagesTest::untickingAVariableDropsItsColumnAndItsCurve()
{
    StubSource source;
    aerosim::ResultDataTablePage page;
    // Heap-allocated on purpose: setCurveView takes ownership and reparents the
    // widget into the page, so a stack instance would be destroyed twice -- once
    // by the page and once by the scope exit -- and abort the whole test run.
    auto *curves = new SpyCurveView;
    page.setDataSource(&source);
    QVERIFY(page.setCurveView(curves));

    QString error;
    QVERIFY(page.loadFile(QStringLiteral("C:/tmp/stub.csv"), &error));
    QCOMPARE(curves->replacements, 0);
    page.tableWidget()->setRangeSelected(QTableWidgetSelectionRange(0, 1, 3, 2), true);
    QCOMPARE(curves->replacements, 1);
    QCOMPARE(curves->keys.size(), 2);

    // The residual lives in the second top-level group.
    QTreeWidgetItem *group = nullptr;
    for (int i = 0; i < page.variableTree()->topLevelItemCount(); ++i) {
        QTreeWidgetItem *candidate = page.variableTree()->topLevelItem(i);
        if (candidate->childCount() > 0
            && candidate->child(0)->data(0, Qt::UserRole).toString() == QStringLiteral("CD")) {
            group = candidate;
            break;
        }
    }
    QVERIFY(group);
    group->child(0)->setCheckState(0, Qt::Unchecked);

    QCOMPARE(page.selectedKeys().size(), 1);
    QCOMPARE(page.tableWidget()->rowCount(), 4);
    QCOMPARE(page.tableWidget()->columnCount(), 2);
    QCOMPARE(page.tableWidget()->horizontalHeaderItem(1)->text(), QStringLiteral("rms[Rho]"));
    // The curve view is told again, with one key instead of two.
    QCOMPARE(curves->replacements, 2);
    QCOMPARE(curves->keys.size(), 1);

    // Unticking everything clears the drawing rather than leaving the last one up.
    group->child(0)->setCheckState(0, Qt::Checked);
    for (int i = 0; i < page.variableTree()->topLevelItemCount(); ++i) {
        QTreeWidgetItem *top = page.variableTree()->topLevelItem(i);
        for (int j = 0; j < top->childCount(); ++j)
            top->child(j)->setCheckState(0, Qt::Unchecked);
    }
    QVERIFY(page.selectedKeys().isEmpty());
    QVERIFY(curves->clears > 0);
    QCOMPARE(page.tableWidget()->rowCount(), 0);
}

void ResultDataPagesTest::failedLoadKeepsThePreviousResult()
{
    StubSource source;
    aerosim::ResultDataTablePage page;
    page.setDataSource(&source);
    QString error;
    QVERIFY(page.loadFile(QStringLiteral("C:/tmp/first.csv"), &error));
    QCOMPARE(page.table().variables.size(), 2);

    source.failNext = true;
    QVERIFY(!page.loadFile(QStringLiteral("C:/tmp/second.csv"), &error));
    QCOMPARE(error, QStringLiteral("stub failure"));
    // A failed open preserves the table the user was reading.
    QCOMPARE(page.table().variables.size(), 2);
    QCOMPARE(page.tableWidget()->rowCount(), 4);
    QCOMPARE(page.tableWidget()->columnCount(), 3);

    // Clearing is explicit and does empty it.
    page.clearData();
    QVERIFY(page.table().isEmpty());
    QCOMPARE(page.tableWidget()->rowCount(), 0);
}

void ResultDataPagesTest::summaryReportsRangeAndLastValue()
{
    StubSource source;
    aerosim::ResultSummaryTablePage page;
    page.setDataSource(&source);
    QString error;
    QVERIFY(page.loadFile(QStringLiteral("C:/tmp/stub.csv"), &error));

    // One row per variable, and one aggregate per variable.
    QCOMPARE(page.tableWidget()->rowCount(), 2);
    QCOMPARE(page.tableWidget()->columnCount(), 8);
    QCOMPARE(page.tableWidget()->item(0, 0)->text(), QStringLiteral("CD"));
    QCOMPARE(page.tableWidget()->item(0, 2)->text(), QStringLiteral("4"));
    QVERIFY(std::abs(page.tableWidget()->item(0, 3)->text().toDouble() - 0.01) < 1e-9);
    QVERIFY(std::abs(page.tableWidget()->item(0, 4)->text().toDouble() - 0.08) < 1e-9);
    QVERIFY(std::abs(page.tableWidget()->item(0, 6)->text().toDouble() - 0.01) < 1e-9);
    QCOMPARE(page.tableWidget()->item(1, 7)->text(), QStringLiteral("残差"));
}

void ResultDataPagesTest::curveSlotAcceptsExactlyOneView()
{
    aerosim::ResultDataTablePage page;
    // Same ownership rule as above: the accepted view becomes a child of the
    // page, the rejected one stays with the caller and is cleaned up here.
    auto *first = new SpyCurveView;
    auto *second = new SpyCurveView;
    QVERIFY(page.setCurveView(first));
    QVERIFY(page.curveView() == first);
    // One slot, same contract as ComputeMonitorPage::insertCurveWidget.
    QVERIFY(!page.setCurveView(second));
    QVERIFY(!page.setCurveView(nullptr));
    QVERIFY(page.curveView() == first);
    delete second;
}

void ResultDataPagesTest::selectingColumnsDrawsImmediatelyBelowTable()
{
    StubSource source;
    aerosim::ResultDataTablePage page;
    auto *view = new SpyCurveView;
    page.setDataSource(&source);
    QVERIFY(page.setCurveView(view));
    QString error;
    QVERIFY(page.loadFile(QStringLiteral("stub.csv"), &error));
    QCOMPARE(view->replacements, 0);
    page.resize(960, 540);
    page.show();
    auto *header = page.tableWidget()->horizontalHeader();
    const QPoint columnCenter(header->sectionViewportPosition(2) + header->sectionSize(2) / 2,
                              header->viewport()->height() / 2);
    QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, columnCenter);
    QCOMPARE(view->keys, QStringList{QStringLiteral("rms[Rho]")});
    QCOMPARE(view->table.axisValues, QVector<double>({0, 10, 20, 30}));
    auto *splitter = page.findChild<QSplitter *>(QStringLiteral("resultDataRightSplit"));
    QVERIFY(splitter);
    QCOMPARE(splitter->widget(0), static_cast<QWidget *>(page.tableWidget()));
    QVERIFY(splitter->widget(1)->isAncestorOf(view));
    page.tableWidget()->selectColumn(1);
    QCOMPARE(view->keys, QStringList{QStringLiteral("CD")});
}

void ResultDataPagesTest::switchingXAxisUsesTheSelectedColumn()
{
    StubSource source;
    aerosim::ResultDataTablePage page;
    auto *view = new SpyCurveView;
    page.setDataSource(&source);
    QVERIFY(page.setCurveView(view));
    QString error;
    QVERIFY(page.loadFile(QStringLiteral("stub.csv"), &error));
    page.tableWidget()->selectColumn(2);
    auto *axis = page.findChild<QComboBox *>(QStringLiteral("resultDataXAxis"));
    QVERIFY(axis);
    QCOMPARE(axis->count(), 3);
    axis->setCurrentIndex(1); // CD becomes X, residual remains Y.
    QCOMPARE(view->table.axisValues, QVector<double>({0.08, 0.05, 0.03, 0.01}));
    QCOMPARE(view->keys, QStringList{QStringLiteral("rms[Rho]")});
    QVERIFY(view->axis.contains(QStringLiteral("CD")));
    QCOMPARE(page.table().axisValues, QVector<double>({0, 10, 20, 30}));
    axis->setCurrentIndex(0);
    QCOMPARE(view->table.axisValues, page.table().axisValues);
    page.clearData();
    QCOMPARE(axis->count(), 0);
    QVERIFY(!axis->isEnabled());
}

void ResultDataPagesTest::clearingSelectionClearsCurves()
{
    StubSource source;
    aerosim::ResultDataTablePage page;
    auto *view = new SpyCurveView;
    page.setDataSource(&source);
    QVERIFY(page.setCurveView(view));
    QString error;
    QVERIFY(page.loadFile(QStringLiteral("stub.csv"), &error));
    page.tableWidget()->selectColumn(1);
    const int clears = view->clears;
    page.tableWidget()->clearSelection();
    QCOMPARE(view->clears, clears + 1);
    // Opening another file must not carry stale column selections into it.
    page.tableWidget()->selectColumn(2);
    const int replacements = view->replacements;
    QVERIFY(page.loadFile(QStringLiteral("next.csv"), &error));
    QCOMPARE(view->replacements, replacements);
    QVERIFY(page.tableWidget()->selectedItems().isEmpty());
}

QTEST_MAIN(ResultDataPagesTest)
#include "ResultDataPagesTest.moc"
