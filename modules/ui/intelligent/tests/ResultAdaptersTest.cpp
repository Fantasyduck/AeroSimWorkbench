#include "aerosim/ui/intelligent/MonitorResultDataSource.h"
#include "aerosim/ui/intelligent/CalculationCurveMonitor.h"
#include <QListWidget>
#include "aerosim/ui/intelligent/QwtResultCurveView.h"
#include "aerosim/ui/intelligent/ResultDataTablePage.h"
#include "aerosim/ui/intelligent/ResultSummaryTablePage.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QHelpEvent>
#include <QLineEdit>
#include <QToolTip>
#include <QPen>
#include <QMouseEvent>
#include <QPushButton>
#include <QWheelEvent>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSplitter>
#include <QFile>
#include <QItemSelectionModel>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <limits>
#include <cmath>

#include <qwt_legend.h>
#include <qwt_legend_label.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_div.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_map.h>
#include <qwt_text.h>

class ResultAdaptersTest final : public QObject {
    Q_OBJECT
private slots:
    void realReaderFeedsSelectedColumnsAndBothAxes();
    void constantTimeIsNotOfferedAsAnAxis();
    void wheelZoomMiddleDragAndResetWorkWithDualAxes();
    void invalidCoordinatesAreExcluded();
    void tecplotFixtureLoadsIntoMergedPage();
    void resultCurveControlsApplyAndSurviveSelection();
    void polynomialFitOnCfyUsesSelectedDegree();
    void forceSummaryComparesCasesByCoefficient();
    void clickingLegendRenamesOnlyThatCurve();
    void axisGranularityCanBeConfiguredFromTheDialog();
    void calculationMonitorFollowsFileAndKeepsSelection();
    void calculationMonitorHandlesRestartAndIndependentSubscriptions();
};

void ResultAdaptersTest::calculationMonitorFollowsFileAndKeepsSelection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("live.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Inner_Iter,Temperature,rms[Rho]\n0,300,0.1\n1,301,0.01\n");
    file.close();
    aerosim::CalculationCurveMonitor monitor;
    QString error;
    QVERIFY2(monitor.monitorFile(path, &error), qPrintable(error));
    auto *status = monitor.findChild<QLabel *>(QStringLiteral("calculationMonitorStatus"));
    QVERIFY(status && status->isHidden());
    QCOMPARE(monitor.table().sampleCount(), 2);
    QCOMPARE(monitor.curveView()->chart()->visibleCurveCount(), 2);
    auto *variables = monitor.findChild<QListWidget *>(QStringLiteral("calculationMonitorVariables"));
    QVERIFY(variables);
    variables->item(0)->setCheckState(Qt::Unchecked);
    QCOMPARE(monitor.curveView()->chart()->visibleCurveCount(), 1);

    QVERIFY(file.open(QIODevice::Append));
    file.write("2,302,");
    file.close();
    QVERIFY(monitor.poll(&error));
    QCOMPARE(monitor.table().sampleCount(), 2);
    QVERIFY(file.open(QIODevice::Append));
    file.write("0.001\n");
    file.close();
    QTRY_COMPARE(monitor.table().sampleCount(), 3);
    QCOMPARE(variables->item(0)->checkState(), Qt::Unchecked);
    const auto id = monitor.curveView()->chart()->curveIds().first();
    QCOMPARE(monitor.curveView()->chart()->curvePoints(id).size(), 3);
    monitor.setPaused(true);
    QVERIFY(file.open(QIODevice::Append));
    file.write("3,303,0.0001\n");
    file.close();
    QVERIFY(monitor.poll(&error));
    QCOMPARE(monitor.table().sampleCount(), 3);
    monitor.setPaused(false);
    QTRY_COMPARE(monitor.table().sampleCount(), 4);
    QVERIFY(status->isHidden());
    QVERIFY(!monitor.monitorFile(directory.filePath(QStringLiteral("missing.csv")), &error));
    QCOMPARE(monitor.table().sampleCount(), 4);
    if (qEnvironmentVariableIsSet("AEROSIM_CALCULATION_MONITOR_SCREENSHOT")) {
        monitor.resize(1000, 600);
        monitor.show();
        QTest::qWait(150);
        QVERIFY(monitor.grab().save(qEnvironmentVariable("AEROSIM_CALCULATION_MONITOR_SCREENSHOT")));
    }
}

void ResultAdaptersTest::calculationMonitorHandlesRestartAndIndependentSubscriptions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("live.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Inner_Iter,Temperature\n0,300\n1,301\n2,302\n3,303\n");
    file.close();
    aerosim::CalculationCurveMonitor single, multiple;
    QString error;
    QVERIFY(single.monitorFile(path, &error));
    QVERIFY(multiple.monitorFile(path, &error));
    multiple.setPaused(true);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("Inner_Iter,Pressure\n0,42\n");
    file.close();
    QVERIFY2(single.poll(&error), qPrintable(error));
    QCOMPARE(single.table().sampleCount(), 1);
    QCOMPARE(single.table().variables.first().key, QStringLiteral("Pressure"));
    QCOMPARE(single.curveView()->chart()->visibleCurveCount(), 1);
    QCOMPARE(multiple.table().sampleCount(), 4);
    multiple.setPaused(false);
    QTRY_COMPARE(multiple.table().sampleCount(), 1);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.close();
    QVERIFY(single.poll(&error));
    QVERIFY(single.table().isEmpty());
    QCOMPARE(single.curveView()->chart()->visibleCurveCount(), 0);
}

