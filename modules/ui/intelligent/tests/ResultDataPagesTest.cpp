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
#include <QFile>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QPixmap>
#include <QTimer>

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
    void summaryReadsArbitraryCsvFields();
    void summaryCombinesDifferentCsvColumns();
    void summaryRejectsMalformedCsvAtomically();
    void summaryComparesDynamicWallFields();
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
    QVERIFY(page.loadFile(QStringLiteral("C:/tmp/stub.dat"), &error));

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

void ResultDataPagesTest::summaryReadsArbitraryCsvFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("measurements.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray contents = QByteArray::fromHex("efbbbf")
        + QStringLiteral(u"\"温度 (K)\",\"Pressure, Pa\",备注,状态\r\n"
                         "300,1.01e5,\"位置 A, 入口\",正常\r\n"
                         "310,,\"第一行\r\n第二行 \"\"引号\"\"\",\r\n").toUtf8();
    QCOMPARE(file.write(contents), qint64(contents.size()));
    file.close();

    aerosim::ResultSummaryTablePage page;
    QSignalSpy loaded(&page, &aerosim::ResultSummaryTablePage::fileLoaded);
    QString error;
    QVERIFY2(page.loadFile(path, &error), qPrintable(error));
    QVERIFY(error.isEmpty());
    QCOMPARE(loaded.count(), 1);
    auto *table = page.tableWidget();
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->horizontalHeaderItem(1)->text(), QStringLiteral(u"温度 (K)"));
    QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("Pressure, Pa"));
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("1.01e5"));
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral(u"位置 A, 入口"));
    QCOMPARE(table->item(1, 2)->text(), QStringLiteral(u"—"));
    QCOMPARE(table->item(1, 3)->text(), QStringLiteral(u"第一行\n第二行 \"引号\""));
    QVERIFY(!page.findChild<QComboBox *>(QStringLiteral("forceSummaryCoefficient"))->isVisibleTo(&page));
    auto *button = page.findChild<QPushButton *>(QStringLiteral("resultSummaryCsvOpenButton"));
    QVERIFY(button && button->isEnabled());
    QCOMPARE(button->text(), QStringLiteral(u"打开 CSV…"));
    page.resize(900, 500);
    page.show();
    QCoreApplication::processEvents();
    QVERIFY(!page.grab().isNull());
    const bool nativeDialogsDisabled = qApp->testAttribute(Qt::AA_DontUseNativeDialogs);
    qApp->setAttribute(Qt::AA_DontUseNativeDialogs);
    QTimer::singleShot(0, &page, [&path] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *dialog = qobject_cast<QFileDialog *>(widget);
            if (dialog) {
                dialog->selectFile(path);
                QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
            }
        }
    });
    QTimer::singleShot(3000, &page, [] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *dialog = qobject_cast<QFileDialog *>(widget);
            if (dialog)
                dialog->reject();
        }
    });
    QTest::mouseClick(button, Qt::LeftButton);
    qApp->setAttribute(Qt::AA_DontUseNativeDialogs, nativeDialogsDisabled);
    QCOMPARE(loaded.count(), 2);
    QCOMPARE(table->horizontalHeaderItem(1)->text(), QStringLiteral(u"温度 (K)"));
}

void ResultDataPagesTest::summaryCombinesDifferentCsvColumns()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString first = directory.filePath(QStringLiteral("first.csv"));
    const QString second = directory.filePath(QStringLiteral("second.csv"));
    QFile file(first);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Temperature,Pressure\n300,101325\n");
    file.close();
    file.setFileName(second);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("pressure,Efficiency,Case\n99000,0.85,case-b\n98000,0.90\n");
    file.close();

    aerosim::ResultSummaryTablePage page;
    QString error;
    QVERIFY2(page.loadCsvFiles({first, second}, &error), qPrintable(error));
    auto *table = page.tableWidget();
    QCOMPARE(table->rowCount(), 3);
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->horizontalHeaderItem(2)->text(), QStringLiteral("Pressure"));
    QCOMPARE(table->item(1, 0)->text(), QStringLiteral("second.csv"));
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral(u"—"));
    QCOMPARE(table->item(1, 1)->text(), QStringLiteral(u"—"));
    QCOMPARE(table->item(1, 2)->text(), QStringLiteral("99000"));
    QCOMPARE(table->item(1, 4)->text(), QStringLiteral("case-b"));
    QCOMPARE(table->item(2, 4)->text(), QStringLiteral(u"—"));
    QVERIFY(!table->item(1, 2)->flags().testFlag(Qt::ItemIsEditable));
    page.clearData();
    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(table->columnCount(), 0);
}

