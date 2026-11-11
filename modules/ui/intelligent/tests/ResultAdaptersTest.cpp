#include "aerosim/ui/intelligent/MonitorResultDataSource.h"
#include "aerosim/ui/intelligent/QwtResultCurveView.h"
#include "aerosim/ui/intelligent/ResultDataTablePage.h"
#include "aerosim/ui/intelligent/ResultSummaryTablePage.h"
#include <QComboBox>
#include <QLabel>
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
#include <limits>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_div.h>

class ResultAdaptersTest final : public QObject {
    Q_OBJECT
private slots:
    void realReaderFeedsSelectedRowsAndBothAxes();
    void constantTimeIsNotOfferedAsAnAxis();
    void wheelZoomMiddleDragAndResetWorkWithDualAxes();
    void invalidCoordinatesAreExcluded();
    void tecplotFixtureLoadsIntoMergedPage();
    void resultCurveControlsApplyAndSurviveSelection();
    void polynomialFitOnCfyUsesSelectedDegree();
    void forceSummaryComparesCasesByCoefficient();
};

void ResultAdaptersTest::realReaderFeedsSelectedRowsAndBothAxes()
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
    int cdRow = -1, residualRow = -1;
    for (int i = 0; i < page.tableWidget()->rowCount(); ++i) {
        const QString key = page.tableWidget()->item(i, 0)->text();
        if (key == QStringLiteral("CD")) cdRow = i;
        if (key == QStringLiteral("rms[Rho]")) residualRow = i;
    }
    QVERIFY(cdRow >= 0 && residualRow >= 0);
    page.tableWidget()->selectRow(residualRow);
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
    QCOMPARE(page.tableWidget()->rowCount(), 4);
    page.tableWidget()->selectRow(0);
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
    QVERIFY(!panel->isWindow());
    page.resize(1100, 450);
    page.show();
    QVERIFY(panel->isVisible());
    QVERIFY(panel->mapTo(&page, QPoint()).x()
            >= view->mapTo(&page, QPoint(view->width(), 0)).x());
    QVERIFY(panel->height() > view->height());
    QVERIFY(!mode->isEnabled());
    const auto chooseMode = [mode](const QString &label) {
        const int index = mode->findText(label);
        QVERIFY(index >= 0);
        mode->setCurrentIndex(index);
    };

    page.tableWidget()->selectRow(0);
    QVERIFY(mode->isEnabled());
    auto *chart = view->chart();
    auto *selection = page.tableWidget()->selectionModel();
    selection->select(page.tableWidget()->model()->index(1, 0),
                      QItemSelectionModel::Select | QItemSelectionModel::Rows);
    QCOMPARE(chart->visibleCurveCount(), 2);
    QCOMPARE(choice->count(), 2);
    const auto firstId = chart->curveIds().at(0);
    const auto secondId = chart->curveIds().at(1);
    QCOMPARE(choice->currentData().toString(), firstId);
    QVERIFY(!width->isEnabled());
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
    QVERIFY(!window->isEnabled());
    degree->setValue(2);
    QCOMPARE(chart->fitResult(firstId).degree, 2);
    chooseMode(QStringLiteral(u"移动平均"));
    QVERIFY(!degree->isEnabled());
    QVERIFY(window->isEnabled());
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

    page.tableWidget()->selectRow(1);
    QCOMPARE(chart->curveIds(), QStringList{secondId});
    selection->select(page.tableWidget()->model()->index(0, 0),
                      QItemSelectionModel::Select | QItemSelectionModel::Rows);
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
    int row = 0;
    while (row < page.tableWidget()->rowCount()
           && page.tableWidget()->item(row, 0)->text() != QStringLiteral("CFy"))
        ++row;
    QVERIFY(row < page.tableWidget()->rowCount());
    page.tableWidget()->selectRow(row);
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
    page.tableWidget()->selectRow(row);
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

QTEST_MAIN(ResultAdaptersTest)
#include "ResultAdaptersTest.moc"