void ResultAdaptersTest::realReaderFeedsSelectedColumnsAndBothAxes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("history.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Inner_Iter,Time_Iter,CD,rms[Rho]\n"
               "0,0,0.08,0.1\n1,1,0.05,0.01\n2,2,0.03,0.001\n");
    file.close();
    aerosim::MonitorResultDataSource source;
    aerosim::ResultDataTablePage page;
    page.setDataSource(&source);
    auto *view = new aerosim::QwtResultCurveView;
    QVERIFY(page.setCurveView(view));
    QString error;
    QVERIFY2(page.loadFile(path, &error), qPrintable(error));
    QCOMPARE(page.table().alternativeAxes.size(), 1);
    QCOMPARE(view->chart()->visibleCurveCount(), 0);
    int cdColumn = -1, residualColumn = -1;
    for (int i = 1; i < page.tableWidget()->columnCount(); ++i) {
        const QString key = page.tableWidget()->horizontalHeaderItem(i)->text();
        if (key == QStringLiteral("CD")) cdColumn = i;
        if (key == QStringLiteral("rms[Rho]")) residualColumn = i;
    }
    QVERIFY(cdColumn >= 0 && residualColumn >= 0);
    page.tableWidget()->selectColumn(residualColumn);
    QCOMPARE(view->chart()->visibleCurveCount(), 1);
    const QString id = view->chart()->curveIds().first();
    QCOMPARE(view->chart()->curvePoints(id).at(1), QPointF(1, 0.01));
    QVERIFY(view->chart()->axisSummaries().first().title.contains(QStringLiteral("rms[Rho]")));
    QVERIFY(view->chart()->axisSummaries().first().logarithmic);
    auto *axis = page.findChild<QComboBox *>(QStringLiteral("resultDataXAxis"));
    QVERIFY(axis);
    axis->setCurrentIndex(1); // Time_Iter from the real reader.
    QCOMPARE(view->chart()->curvePoints(id).at(2), QPointF(2, 0.001));
    // CD from the same file as X, selected residual as Y.
    const int cdAxis = axis->findText(QStringLiteral("CD (-)"));
    QVERIFY(cdAxis >= 0);
    axis->setCurrentIndex(cdAxis);
    QCOMPARE(view->chart()->curvePoints(id).at(1), QPointF(0.05, 0.01));
    QCOMPARE(page.table().axisValues.at(1), 1.0);
    page.tableWidget()->clearSelection();
    QCOMPARE(view->chart()->visibleCurveCount(), 0);
}