void ResultDataPagesTest::summaryRejectsMalformedCsvAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString good = directory.filePath(QStringLiteral("good.csv"));
    const QString bad = directory.filePath(QStringLiteral("bad.csv"));
    QFile file(good);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Temperature\n300\n\"\"\n");
    file.close();
    aerosim::ResultSummaryTablePage page;
    QString error;
    QVERIFY2(page.loadCsvFiles({good}, &error), qPrintable(error));
    QCOMPARE(page.tableWidget()->rowCount(), 2);
    QSignalSpy loaded(&page, &aerosim::ResultSummaryTablePage::fileLoaded);
    const QList<QByteArray> malformed{
        "A,a\n1,2\n", "A,\n1,2\n", "A,B\n1,2,3\n",
        "A,B\n1,\"open\n", "A,B\n1,\"value\"x\n", "A,B\n1,un\"quoted\n",
        "A,B\n", ""};
    for (const QByteArray &contents : malformed) {
        file.setFileName(bad);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents);
        file.close();
        QVERIFY(!page.loadCsvFiles({good, bad}, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(page.tableWidget()->rowCount(), 2);
        QCOMPARE(page.tableWidget()->item(0, 1)->text(), QStringLiteral("300"));
    }
    QVERIFY(!page.loadCsvFiles({}, &error));
    QVERIFY(!page.loadCsvFiles({directory.filePath(QStringLiteral("absent.csv"))}, &error));
    QCOMPARE(loaded.count(), 0);
}

void ResultDataPagesTest::summaryComparesDynamicWallFields()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString first = directory.filePath(QStringLiteral("first.csv"));
    const QString second = directory.filePath(QStringLiteral("second.csv"));
    QFile file(first);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("wallname,Temperature,Cl,Note\nwall-a,300,0.1,first\n");
    file.close();
    file.setFileName(second);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("wallname,Pressure,Temperature\nwall-b,100000,320\n");
    file.close();
    aerosim::ResultSummaryTablePage page;
    QString error;
    QVERIFY2(page.loadForceFiles({first, second}, &error), qPrintable(error));
    auto *field = page.findChild<QComboBox *>(QStringLiteral("forceSummaryCoefficient"));
    auto *view = page.findChild<QComboBox *>(QStringLiteral("resultSummaryCsvView"));
    QVERIFY(field && view);
    QCOMPARE(field->count(), 4);
    QCOMPARE(field->currentText(), QStringLiteral("Temperature"));
    QCOMPARE(page.tableWidget()->item(0, 5)->text(), QStringLiteral("300"));
    field->setCurrentIndex(field->findText(QStringLiteral("Pressure")));
    QCOMPARE(page.tableWidget()->item(0, 5)->text(), QStringLiteral(u"—"));
    QCOMPARE(page.tableWidget()->item(1, 6)->text(), QStringLiteral("100000"));
    field->setCurrentIndex(field->findText(QStringLiteral("Note")));
    QCOMPARE(page.tableWidget()->item(0, 5)->text(), QStringLiteral("first"));
    view->setCurrentIndex(0);
    QCOMPARE(page.tableWidget()->columnCount(), 6);
    QCOMPARE(page.tableWidget()->horizontalHeaderItem(1)->text(), QStringLiteral("wallname"));
    QCOMPARE(page.tableWidget()->item(1, 5)->text(), QStringLiteral("100000"));
    QVERIFY(!field->isVisibleTo(&page));
    view->setCurrentIndex(1);
    QCOMPARE(page.tableWidget()->item(0, 5)->text(), QStringLiteral("first"));

    // Duplicate wall names are valid records and stay available in detail view.
    file.setFileName(second);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("wallname,Temperature\nwall-a,300\nwall-a,310\n");
    file.close();
    QVERIFY2(page.loadFile(second, &error), qPrintable(error));
    QCOMPARE(page.tableWidget()->rowCount(), 2);
    QVERIFY(!view->isVisibleTo(&page));
    QCOMPARE(page.tableWidget()->item(1, 2)->text(), QStringLiteral("310"));
}

QTEST_MAIN(ResultDataPagesTest)
#include "ResultDataPagesTest.moc"