void ResultAdaptersTest::wheelZoomMiddleDragAndResetWorkWithDualAxes()
{
    aerosim::ResultTable table;
    table.datasetId = QStringLiteral("zoom");
    table.axisValues = {0, 10, 20, 30};
    table.variables = {{QStringLiteral("CD"), {}, false}, {QStringLiteral("residual"), {}, true}};
    table.samples.insert(QStringLiteral("CD"), {0.1, 0.2, 0.15, 0.25});
    table.samples.insert(QStringLiteral("residual"), {1.0, 0.1, 0.01, 0.001});
    aerosim::QwtResultCurveView view;
    view.resize(750, 400);
    view.show();
    view.replaceSeries(table, {QStringLiteral("CD"), QStringLiteral("residual")}, QStringLiteral("X"));
    auto *plot = view.chart()->findChild<QwtPlot *>();
    auto *reset = view.findChild<QPushButton *>(QStringLiteral("resultCurveResetView"));
    QVERIFY(plot && reset);
    QVERIFY(view.chart()->axisSummaries().last().logarithmic);
    const int builds = view.chart()->chartBuildCount();
    const auto x = plot->axisScaleDiv(QwtAxis::XBottom).interval();
    const auto y = plot->axisScaleDiv(QwtAxis::YLeft).interval();
    const auto logY = plot->axisScaleDiv(QwtAxis::YRight).interval();
    QWidget *canvas = plot->canvas();
    const QPoint pos = canvas->rect().center();
    const auto scroll = [canvas, pos](int delta) {
        QWheelEvent wheel(pos, canvas->mapToGlobal(pos), {}, QPoint(0, delta),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(canvas, &wheel);
    };

    scroll(120);
    const double zoomedXWidth = plot->axisScaleDiv(QwtAxis::XBottom).interval().width();
    QVERIFY(zoomedXWidth < x.width());
    QVERIFY(plot->axisScaleDiv(QwtAxis::YLeft).interval().width() < y.width());
    const auto zoomedLog = plot->axisScaleDiv(QwtAxis::YRight).interval();
    QVERIFY(zoomedLog.minValue() > logY.minValue());
    QVERIFY(zoomedLog.maxValue() < logY.maxValue());
    scroll(-120);
    QVERIFY(plot->axisScaleDiv(QwtAxis::XBottom).interval().width() > zoomedXWidth);
    scroll(120);
    const auto beforePanX = plot->axisScaleDiv(QwtAxis::XBottom).interval();
    const auto beforePanY = plot->axisScaleDiv(QwtAxis::YLeft).interval();
    const auto beforePanLog = plot->axisScaleDiv(QwtAxis::YRight).interval();
    const QPoint dragged = pos + QPoint(42, 26);
    const auto sendMouse = [canvas](QEvent::Type type, const QPoint &position,
                                   Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent mouse(type, position, canvas->mapToGlobal(position), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(canvas, &mouse);
    };
    sendMouse(QEvent::MouseButtonPress, pos, Qt::MiddleButton, Qt::MiddleButton);
    sendMouse(QEvent::MouseMove, dragged, Qt::NoButton, Qt::MiddleButton);
    sendMouse(QEvent::MouseButtonRelease, dragged, Qt::MiddleButton, Qt::NoButton);

    const auto pannedX = plot->axisScaleDiv(QwtAxis::XBottom).interval();
    const auto pannedY = plot->axisScaleDiv(QwtAxis::YLeft).interval();
    const auto pannedLog = plot->axisScaleDiv(QwtAxis::YRight).interval();
    QVERIFY(pannedX.minValue() < beforePanX.minValue());
    QVERIFY(qAbs(pannedX.width() - beforePanX.width()) < 1e-10);
    QVERIFY(pannedY.minValue() > beforePanY.minValue());
    QVERIFY(qAbs(pannedY.width() - beforePanY.width()) < 1e-10);
    QVERIFY(pannedLog.minValue() > beforePanLog.minValue());
    QVERIFY(qAbs(pannedLog.maxValue() / pannedLog.minValue()
                 - beforePanLog.maxValue() / beforePanLog.minValue()) < 1e-8);
    sendMouse(QEvent::MouseMove, pos, Qt::NoButton, Qt::NoButton);
    QVERIFY(plot->axisScaleDiv(QwtAxis::XBottom).interval() == pannedX);

    reset->click();
    QVERIFY(plot->axisScaleDiv(QwtAxis::XBottom).interval() == x);
    QVERIFY(plot->axisScaleDiv(QwtAxis::YLeft).interval() == y);
    QVERIFY(plot->axisScaleDiv(QwtAxis::YRight).interval() == logY);
    QCOMPARE(view.chart()->chartBuildCount(), builds);

    auto ranges = view.chart()->axisRanges();
    QCOMPARE(ranges.size(), 3);
    auto invalid = ranges;
    QVERIFY(invalid.last().logarithmic);
    invalid.last().lower = 0.0;
    QVERIFY(!view.chart()->setFixedAxisRanges(invalid));
    ranges[0].lower = 5.0;
    ranges[0].upper = 15.0;
    QVERIFY(view.chart()->setFixedAxisRanges(ranges));
    const auto fixedX = plot->axisScaleDiv(QwtAxis::XBottom).interval();
    QCOMPARE(fixedX.minValue(), 5.0);
    QCOMPARE(fixedX.maxValue(), 15.0);
    scroll(120);
    QVERIFY(plot->axisScaleDiv(QwtAxis::XBottom).interval() == fixedX);
    view.replaceSeries(table, {QStringLiteral("CD"), QStringLiteral("residual")}, QStringLiteral("X"));
    plot = view.chart()->findChild<QwtPlot *>();
    QVERIFY(plot);
    QVERIFY(plot->axisScaleDiv(QwtAxis::XBottom).interval() == fixedX);
    QVERIFY(view.chart()->setFixedAxisRanges({}));
    QVERIFY(plot->axisScaleDiv(QwtAxis::XBottom).interval() == x);
}

void ResultAdaptersTest::constantTimeIsNotOfferedAsAnAxis()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("steady.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Inner_Iter,Time_Iter,CD\n0,0,0.08\n1,0,0.05\n2,0,0.03\n");
    file.close();
    aerosim::MonitorResultDataSource source;
    aerosim::ResultTable table;
    QString error;
    QVERIFY2(source.load(path, &table, &error), qPrintable(error));
    QVERIFY(table.alternativeAxes.isEmpty());
}

void ResultAdaptersTest::invalidCoordinatesAreExcluded()
{
    aerosim::ResultTable table;
    table.datasetId = QStringLiteral("finite");
    table.axisValues = {0, std::numeric_limits<double>::quiet_NaN(), 2, 3};
    aerosim::ResultVariable variable;
    variable.key = QStringLiteral("CD");
    table.variables.append(variable);
    table.samples.insert(variable.key, {1, 2, std::numeric_limits<double>::infinity(), 4});
    aerosim::QwtResultCurveView view;
    view.replaceSeries(table, {variable.key}, QStringLiteral("X"));
    const auto points = view.chart()->curvePoints(view.chart()->curveIds().first());
    QCOMPARE(points.size(), 2);
    QCOMPARE(points.first(), QPointF(0, 1));
    QCOMPARE(points.last(), QPointF(3, 4));
}

void ResultAdaptersTest::tecplotFixtureLoadsIntoMergedPage()
{
    aerosim::MonitorResultDataSource source;
    aerosim::ResultDataTablePage page;
    page.setDataSource(&source);
    auto *view = new aerosim::QwtResultCurveView;
    QVERIFY(page.setCurveView(view));
    QString error;
    QVERIFY2(page.loadFile(QStringLiteral(AEROSIM_RESULT_FIXTURE_DIR "/xcfd_history_tec.tec"), &error), qPrintable(error));
    QCOMPARE(page.table().sampleCount(), 10);
    QCOMPARE(page.tableWidget()->columnCount(), 5);
    QCOMPARE(page.tableWidget()->rowCount(), 10);
    page.tableWidget()->selectColumn(1);
    QCOMPARE(view->chart()->visibleCurveCount(), 1);
    page.resize(1000, 450);
    page.show();
    QTest::qWait(100);
    auto *splitter = page.findChild<QSplitter *>(QStringLiteral("resultDataRightSplit"));
    QVERIFY(splitter);
    QVERIFY(splitter->sizes().at(1) > splitter->sizes().at(0));
    splitter->setSizes({splitter->height(), 0});
    auto *chart = view->chart();
    QVERIFY(chart->isVisible());
    QVERIFY(chart->height() >= 200);
    QVERIFY(page.rect().contains(chart->mapTo(&page, chart->rect().bottomRight())));
    QVERIFY(!page.grab().isNull());
    const QString screenshot = qEnvironmentVariable("AEROSIM_RESULT_SCREENSHOT");
    if (!screenshot.isEmpty())
        QVERIFY(page.grab().save(screenshot));
}

void ResultAdaptersTest::resultCurveControlsApplyAndSurviveSelection()
{
    using namespace aerosim::monitor;
    aerosim::MonitorResultDataSource source;
    aerosim::ResultDataTablePage page;
    page.setDataSource(&source);
    auto *view = new aerosim::QwtResultCurveView;
    QVERIFY(page.setCurveView(view));
    QString error;
    QVERIFY2(page.loadFile(QStringLiteral(AEROSIM_RESULT_FIXTURE_DIR "/xcfd_history_tec.tec"), &error),
             qPrintable(error));
    auto *choice = view->settingsPanel()->findChild<QComboBox *>(QStringLiteral("resultCurveChoice"));
    auto *line = view->settingsPanel()->findChild<QComboBox *>(QStringLiteral("resultCurveLineStyle"));
    auto *width = view->settingsPanel()->findChild<QDoubleSpinBox *>(QStringLiteral("resultCurveLineWidth"));
    auto *shape = view->settingsPanel()->findChild<QComboBox *>(QStringLiteral("resultCurvePointShape"));
    auto *size = view->settingsPanel()->findChild<QDoubleSpinBox *>(QStringLiteral("resultCurvePointSize"));
    auto *mode = view->settingsPanel()->findChild<QComboBox *>(QStringLiteral("resultCurveDrawingMode"));
    auto *degree = view->settingsPanel()->findChild<QSpinBox *>(QStringLiteral("resultCurveFitDegree"));
    auto *window = view->settingsPanel()->findChild<QSpinBox *>(QStringLiteral("resultCurveFitWindow"));
    QVERIFY(choice && line && width && shape && size && mode && degree && window);
    auto *panel = view->settingsPanel();
    QVERIFY(panel->isAncestorOf(choice));
    QVERIFY(panel->isWindow());
    page.resize(1100, 450);
    page.show();
    QVERIFY(!panel->isVisible());
    QVERIFY(!mode->isEnabled());
    const auto chooseMode = [mode](const QString &label) {
        const int index = mode->findText(label);
        QVERIFY(index >= 0);
        mode->setCurrentIndex(index);
    };

    page.tableWidget()->selectColumn(1);
    QVERIFY(mode->isEnabled());
    auto *chart = view->chart();
    auto *selection = page.tableWidget()->selectionModel();
    selection->select(page.tableWidget()->model()->index(0, 2),
                      QItemSelectionModel::Select | QItemSelectionModel::Columns);
    QCOMPARE(chart->visibleCurveCount(), 2);
    QCOMPARE(choice->count(), 2);
    const auto firstId = chart->curveIds().at(0);
    const auto secondId = chart->curveIds().at(1);
    QCOMPARE(choice->currentData().toString(), firstId);
    QVERIFY(!width->isEnabled());
    QVERIFY(width->isHidden());
    QVERIFY(degree->isHidden());
    QVERIFY(window->isHidden());
    const auto renderedWidth = [chart]() -> qreal {
        const auto *plot = chart->findChild<QwtPlot *>();
        if (plot) {
            for (const auto *item : plot->itemList(QwtPlotItem::Rtti_PlotCurve)) {
                const auto *curve = static_cast<const QwtPlotCurve *>(item);
                if (curve->style() == QwtPlotCurve::Lines)
                    return curve->pen().widthF();
            }
        }
        return -1.0;
    };
    const int builds = chart->chartBuildCount();
    QCOMPARE(size->value(), 2.5);
    shape->setCurrentIndex(shape->findData(static_cast<int>(MonitorPointShape::Diamond)));
    size->setValue(8.0);
    chooseMode(QStringLiteral(u"直线连接采样点"));
    QVERIFY(width->isEnabled());
    QVERIFY(!width->isHidden());
    QCOMPARE(width->value(), 0.5);
    const qreal initialJoinWidth = renderedWidth();
    QVERIFY(initialJoinWidth > 0.0);
    width->setValue(4.0);
    QCOMPARE(renderedWidth(), initialJoinWidth + 3.5);
    line->setCurrentIndex(line->findData(static_cast<int>(Qt::DotLine)));
    QCOMPARE(chart->lineSeriesCount(), 1);
    QCOMPARE(chart->fitSeriesCount(), 0);
    chooseMode(QStringLiteral(u"最小二乘直线"));
    QCOMPARE(line->currentData().toInt(), static_cast<int>(Qt::DashLine));
    QCOMPARE(width->value(), 0.5);
    const qreal initialFitWidth = renderedWidth();
    QVERIFY(initialFitWidth > 0.0);
    width->setValue(6.5);
    QCOMPARE(renderedWidth(), initialFitWidth + 6.0);
    line->setCurrentIndex(line->findData(static_cast<int>(Qt::DashDotLine)));
    QCOMPARE(chart->chartBuildCount(), builds);
    QCOMPARE(chart->lineSeriesCount(), 0);
    QCOMPARE(chart->fitSeriesCount(), 1);
    chooseMode(QStringLiteral(u"样条连接采样点"));
    QCOMPARE(line->currentData().toInt(), static_cast<int>(Qt::DotLine));
    QCOMPARE(width->value(), 4.0);
    QCOMPARE(chart->lineSeriesCount(), 1);
    QCOMPARE(chart->fitSeriesCount(), 0);
    chooseMode(QStringLiteral(u"最小二乘直线"));
    QCOMPARE(line->currentData().toInt(), static_cast<int>(Qt::DashDotLine));
    QCOMPARE(chart->curveStyle(firstId).pointShape, MonitorPointShape::Diamond);
    QCOMPARE(chart->curveStyle(firstId).pointSize, 8.0);
    QCOMPARE(chart->curveStyle(firstId).lineStyle, Qt::DotLine);
    QCOMPARE(chart->curveStyle(firstId).fit.lineStyle, Qt::DashDotLine);
    QCOMPARE(chart->curveStyle(firstId).lineWidth, 4.0);
    QCOMPARE(chart->curveStyle(firstId).fit.lineWidth, 6.5);
    QCOMPARE(width->value(), 6.5);
    QCOMPARE(chart->curveStyle(secondId).pointShape, MonitorPointShape::Circle);
    QCOMPARE(chart->curveStyle(secondId).fit.method, MonitorFitMethod::None);
    QCOMPARE(chart->curveStyle(secondId).lineWidth, 0.5);
    QCOMPARE(chart->curveStyle(secondId).fit.lineWidth, 0.5);
    QCOMPARE(chart->curveStyle(secondId).interpolation, MonitorInterpolation::None);

    chooseMode(QStringLiteral(u"多项式拟合"));
    QVERIFY(degree->isEnabled());
    QVERIFY(!degree->isHidden());
    QVERIFY(!window->isEnabled());
    QVERIFY(window->isHidden());
    degree->setValue(2);
    QCOMPARE(chart->fitResult(firstId).degree, 2);
    chooseMode(QStringLiteral(u"移动平均"));
    QVERIFY(!degree->isEnabled());
    QVERIFY(degree->isHidden());
    QVERIFY(window->isEnabled());
    QVERIFY(!window->isHidden());
    window->setValue(5);
    QCOMPARE(chart->fitResult(firstId).window, 5);

    chart->selectAt(secondId, chart->curvePoints(secondId).first());
    QCOMPARE(choice->currentData().toString(), secondId);
    QCOMPARE(shape->currentData().toInt(), static_cast<int>(MonitorPointShape::Circle));
    shape->setCurrentIndex(shape->findData(static_cast<int>(MonitorPointShape::Square)));
    QCOMPARE(chart->curveStyle(firstId).pointShape, MonitorPointShape::Diamond);
    QCOMPARE(chart->curveStyle(secondId).pointShape, MonitorPointShape::Square);

    auto *axis = page.findChild<QComboBox *>(QStringLiteral("resultDataXAxis"));
    QVERIFY(axis && axis->count() > 1);
    axis->setCurrentIndex(1);
    QCOMPARE(choice->currentData().toString(), secondId);
    QCOMPARE(chart->curveStyle(secondId).pointShape, MonitorPointShape::Square);

    page.tableWidget()->selectColumn(2);
    QCOMPARE(chart->curveIds(), QStringList{secondId});
    selection->select(page.tableWidget()->model()->index(0, 1),
                      QItemSelectionModel::Select | QItemSelectionModel::Columns);
    choice->setCurrentIndex(choice->findData(firstId));
    QCOMPARE(chart->selectedCurve(), firstId);
    QCOMPARE(chart->curveStyle(firstId).pointShape, MonitorPointShape::Diamond);
    QCOMPARE(chart->curveStyle(firstId).lineStyle, Qt::DotLine);
    QCOMPARE(chart->curveStyle(firstId).fit.method, MonitorFitMethod::MovingAverage);
    QCOMPARE(chart->curveStyle(firstId).fit.window, 5);
    QCOMPARE(chart->curveStyle(firstId).fit.lineStyle, Qt::DashDotLine);
    QCOMPARE(chart->curveStyle(firstId).lineWidth, 4.0);
    QCOMPARE(chart->curveStyle(firstId).fit.lineWidth, 6.5);
    QCOMPARE(width->value(), 6.5);
}

void ResultAdaptersTest::polynomialFitOnCfyUsesSelectedDegree()
{
    using namespace aerosim::monitor;
    aerosim::MonitorResultDataSource source;
    aerosim::ResultDataTablePage page;
    page.setDataSource(&source);
    auto *view = new aerosim::QwtResultCurveView;
    QVERIFY(page.setCurveView(view));
    QString error;
    QVERIFY2(page.loadFile(QStringLiteral(AEROSIM_RESULT_FIXTURE_DIR "/su2_history_steady.csv"), &error),
             qPrintable(error));
    int column = 1;
    while (column < page.tableWidget()->columnCount()
           && page.tableWidget()->horizontalHeaderItem(column)->text() != QStringLiteral("CFy"))
        ++column;
    QVERIFY(column < page.tableWidget()->columnCount());
    page.tableWidget()->selectColumn(column);
    auto *chart = view->chart();
    QCOMPARE(chart->curveIds().size(), 1);
    const QString id = chart->curveIds().first();
    const auto yMaximum = [chart] {
        auto *plot = chart->findChild<QwtPlot *>();
        return plot ? plot->axisScaleDiv(QwtAxis::YLeft).upperBound() : 0.0;
    };
    const double rawMaximum = yMaximum();
    auto *method = view->settingsPanel()->findChild<QComboBox *>(QStringLiteral("resultCurveDrawingMode"));
    auto *feedback = view->settingsPanel()->findChild<QLabel *>(QStringLiteral("resultCurveFitFeedback"));
    QVERIFY(method && feedback);
    method->setCurrentIndex(method->findText(QStringLiteral(u"最小二乘直线")));
    const auto linear = chart->fitResult(id);
    QVERIFY(linear.isValid());
    QCOMPARE(linear.degree, 1);
    QVERIFY(qAbs(linear.rSquared - 0.104563) < 1e-5);

    method->setCurrentIndex(method->findText(QStringLiteral(u"多项式拟合")));
    const auto cubic = chart->fitResult(id);
    QVERIFY(cubic.isValid());
    QCOMPARE(cubic.degree, 3);
    QVERIFY(qAbs(cubic.rSquared - 0.477227) < 1e-5);
    QVERIFY(feedback->text().contains(QStringLiteral("0.477")));
    QVERIFY(yMaximum() > rawMaximum + 0.04);
    for (const auto &point : cubic.points)
        QVERIFY(yMaximum() + 1e-10 >= point.y());

    page.tableWidget()->clearSelection();
    page.tableWidget()->selectColumn(column);
    QVERIFY(yMaximum() > rawMaximum + 0.04);
    method->setCurrentIndex(method->findText(QStringLiteral(u"仅散点")));
    QCOMPARE(yMaximum(), rawMaximum);
}

void ResultAdaptersTest::forceSummaryComparesCasesByCoefficient()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto first = directory.filePath(QStringLiteral("M6_Ma5h5000.00a-2.00b1.00_force.csv"));
    const auto second = directory.filePath(QStringLiteral("M6_Ma6h6000.00a0.00b0.00_force.csv"));
    const QByteArray header("BC index,wallname,Cl,Cd,Cx,Cy,Cz,Cmx,Cmy,Cmz\n");
    QFile firstFile(first);
    QVERIFY(firstFile.open(QIODevice::WriteOnly));
    firstFile.write(header + "3,wing_down,0.048,0.024,0,0,0,0,0,0\n"
                             "ALL,ALL_WALL,-3.3624989E-07,4.8601479E-02,0,0,0,0,0,0\n");
    firstFile.close();
    QFile secondFile(second);
    QVERIFY(secondFile.open(QIODevice::WriteOnly));
    secondFile.write(header + "4,wing_edge,0,3.2065515E-05,0,0,0,0,0,0\n"
                              "ALL,ALL_WALL,-3.4399804E-07,4.7155040E-02,0,0,0,0,0,0\n");
    secondFile.close();

    aerosim::ResultSummaryTablePage page;
    QString error;
    QVERIFY2(page.loadForceFiles({first, second}, &error), qPrintable(error));
    auto *table = page.tableWidget();
    auto *coefficient = page.findChild<QComboBox *>(QStringLiteral("forceSummaryCoefficient"));
    QVERIFY(coefficient);
    QVERIFY(coefficient->isVisibleTo(&page));
    QCOMPARE(coefficient->count(), 8);
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->columnCount(), 8);
    QCOMPARE(table->horizontalHeaderItem(5)->text(), QStringLiteral("wing_down"));
    QCOMPARE(table->horizontalHeaderItem(6)->text(), QStringLiteral("wing_edge"));
    QCOMPARE(table->horizontalHeaderItem(7)->text(), QStringLiteral("ALL_WALL"));
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("h:5000.00"));
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("5"));
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral("1.00"));
    QCOMPARE(table->item(0, 4)->text(), QStringLiteral("-2.00"));
    QCOMPARE(table->item(1, 5)->text(), QStringLiteral(u"—"));
    QCOMPARE(table->item(0, 7)->text(), QStringLiteral("-3.3624989E-07"));
    coefficient->setCurrentIndex(coefficient->findText(QStringLiteral("Cd")));
    QCOMPARE(table->item(0, 7)->text(), QStringLiteral("4.8601479E-02"));
    QCOMPARE(table->item(1, 7)->text(), QStringLiteral("4.7155040E-02"));
    QVERIFY(table->item(1, 7)->font().bold());
    QVERIFY(!page.loadForceFiles({first, directory.filePath(QStringLiteral("missing.csv"))}, &error));
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(1, 7)->text(), QStringLiteral("4.7155040E-02"));
    page.clearData();
    QVERIFY(!coefficient->isVisibleTo(&page));
    QCOMPARE(table->rowCount(), 0);
}

void ResultAdaptersTest::clickingLegendRenamesOnlyThatCurve()
{
    aerosim::ResultTable table;
    table.datasetId = QStringLiteral("legend-test");
    table.axisValues = {0.0, 1.0, 2.0};
    table.variables = {{QStringLiteral("CD"), QStringLiteral("1"), false},
                       {QStringLiteral("CL"), QString(), false}};
    table.samples.insert(QStringLiteral("CD"), {0.1, 0.2, 0.3});
    table.samples.insert(QStringLiteral("CL"), {0.5, 0.6, 0.7});
    const QStringList keys{QStringLiteral("CD"), QStringLiteral("CL")};
    aerosim::QwtResultCurveView view;
    view.resize(750, 420);
    view.show();
    view.replaceSeries(table, keys, QStringLiteral("X"));
    auto *panel = view.settingsPanel();
    QVERIFY(panel && panel->isWindow());
    QVERIFY(!panel->isVisible());
    auto *editor = panel->findChild<QLineEdit *>(QStringLiteral("resultCurveLegendName"));
    QVERIFY(editor);
    const auto clickLegend = [&view, panel, editor](const QString &caption, const QString &name) {
        auto *plot = view.chart()->findChild<QwtPlot *>();
        QVERIFY(plot && plot->legend());
        QwtLegendLabel *entry = nullptr;
        for (auto *label : plot->legend()->findChildren<QwtLegendLabel *>()) {
            if (label->text().text() == caption) {
                entry = label;
                break;
            }
        }
        QVERIFY(entry);
        QTest::mouseClick(entry, Qt::LeftButton);
        QApplication::processEvents();
        QVERIFY(panel->isVisible());
        QCOMPARE(editor->text(), caption);
        editor->selectAll();
        QTest::keyClick(editor, Qt::Key_Backspace);
        if (!name.isEmpty())
            QTest::keyClicks(editor, name);
        QTest::keyClick(editor, Qt::Key_Return);
        panel->close();
        QVERIFY(!panel->isVisible());
    };
    const auto defaults = view.chart()->legendLabels();
    QCOMPARE(defaults, (QStringList{QStringLiteral("CD (1)"), QStringLiteral("CL")}));
    clickLegend(defaults.first(), QStringLiteral("Drag coefficient"));
    QCOMPARE(view.chart()->legendLabels().first(), QStringLiteral("Drag coefficient"));
    QCOMPARE(view.chart()->legendLabels().last(), QStringLiteral("CL"));
    view.clearSeries();
    view.replaceSeries(table, keys, QStringLiteral("X"));
    QCOMPARE(view.chart()->legendLabels().first(), QStringLiteral("Drag coefficient"));
    clickLegend(QStringLiteral("Drag coefficient"), QString());
    QCOMPARE(view.chart()->legendLabels(), defaults);

    auto *plot = view.chart()->findChild<QwtPlot *>();
    QVERIFY(plot);
    QWidget *canvas = plot->canvas();
    const QPoint sample(qRound(plot->canvasMap(QwtAxis::XBottom).transform(1.0)),
                        qRound(plot->canvasMap(QwtAxis::YLeft).transform(0.2)));
    QHelpEvent hover(QEvent::ToolTip, sample, canvas->mapToGlobal(sample));
    QApplication::sendEvent(canvas, &hover);
    QVERIFY(QToolTip::text().contains(QStringLiteral("CD (1)")));
    QVERIFY(QToolTip::text().contains(QStringLiteral("X: 1")));
    QVERIFY(QToolTip::text().contains(QStringLiteral("Y: 0.2")));
    QToolTip::hideText();
}

void ResultAdaptersTest::axisGranularityCanBeConfiguredFromTheDialog()
{
    aerosim::ResultTable table;
    table.datasetId = QStringLiteral("axis-granularity");
    table.variables = {{QStringLiteral("CD"), QStringLiteral("-"), false},
                       {QStringLiteral("residual"), QStringLiteral("-"), true}};
    for (int row = 0; row <= 10; ++row) {
        table.axisValues.append(row);
        table.samples[QStringLiteral("CD")].append(row / 10.0);
        table.samples[QStringLiteral("residual")].append(std::pow(10.0, -row - 2));
    }
    aerosim::QwtResultCurveView view;
    view.replaceSeries(table, {QStringLiteral("CD"), QStringLiteral("residual")}, QStringLiteral("X"));
    view.resize(1000, 650);
    view.show();
    auto *button = view.findChild<QPushButton *>(QStringLiteral("resultCurveAxisSettings"));
    QVERIFY(button);
    bool configured = false;
    QTimer::singleShot(0, &view, [&] {
        auto *dialog = view.findChild<QDialog *>(QStringLiteral("resultAxisSettingsDialog"));
        QVERIFY(dialog);
        auto *mode = dialog->findChild<QComboBox *>(QStringLiteral("resultAxisRangeMode"));
        QVERIFY(mode);
        mode->setCurrentIndex(1);
        for (const auto &axis : view.chart()->axisRanges()) {
            auto *lower = dialog->findChild<QLineEdit *>(QStringLiteral("axisLower_%1").arg(axis.id));
            auto *upper = dialog->findChild<QLineEdit *>(QStringLiteral("axisUpper_%1").arg(axis.id));
            auto *step = dialog->findChild<QLineEdit *>(QStringLiteral("axisStep_%1").arg(axis.id));
            auto *minor = dialog->findChild<QSpinBox *>(QStringLiteral("axisMinor_%1").arg(axis.id));
            auto *precision = dialog->findChild<QSpinBox *>(QStringLiteral("axisPrecision_%1").arg(axis.id));
            QVERIFY(lower && upper && step && minor && precision);
            lower->setText(axis.logarithmic ? QStringLiteral("1e-12") : QStringLiteral("0"));
            upper->setText(axis.id == QwtAxis::XBottom ? QStringLiteral("10")
                : axis.logarithmic ? QStringLiteral("1e-2") : QStringLiteral("1"));
            step->setText(axis.id == QwtAxis::XBottom || axis.logarithmic
                ? QStringLiteral("2") : QStringLiteral("0.2"));
            minor->setValue(0);
            precision->setValue(12);
        }
        const QString screenshot = qEnvironmentVariable("AEROSIM_AXIS_DIALOG_SCREENSHOT");
        if (!screenshot.isEmpty())
            QVERIFY(dialog->grab().save(screenshot));
        auto *buttons = dialog->findChild<QDialogButtonBox *>(QStringLiteral("resultAxisButtons"));
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Ok)->click();
        configured = dialog->result() == QDialog::Accepted;
    });
    QTimer::singleShot(5000, &view, [&view] {
        auto *dialog = view.findChild<QDialog *>(QStringLiteral("resultAxisSettingsDialog"));
        if (dialog)
            dialog->reject();
    });
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY(configured);
    QVERIFY(view.chart()->hasFixedAxisRanges());
    auto *plot = view.chart()->findChild<QwtPlot *>();
    QVERIFY(plot);
    const auto xTicks = plot->axisScaleDiv(QwtAxis::XBottom).ticks(QwtScaleDiv::MajorTick);
    QCOMPARE(xTicks.size(), 6);
    QVERIFY(std::abs(xTicks.at(1) - xTicks.at(0) - 2) < 1e-10);
    const auto yTicks = plot->axisScaleDiv(QwtAxis::YLeft).ticks(QwtScaleDiv::MajorTick);
    QVERIFY(yTicks.size() >= 2);
    QVERIFY(std::abs(yTicks.at(1) - yTicks.at(0) - 0.2) < 1e-10);
    const auto logTicks = plot->axisScaleDiv(QwtAxis::YRight).ticks(QwtScaleDiv::MajorTick);
    QVERIFY(logTicks.size() >= 2);
    QVERIFY(std::abs(std::log10(logTicks.at(1) / logTicks.at(0)) - 2) < 1e-10);
    QVERIFY(plot->axisScaleDiv(QwtAxis::YLeft).ticks(QwtScaleDiv::MinorTick).isEmpty());
    QVERIFY(plot->axisScaleDiv(QwtAxis::YLeft).ticks(QwtScaleDiv::MediumTick).isEmpty());
    QCOMPARE(plot->axisScaleDraw(QwtAxis::YLeft)->label(0.01100871616).text(), QStringLiteral("0.01100871616"));

    const QString plotScreenshot = qEnvironmentVariable("AEROSIM_AXIS_PLOT_SCREENSHOT");
    if (!plotScreenshot.isEmpty()) {
        QCoreApplication::processEvents();
        QVERIFY(view.grab().save(plotScreenshot));
    }

    auto settings = view.chart()->axisRanges();
    auto invalid = settings;
    invalid[0].majorStep = -1;
    QVERIFY(!view.chart()->setAxisConfiguration(invalid, true));
    invalid = settings;
    invalid[0].majorStep = 1e-300;
    QVERIFY(!view.chart()->setAxisConfiguration(invalid, true));
    invalid = settings;
    invalid.last().majorStep = 0.5;
    QVERIFY(!view.chart()->setAxisConfiguration(invalid, true));
    QCOMPARE(view.chart()->axisRanges().first().majorStep, 2.0);

    // Granularity also applies with automatic bounds and survives plot rebuilds.
    bool automatic = false;
    QTimer::singleShot(0, &view, [&] {
        auto *dialog = view.findChild<QDialog *>(QStringLiteral("resultAxisSettingsDialog"));
        QVERIFY(dialog);
        auto *mode = dialog->findChild<QComboBox *>(QStringLiteral("resultAxisRangeMode"));
        QVERIFY(mode);
        mode->setCurrentIndex(0);
        auto *lower = dialog->findChild<QLineEdit *>(QStringLiteral("axisLower_%1").arg(QwtAxis::XBottom));
        auto *step = dialog->findChild<QLineEdit *>(QStringLiteral("axisStep_%1").arg(QwtAxis::XBottom));
        QVERIFY(lower && step);
        QVERIFY(!lower->isEnabled() && step->isEnabled());
        auto *buttons = dialog->findChild<QDialogButtonBox *>(QStringLiteral("resultAxisButtons"));
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Ok)->click();
        automatic = dialog->result() == QDialog::Accepted;
    });
    QTest::mouseClick(button, Qt::LeftButton);
    QVERIFY(automatic);
    QVERIFY(!view.chart()->hasFixedAxisRanges());
    view.replaceSeries(table, {QStringLiteral("CD"), QStringLiteral("residual")}, QStringLiteral("X"));
    plot = view.chart()->findChild<QwtPlot *>();
    QVERIFY(plot);
    QCOMPARE(view.chart()->axisRanges().first().majorStep, 2.0);
    QCOMPARE(plot->axisMaxMinor(QwtAxis::YLeft), 0);
    QCOMPARE(plot->axisScaleDraw(QwtAxis::YLeft)->label(0.01100871616).text(), QStringLiteral("0.01100871616"));
    QWidget *canvas = plot->canvas();
    const QPoint center = canvas->rect().center();
    QWheelEvent zoom(center, canvas->mapToGlobal(center), {}, QPoint(0, 120),
                     Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(canvas, &zoom);
    const auto zoomed = plot->axisScaleDiv(QwtAxis::XBottom).interval();
    settings = view.chart()->axisRanges();
    settings[0].majorStep = 1;
    QVERIFY(view.chart()->setAxisConfiguration(settings, false));
    QVERIFY(plot->axisScaleDiv(QwtAxis::XBottom).interval() == zoomed);
    settings = view.chart()->axisRanges();
    for (auto &axis : settings) {
        if (axis.id == QwtAxis::YLeft) {
            axis.lower = 0.01100871;
            axis.upper = 0.01100873;
            axis.majorStep = 1e-9;
            axis.minorTicks = 4;
        }
    }
    QVERIFY(view.chart()->setAxisConfiguration(settings, true));
    const auto fineTicks = plot->axisScaleDiv(QwtAxis::YLeft).ticks(QwtScaleDiv::MajorTick);
    QVERIFY(fineTicks.size() > 10);
    QVERIFY(std::abs(fineTicks.at(1) - fineTicks.at(0) - 1e-9) < 1e-14);
    QVERIFY(plot->axisScaleDraw(QwtAxis::YLeft)->label(fineTicks.at(0)).text()
        != plot->axisScaleDraw(QwtAxis::YLeft)->label(fineTicks.at(1)).text());
    QVERIFY(!plot->axisScaleDiv(QwtAxis::YLeft).ticks(QwtScaleDiv::MinorTick).isEmpty()
        || !plot->axisScaleDiv(QwtAxis::YLeft).ticks(QwtScaleDiv::MediumTick).isEmpty());
}

QTEST_MAIN(ResultAdaptersTest)
#include "ResultAdaptersTest.moc"
