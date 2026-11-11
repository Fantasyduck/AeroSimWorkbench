// Renders the monitor chart from a real solver history file and checks what the
// widget actually reports. The PNG is the acceptance artefact: without it a
// "curve module" can pass every numeric check and still draw nothing.
// <iterator> must come before every Qt header: Qt 5.12's qlist.h reaches for
// stdext::make_checked_array_iterator, which only exists once MSVC's iterator
// header has been pulled in. MonitorChartView.h already drags in Qt, so this
// include has to stay at the very top of the file.
#include <iterator>

#include "MonitorChartView.h"
#include "MonitorLogReader.h"
#include "MonitorSeriesModel.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QElapsedTimer>
#include <QSet>
#include <QTextStream>

#include <qwt_plot_curve.h>

using aerosim::monitor::MonitorAxis;
using aerosim::monitor::MonitorCurve;
using aerosim::monitor::MonitorLog;
using aerosim::monitor::MonitorLogReader;
using aerosim::monitor::MonitorScale;
using aerosim::monitor::MonitorSeriesModel;
using aerosim::monitor::MonitorChartView;
using aerosim::monitor::MonitorInterpolation;
using aerosim::monitor::MonitorCurveStyle;
using aerosim::monitor::MonitorPointContent;
using aerosim::monitor::MonitorPointShape;
using aerosim::monitor::MonitorFitMethod;
using aerosim::monitor::MonitorFitResult;

namespace {

int failures = 0;

void check(bool passed, const QString &what)
{
    if (passed)
        return;
    ++failures;
    QTextStream(stderr) << "FAIL: " << what << '\n';
}

// Compares two label lists element by element. Deliberately avoids
// QList<QString>::operator==, which instantiates QT_MAKE_CHECKED_ARRAY_ITERATOR
// and therefore needs MSVC's stdext::make_checked_array_iterator -- a helper the
// 14.5x toolsets no longer ship (Qt 5.12.12 predates its removal).
bool sameLabels(const QStringList &actual, const QStringList &expected)
{
    if (actual.size() != expected.size())
        return false;
    for (int i = 0; i < actual.size(); ++i) {
        if (actual.at(i) != expected.at(i))
            return false;
    }
    return true;
}

QString fixtureDir()
{
    return QString::fromLocal8Bit(qgetenv("AEROSIM_POSTPROCESS_FIXTURE_DIR"));
}

QString outputDir()
{
    const QString fromEnv = QString::fromLocal8Bit(qgetenv("AEROSIM_MONITOR_RENDER_DIR"));
    if (!fromEnv.isEmpty())
        return fromEnv;
    return QDir::tempPath();
}

// The pixels of the chart as it stands, without going through savePng(). Shape
// coverage is about what lands on the canvas, so the check compares rendered
// images rather than reported counts.
QImage renderImage(QWidget &widget)
{
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return image;
    image.fill(Qt::white);
    QPainter painter(&image);
    widget.render(&painter);
    return image;
}

// Mirrors what the workbench builds from the model: one curve per selected
// column, grouped by dimension so residuals and coefficients keep their own Y
// axis (plan 5.4).
QVector<MonitorCurve> buildCurves(const MonitorSeriesModel &model, const QStringList &columns,
                                  MonitorAxis axis,
                                  MonitorInterpolation interpolation = MonitorInterpolation::Linear)
{
    QVector<MonitorCurve> curves;
    int index = 0;
    for (const QString &column : columns) {
        MonitorCurve curve;
        curve.key = column;
        curve.label = column;
        curve.points = model.points(column, axis);
        curve.interpolation = interpolation;
        curve.color = QColor::fromHsv((index * 47) % 360, 190, 200);
        curve.lineStyle = (index % 4 == 0) ? Qt::SolidLine
                        : (index % 4 == 1) ? Qt::DashLine
                        : (index % 4 == 2) ? Qt::DotLine : Qt::DashDotLine;
        curve.logarithmic = model.suggestedScale(column) == MonitorScale::Logarithmic;
        curve.groupKey = model.isResidual(column) ? QStringLiteral("residual")
                                                  : QStringLiteral("coefficient");
        curve.groupTitle = model.isResidual(column) ? QStringLiteral("residual")
                                                    : QStringLiteral("coefficient");
        curves.append(curve);
        ++index;
    }
    return curves;
}

void testRendersRealHistory()
{
    const QString path = QDir(fixtureDir()).filePath(QStringLiteral("su2_history_steady.csv"));
    check(QFile::exists(path), QStringLiteral("fixture path exists: ") + path);

    MonitorLog log;
    QString error;
    check(MonitorLogReader::read(path, &log, &error),
          QStringLiteral("reader accepts the fixture: ") + error);

    MonitorSeriesModel model;
    check(model.setLog(log, &error), QStringLiteral("model accepts the log: ") + error);
    check(model.rowCount() == 79, QStringLiteral("79 data rows"));

    MonitorChartView view;
    view.resize(1200, 720);

    // One series first: a single group must produce exactly one Y axis.
    const QVector<MonitorCurve> single = buildCurves(model, QStringList() << QStringLiteral("CD"),
                                                     MonitorAxis::IterationStep);
    view.setAxisLabel(model.axisLabel(MonitorAxis::IterationStep));
    view.setCurves(single);
    check(view.visibleCurveCount() == 1, QStringLiteral("one visible curve"));
    check(view.axisCount() == 1, QStringLiteral("one Y axis for one group"));
    check(view.drawnPointCount() == 79, QStringLiteral("79 drawn points"));

    // Overlay a residual on top: two dimensions, therefore two Y axes.
    QStringList overlay;
    overlay << QStringLiteral("CD") << QStringLiteral("rms[Rho]");
    const QVector<MonitorCurve> both = buildCurves(model, overlay, MonitorAxis::IterationStep);
    view.setCurves(both);
    check(view.visibleCurveCount() == 2, QStringLiteral("two visible curves after overlay"));
    check(view.axisCount() == 2, QStringLiteral("two Y axes for two dimensions"));

    const QVector<aerosim::monitor::MonitorAxisSummary> axes = view.axisSummaries();
    check(axes.size() == 2, QStringLiteral("two axis summaries"));
    if (axes.size() == 2) {
        check(!axes.at(0).title.isEmpty(), QStringLiteral("first axis has a caption"));
        check(!axes.at(1).title.isEmpty(), QStringLiteral("second axis has a caption"));
        check(axes.at(0).onRight != axes.at(1).onRight,
              QStringLiteral("the two axes sit on opposite sides"));
        check(axes.at(0).curveCount + axes.at(1).curveCount == view.visibleCurveCount(),
              QStringLiteral("axis curve counts cover every visible curve"));
        // The fixture residuals are negative, so a log request must degrade and
        // the caption must say so instead of promising a log axis.
        check(axes.at(1).logarithmic == false,
              QStringLiteral("negative residual degrades the log axis to linear"));
    }
    check(!view.statusText().isEmpty(), QStringLiteral("status text explains the drawing"));

    // Real pixels: a chart that reports two curves but paints nothing fails here.
    // show() is required: a never-shown QwtPlot has no geometry for the renderer
    // to lay the drawing out in, and QwtPlotRenderer skips a zero-sized plot.
    view.show();
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
    const QString png = QDir(outputDir()).filePath(QStringLiteral("monitor_chart_overlay.png"));
    check(view.savePng(png), QStringLiteral("PNG written to ") + png);
    QImage image(png);
    check(!image.isNull(), QStringLiteral("PNG is a readable image"));
    if (!image.isNull()) {
        check(image.width() >= 600 && image.height() >= 400,
              QStringLiteral("PNG has a usable size"));
        // Count distinct colours as a cheap "something was drawn" proxy: an empty
        // white canvas collapses to a handful of values.
        QSet<QRgb> colours;
        for (int y = 0; y < image.height(); y += 4) {
            for (int x = 0; x < image.width(); x += 4)
                colours.insert(image.pixel(x, y));
        }
        check(colours.size() > 32,
              QStringLiteral("PNG is not a blank canvas (distinct colours: %1)")
                  .arg(colours.size()));
    }

    // A residual-only chart also has to render, and here the log request cannot
    // silently swallow the series either.
    const QVector<MonitorCurve> residuals =
        buildCurves(model, model.residualColumns(), MonitorAxis::IterationStep);
    view.setCurves(residuals);
    check(view.visibleCurveCount() == model.residualColumns().size(),
          QStringLiteral("every residual column becomes a curve"));
    check(view.axisCount() == 1, QStringLiteral("residuals share one Y axis"));
    const QString pngResidual = QDir(outputDir()).filePath(QStringLiteral("monitor_chart_residual.png"));
    check(view.savePng(pngResidual), QStringLiteral("residual PNG written to ") + pngResidual);

    // Redrawing must not accumulate scene items. Reusing one chart and calling
    // removeAxis() left the removed axes' graphics items in the scene, so the
    // second drawing showed the previous axis labels under the new ones. The
    // item count is the observable form of that bug: it used to grow past 150
    // for a four-curve chart instead of staying near the fresh-drawing value.
    {
        MonitorChartView fresh;
        fresh.resize(1200, 720);
        fresh.show();
        fresh.setCurves(residuals);
        for (int i = 0; i < 3; ++i)
            QApplication::processEvents();
        const int freshItems = fresh.sceneItemCount();
        check(freshItems > 0, QStringLiteral("a fresh chart has scene items"));

        view.setCurves(both);
        view.setCurves(residuals);
        view.show();
        for (int i = 0; i < 3; ++i)
            QApplication::processEvents();
        const int reusedItems = view.sceneItemCount();
        check(reusedItems <= freshItems + 8,
              QStringLiteral("redrawing does not accumulate scene items (fresh %1, redrawn %2)")
                  .arg(freshItems)
                  .arg(reusedItems));
    }

    // Every coefficient at once: 10 series, still one axis, still 79 points each.
    const QVector<MonitorCurve> coefficients =
        buildCurves(model, model.coefficientColumns(), MonitorAxis::IterationStep);
    view.setCurves(coefficients);
    check(view.axisCount() == 1, QStringLiteral("coefficients share one Y axis"));
    check(view.drawnPointCount() == 79 * model.coefficientColumns().size(),
          QStringLiteral("every coefficient keeps all 79 samples"));
    const QString pngCoefficients =
        QDir(outputDir()).filePath(QStringLiteral("monitor_chart_coefficients.png"));
    check(view.savePng(pngCoefficients), QStringLiteral("coefficient PNG written to ") + pngCoefficients);

    // Qwt has one scale per side, so a drawing carries two magnitudes. A third
    // group is not squeezed onto the right axis -- it is not drawn at all, and
    // the status line names it. Sharing a scale between two magnitudes is the
    // exact flattening the grouping exists to prevent, so a silent merge here
    // would re-introduce the bug this test guards against.
    if (!coefficients.isEmpty()) {
        QVector<MonitorCurve> threeGroups = coefficients.mid(0, 3);
        threeGroups[0].groupKey = QStringLiteral("group-a");
        threeGroups[0].groupTitle = QStringLiteral("group-a");
        threeGroups[1].groupKey = QStringLiteral("group-b");
        threeGroups[1].groupTitle = QStringLiteral("group-b");
        threeGroups[2].groupKey = QStringLiteral("group-c");
        threeGroups[2].groupTitle = QStringLiteral("group-c");
        view.setCurves(QVector<MonitorCurve>() << threeGroups.at(0) << threeGroups.at(1)
                                               << threeGroups.at(2));
        check(view.axisCount() == 2, QStringLiteral("never more than two Y axes"));
        // visibleCurveCount() reports the caller's visible input, which still
        // lists all three; pointSeriesCount() is what actually reached the plot.
        check(view.pointSeriesCount() == 2,
              QStringLiteral("the third magnitude group is dropped, not merged"));
        check(view.legendLabels().size() == 2,
              QStringLiteral("a dropped group contributes no legend entry"));
        check(view.statusText().contains(QStringLiteral("group-c")),
              QStringLiteral("status names the group it refused to draw"));
    }

    // Hiding everything must say so, never leave a stale chart behind.
    QVector<MonitorCurve> hidden = both;
    for (MonitorCurve &curve : hidden)
        curve.visible = false;
    view.setCurves(hidden);
    check(view.axisCount() == 0, QStringLiteral("no axis survives when every curve is hidden"));
    check(view.statusText().contains(QStringLiteral("勾选")) ||
              view.statusText().contains(QChar(0x52FE)),
          QStringLiteral("status explains why the chart is empty"));
}

// A solver record is a list of discrete samples, so the points are the primary
// mark and the joining curve is an option. These checks pin that contract: one
// point series per curve always, a line series only when interpolation asks for
// one, caller-owned captions, and a working hook for a custom joining method.
void testPointGranularityAndLabels()
{
    const QString path = QDir(fixtureDir()).filePath(QStringLiteral("su2_history_steady.csv"));
    MonitorLog log;
    QString error;
    if (!MonitorLogReader::read(path, &log, &error))
        return;
    MonitorSeriesModel model;
    if (!model.setLog(log, &error))
        return;

    MonitorChartView view;
    view.resize(1200, 720);
    view.setAxisLabel(model.axisLabel(MonitorAxis::IterationStep));

    // Points only: every curve still appears, but nothing is connected.
    const QStringList columns = QStringList() << QStringLiteral("CD") << QStringLiteral("CL");
    const QVector<MonitorCurve> raw =
        buildCurves(model, columns, MonitorAxis::IterationStep, MonitorInterpolation::None);
    view.setCurves(raw);
    check(view.visibleCurveCount() == 2, QStringLiteral("both curves are visible"));
    check(view.pointSeriesCount() == 2, QStringLiteral("one point series per curve"));
    check(view.lineSeriesCount() == 0, QStringLiteral("no curve is drawn when interpolation is None"));
    check(view.statusText().contains(QChar(0x79BB)), // "discrete samples are not joined" notice
          QStringLiteral("status says the samples are not joined"));
    check(sameLabels(view.legendLabels(), columns),
          QStringLiteral("legend carries the curve labels"));

    // Linear joining adds exactly one line series per curve on top of the points.
    const QVector<MonitorCurve> linear =
        buildCurves(model, columns, MonitorAxis::IterationStep, MonitorInterpolation::Linear);
    view.setCurves(linear);
    check(view.pointSeriesCount() == 2, QStringLiteral("points survive a joined drawing"));
    check(view.lineSeriesCount() == 2, QStringLiteral("linear joining draws one curve per series"));

    // Spline joins through the same samples; the mark count never changes.
    const QVector<MonitorCurve> spline =
        buildCurves(model, columns, MonitorAxis::IterationStep, MonitorInterpolation::Spline);
    view.setCurves(spline);
    check(view.pointSeriesCount() == 2, QStringLiteral("spline keeps the sample marks"));
    check(view.lineSeriesCount() == 2, QStringLiteral("spline draws a joining curve"));

    // Point appearance is per curve, so a reader can tell a record's quantities
    // apart even in black and white.
    QVector<MonitorCurve> shaped = raw;
    shaped[0].pointShape = MonitorPointShape::Square;
    shaped[0].pointSize = 9.0;
    view.setCurves(shaped);
    check(view.pointSeriesCount() == 2, QStringLiteral("marker shape does not change the count"));

    // The caption is a caller decision: one record can carry several physical
    // quantities and one quantity can span several records.
    view.setLabelResolver([](const MonitorCurve &curve) {
        return QStringLiteral("history.csv · %1").arg(curve.key);
    });
    view.setCurves(raw);
    check(view.legendLabels().size() == 2, QStringLiteral("one legend entry per curve"));
    check(!view.legendLabels().isEmpty() &&
              view.legendLabels().first().startsWith(QStringLiteral("history.csv")),
          QStringLiteral("the caller's label resolver wins over the column name"));

    // An empty resolver answer means "no opinion" and falls back to the label.
    view.setLabelResolver([](const MonitorCurve &) { return QString(); });
    view.setCurves(raw);
    check(sameLabels(view.legendLabels(), columns),
          QStringLiteral("an empty resolver answer falls back to the curve label"));

    // Reserved hook for a joining method the built-in set does not cover.
    int factoryCalls = 0;
    view.setCurveFactory([&factoryCalls](const MonitorCurve &curve, QwtPlot *plot) -> QwtPlotCurve * {
        ++factoryCalls;
        (void)plot; // the view attaches the item; the factory only shapes it
        auto *item = new QwtPlotCurve(curve.key);
        item->setStyle(QwtPlotCurve::Lines);
        return item;
    });
    view.setCurves(linear);
    check(factoryCalls == 2, QStringLiteral("the curve factory is asked for every joined curve"));
    check(view.lineSeriesCount() == 2,
          QStringLiteral("factory-produced curves are counted like built-in ones"));
    view.setCurveFactory(MonitorChartView::CurveFactory());

    // A decimated drawing must say so rather than silently dropping samples.
    view.setPointBudget(20);
    view.setCurves(raw);
    check(view.statusText().contains(QChar(0x62BD)), // the decimation notice
          QStringLiteral("status reports decimation when the budget bites"));
    const QString pngPoints =
        QDir(outputDir()).filePath(QStringLiteral("monitor_chart_points.png"));
    check(view.savePng(pngPoints), QStringLiteral("points-only PNG written to ") + pngPoints);
    view.setPointBudget(0);
}

// The UI contract other members program against: one physical quantity per data
// source is a different curve even though it shares a column name, and every
// curve's marks and stroke can be re-styled at run time without redrawing.
void testLiveStyleEditing()
{
    const QString path = QDir(fixtureDir()).filePath(QStringLiteral("su2_history_steady.csv"));
    MonitorLog log;
    QString error;
    if (!MonitorLogReader::read(path, &log, &error))
        return;

    MonitorSeriesModel model;
    if (!model.setLog(log, &error))
        return;

    // Two "datasets" with the same columns. A baseline and a variant both report
    // CD; they must not collapse into one curve.
    QVector<MonitorCurve> curves =
        buildCurves(model, QStringList() << QStringLiteral("CD") << QStringLiteral("CL"),
                    MonitorAxis::IterationStep, MonitorInterpolation::None);
    for (int i = 0; i < curves.size(); ++i) {
        curves[i].datasetId = QStringLiteral("baseline");
        curves[i].label = QStringLiteral("baseline · ") + curves[i].key;
    }
    QVector<MonitorCurve> variant;
    int variantIndex = 0;
    for (MonitorCurve curve : curves) {
        curve.datasetId = QStringLiteral("variant");
        curve.label = QStringLiteral("variant · ") + curve.key;
        // Same column names, different data: offset so the two are distinguishable.
        for (int i = 0; i < curve.points.size(); ++i)
            curve.points[i].setY(curve.points.at(i).y() * 1.05);
        // The widget does not colour-code by identity -- that stays the caller's
        // decision. Offsetting the hues here shows why it has to be: two data
        // sources reporting the same column would otherwise look identical.
        curve.color = QColor::fromHsv((120 + variantIndex * 47) % 360, 190, 200);
        ++variantIndex;
        variant.append(curve);
    }
    QVector<MonitorCurve> both = curves;
    both += variant;

    MonitorChartView view;
    view.resize(1200, 720);
    view.setAxisLabel(model.axisLabel(MonitorAxis::IterationStep));
    view.setCurves(both);

    check(view.visibleCurveCount() == 4,
          QStringLiteral("four curves: two quantities from two data sources"));
    check(view.curveIds().size() == 4, QStringLiteral("every curve gets an identity"));
    check(view.curveIds().at(0) != view.curveIds().at(2),
          QStringLiteral("the same column from two sources is not one curve"));
    check(view.curveIds().at(0).startsWith(QStringLiteral("baseline")),
          QStringLiteral("identity names the source first"));
    check(view.pointSeriesCount() == 4, QStringLiteral("one marker series per curve"));

    const QString baselineCd =
        MonitorChartView::makeCurveId(QStringLiteral("baseline"), QStringLiteral("CD"));
    const QString variantCd =
        MonitorChartView::makeCurveId(QStringLiteral("variant"), QStringLiteral("CD"));

    // ---- Style edits must not rebuild the chart ----------------------------
    const int buildsBefore = view.chartBuildCount();
    int styleSignals = 0;
    QObject::connect(&view, &MonitorChartView::curveStyleChanged,
                     [&styleSignals](const QString &) { ++styleSignals; });

    aerosim::monitor::MonitorCurveStyle style = view.curveStyle(baselineCd);
    check(style.label.startsWith(QStringLiteral("baseline")),
          QStringLiteral("the current style reports the current caption"));
    style.color = QColor(255, 64, 32);
    style.lineStyle = Qt::DashDotLine;
    style.pointSize = 9.0;
    style.pointShape = MonitorPointShape::Square;
    check(view.setCurveStyle(baselineCd, style),
          QStringLiteral("a known curve accepts a style edit"));
    check(view.chartBuildCount() == buildsBefore,
          QStringLiteral("editing one curve does not rebuild the chart"));
    check(styleSignals == 1, QStringLiteral("the style edit is announced once"));

    const aerosim::monitor::MonitorCurveStyle after = view.curveStyle(baselineCd);
    check(after.color == QColor(255, 64, 32), QStringLiteral("the new colour is stored"));
    check(after.lineStyle == Qt::DashDotLine, QStringLiteral("the new stroke is stored"));
    check(after.pointSize == 9.0, QStringLiteral("the new marker size is stored"));
    check(after.pointShape == MonitorPointShape::Square,
          QStringLiteral("the new marker shape is stored"));

    // The untouched curve keeps its own appearance.
    const aerosim::monitor::MonitorCurveStyle other = view.curveStyle(variantCd);
    check(other.color != QColor(255, 64, 32),
          QStringLiteral("editing one curve leaves the others alone"));
    check(other.pointSize == 5.0, QStringLiteral("the other marker size is untouched"));

    // ---- Caption edits -----------------------------------------------------
    check(view.setCurveLabel(baselineCd, QStringLiteral("基准算例 CD")),
          QStringLiteral("a known curve accepts a caption edit"));
    const QStringList labels = view.legendLabels();
    check(labels.size() == 4, QStringLiteral("four legend entries after the edit"));
    check(labels.contains(QStringLiteral("基准算例 CD")),
          QStringLiteral("the new caption replaces the old one"));
    check(!view.setCurveLabel(QStringLiteral("no-such-curve"), QStringLiteral("x")),
          QStringLiteral("an unknown curve rejects a caption edit"));

    // ---- Selection survives a style edit -----------------------------------
    view.setSelectedCurve(baselineCd);
    check(view.selectedCurve() == baselineCd, QStringLiteral("a curve can be selected"));
    const int buildsBeforeSelection = view.chartBuildCount();
    aerosim::monitor::MonitorCurveStyle highlight = view.curveStyle(baselineCd);
    highlight.pointSize = 4.0;
    check(view.setCurveStyle(baselineCd, highlight), QStringLiteral("style edit after selection"));
    check(view.chartBuildCount() == buildsBeforeSelection,
          QStringLiteral("a style edit does not reset the selection"));
    check(view.selectedCurve() == baselineCd,
          QStringLiteral("the selection survives the style edit"));

    // Re-selecting a different curve moves the highlight.
    view.setSelectedCurve(variantCd);
    check(view.selectedCurve() == variantCd, QStringLiteral("the selection can move"));
    view.setSelectedCurve(QString());

    // ---- Sample-level selection -------------------------------------------
    const QVector<QPointF> cdPoints = model.points(QStringLiteral("CD"), MonitorAxis::IterationStep);
    check(!cdPoints.isEmpty(), QStringLiteral("the fixture has CD samples"));
    if (!cdPoints.isEmpty()) {
        const int middle = cdPoints.size() / 2;
        check(view.sampleIndexAt(baselineCd, cdPoints.at(middle)) == middle,
              QStringLiteral("a clicked sample resolves to its own index"));
        view.setSelectedPoint(baselineCd, middle);
        check(view.selectedPointCurve() == baselineCd
                  && view.selectedPointSample() == middle,
              QStringLiteral("the selected sample is remembered"));
        check(view.sampleIndexAt(QStringLiteral("no-such-curve"), cdPoints.at(0)) == -1,
              QStringLiteral("an unknown curve has no sample"));
    }

    // ---- Interpolation is the one edit that replaces a series -------------
    const int linesBefore = view.lineSeriesCount();
    check(linesBefore == 0, QStringLiteral("points-only drawing has no joining curve"));
    aerosim::monitor::MonitorCurveStyle joined = view.curveStyle(variantCd);
    joined.interpolation = MonitorInterpolation::Linear;
    check(view.setCurveStyle(variantCd, joined),
          QStringLiteral("the joining method can be changed at run time"));
    check(view.lineSeriesCount() == 1,
          QStringLiteral("only that curve gains a joining line"));
    check(view.pointSeriesCount() == 4,
          QStringLiteral("the marker series are not rebuilt by a joining change"));
    check(view.chartBuildCount() == buildsBefore,
          QStringLiteral("changing the joining method still does not rebuild the chart"));
    check(view.curveStyle(variantCd).interpolation == MonitorInterpolation::Linear,
          QStringLiteral("the new joining method is stored"));

    // Going back to points only removes just that joining line again.
    joined.interpolation = MonitorInterpolation::None;
    view.setCurveStyle(variantCd, joined);
    check(view.lineSeriesCount() == 0, QStringLiteral("the joining line can be removed again"));

    // ---- Clicks resolve to a curve and a sample ---------------------------
    int clickedCurves = 0;
    int clickedPoints = 0;
    QString lastClickedId;
    int lastClickedSample = -1;
    QObject::connect(&view, &MonitorChartView::curveClicked,
                     [&clickedCurves, &lastClickedId](const QString &id) {
                         ++clickedCurves;
                         lastClickedId = id;
                     });
    QObject::connect(&view, &MonitorChartView::pointClicked,
                     [&clickedPoints, &lastClickedSample](const QString &, int index, const QPointF &) {
                         ++clickedPoints;
                         lastClickedSample = index;
                     });
    if (!cdPoints.isEmpty())
        view.selectAt(baselineCd, cdPoints.at(3));
    check(clickedCurves == 1, QStringLiteral("a click reports the curve"));
    check(lastClickedId == baselineCd, QStringLiteral("the click names the right curve"));
    check(clickedPoints == 1 && lastClickedSample == 3,
          QStringLiteral("the click names the right sample"));

    // ---- A fresh data set is a fresh set of subjects -----------------------
    // Select the curve that is about to disappear: only then does the drop rule
    // have anything to prove. (A selection of a surviving curve must be kept,
    // which the style-edit check above already covers.)
    view.setSelectedCurve(variantCd);
    check(view.selectedCurve() == variantCd,
          QStringLiteral("the vanishing curve is selected before the redraw"));
    view.setCurves(curves);
    check(view.visibleCurveCount() == 2, QStringLiteral("the new data set replaces the old one"));
    check(view.curveIds().size() == 2, QStringLiteral("identities follow the new data set"));
    check(view.selectedCurve().isEmpty(),
          QStringLiteral("a selection of a vanished curve is dropped"));

    const QString png =
        QDir(outputDir()).filePath(QStringLiteral("monitor_chart_restyled.png"));
    view.setCurves(both);
    aerosim::monitor::MonitorCurveStyle accent = view.curveStyle(baselineCd);
    accent.color = QColor(214, 39, 40);
    accent.lineStyle = Qt::SolidLine;
    accent.pointSize = 8.0;
    accent.pointShape = MonitorPointShape::Square;
    accent.interpolation = MonitorInterpolation::Linear;
    view.setCurveStyle(baselineCd, accent);
    view.setSelectedCurve(baselineCd);
    check(view.savePng(png), QStringLiteral("restyled PNG written to ") + png);
}

void testDegenerateTimeAxis()
{
    const QString path = QDir(fixtureDir()).filePath(QStringLiteral("su2_history_steady.csv"));
    MonitorLog log;
    QString error;
    if (!MonitorLogReader::read(path, &log, &error))
        return;

    MonitorSeriesModel model;
    if (!model.setLog(log, &error))
        return;

    // This fixture is a steady run: Time_Iter never advances, so the time axis is
    // degenerate and must be reported as such rather than drawn as one point.
    QString reason;
    const bool available = model.axisAvailable(MonitorAxis::TimeIndex, &reason);
    check(!available, QStringLiteral("time axis is unavailable for a steady run"));
    check(!reason.isEmpty(), QStringLiteral("the unavailable axis explains itself"));
    check(model.axisAvailable(MonitorAxis::IterationStep),
          QStringLiteral("the iteration axis stays available"));
}

// A settings panel reasons in scopes -- "the residuals", "these three
// coefficients" -- so the widget has to own the iteration. This covers the
// interval (a view property the user sets) and the three scopes, including the
// part that is easy to get wrong: a partial patch must not stamp every field.
void testScopedStyleEdits()
{
    const QString path = QDir(fixtureDir()).filePath(QStringLiteral("su2_history_steady.csv"));
    MonitorLog log;
    QString error;
    if (!MonitorLogReader::read(path, &log, &error))
        return;

    MonitorSeriesModel model;
    if (!model.setLog(log, &error))
        return;

    const QStringList columns = QStringList()
        << QStringLiteral("rms[Rho]") << QStringLiteral("rms[RhoU]")
        << QStringLiteral("rms[RhoV]") << QStringLiteral("rms[RhoE]")
        << QStringLiteral("CD") << QStringLiteral("CL") << QStringLiteral("CEff");
    QVector<MonitorCurve> curves = buildCurves(model, columns, MonitorAxis::IterationStep,
                                               MonitorInterpolation::None);
    for (MonitorCurve &curve : curves)
        curve.datasetId = QStringLiteral("scoped");

    MonitorChartView view;
    view.resize(1200, 720);
    view.setAxisLabel(model.axisLabel(MonitorAxis::IterationStep));
    view.setCurves(curves);

    const QString residualKey = QStringLiteral("residual");
    const QString coefficientKey = QStringLiteral("coefficient");
    check(view.groupKeys().size() == 2, QStringLiteral("two magnitude groups are reported"));
    check(view.curveIdsInGroup(residualKey).size() == 4,
          QStringLiteral("the residual group lists its four curves"));
    check(view.curveIdsInGroup(coefficientKey).size() == 3,
          QStringLiteral("the coefficient group lists its three curves"));

    const int totalSamples = 79;
    const QString residualId = view.curveIdsInGroup(residualKey).first();
    const QString coefficientId = view.curveIdsInGroup(coefficientKey).first();
    check(view.drawnSampleCount(residualId) == totalSamples,
          QStringLiteral("an untouched curve draws every sample"));

    // ---- Interval scope: a whole group -------------------------------------
    const int buildsBeforeGroup = view.chartBuildCount();
    check(view.setSampleIntervalForGroup(residualKey, 5) == 4,
          QStringLiteral("a group scope reaches every curve of that group"));
    check(view.chartBuildCount() == buildsBeforeGroup + 1,
          QStringLiteral("a sampling interval change rebuilds the drawing once"));
    check(view.drawnSampleCount(residualId) == 17,
          QStringLiteral("every fifth sample plus the last one is drawn"));
    check(view.drawnSampleCount(coefficientId) == totalSamples,
          QStringLiteral("a group edit leaves the other group alone"));
    check(view.curveStyle(residualId).sampleInterval == 5,
          QStringLiteral("the interval is readable back as a style field"));

    // ---- Interval scope: an explicit list of curves -------------------------
    const QStringList pair = view.curveIdsInGroup(coefficientKey).mid(0, 2);
    check(view.setSampleIntervalForCurves(pair, 3) == 2,
          QStringLiteral("a curve list scope reaches exactly the listed curves"));
    check(view.drawnSampleCount(pair.at(0)) == 27,
          QStringLiteral("every third sample is drawn"));
    check(view.drawnSampleCount(view.curveIdsInGroup(coefficientKey).at(2)) == totalSamples,
          QStringLiteral("a curve outside the list keeps its own interval"));

    // The interval belongs to the view: a redraw with the same data must not
    // silently reset what the user chose.
    view.setCurves(curves);
    check(view.drawnSampleCount(residualId) == 17,
          QStringLiteral("the interval survives a redraw with the same data"));

    // ---- Partial patch: only the flagged field is written -------------------
    aerosim::monitor::MonitorChartView::StylePatch sizePatch;
    sizePatch.hasPointSize = true;
    sizePatch.pointSize = 11.0;
    const QColor residualColorBefore = view.curveStyle(residualId).color;
    check(view.applyStyle(aerosim::monitor::MonitorChartView::EditScope::Curves, sizePatch,
                          QString(), pair) == 2,
          QStringLiteral("a partial patch reports the curves it changed"));
    check(view.curveStyle(pair.at(0)).pointSize == 11.0,
          QStringLiteral("the flagged field is written"));
    check(!sizePatch.hasColor,
          QStringLiteral("a marker-size patch carries no colour to stamp over the group"));
    check(view.curveStyle(residualId).color == residualColorBefore,
          QStringLiteral("curves outside the scope keep their colour"));
    check(view.curveStyle(residualId).pointSize == 5.0,
          QStringLiteral("curves outside the scope keep their marker size"));

    // ---- Group scope: one appearance field across a group -------------------
    aerosim::monitor::MonitorChartView::StylePatch colorPatch;
    colorPatch.hasColor = true;
    colorPatch.color = QColor(214, 39, 40);
    check(view.applyStyle(aerosim::monitor::MonitorChartView::EditScope::Group, colorPatch,
                          residualKey) == 4,
          QStringLiteral("a group colour edit reaches all four residual curves"));
    for (const QString &id : view.curveIdsInGroup(residualKey)) {
        check(view.curveStyle(id).color == QColor(214, 39, 40),
              QStringLiteral("every curve in the group took the colour"));
    }
    check(view.curveStyle(coefficientId).color != QColor(214, 39, 40),
          QStringLiteral("a group colour edit does not leak to the other group"));

    // ---- Captions the user typed survive a scoped rebuild -------------------
    view.setCurveLabel(residualId, QStringLiteral("residual (renamed)"));
    aerosim::monitor::MonitorChartView::StylePatch intervalPatch;
    intervalPatch.hasSampleInterval = true;
    intervalPatch.sampleInterval = 2;
    view.applyStyle(aerosim::monitor::MonitorChartView::EditScope::Curves, intervalPatch,
                    QString(), QStringList() << residualId);
    check(view.curveStyle(residualId).label == QStringLiteral("residual (renamed)"),
          QStringLiteral("a scoped rebuild keeps the caption the user set"));
    check(view.drawnSampleCount(residualId) == 40,
          QStringLiteral("every second sample plus the last one is drawn"));

    // ---- Global scope ------------------------------------------------------
    aerosim::monitor::MonitorChartView::StylePatch hidePatch;
    hidePatch.hasVisible = true;
    hidePatch.visible = false;
    check(view.applyStyle(aerosim::monitor::MonitorChartView::EditScope::All, hidePatch) == 7,
          QStringLiteral("the global scope reaches every curve"));
    check(view.visibleCurveCount() == 0, QStringLiteral("nothing is drawn once all are hidden"));

    // A hidden curve has no live item, so the interval has to be remembered by
    // the view rather than read back off the drawing: show it again and look.
    aerosim::monitor::MonitorChartView::StylePatch showPatch;
    showPatch.hasVisible = true;
    showPatch.visible = true;
    view.applyStyle(aerosim::monitor::MonitorChartView::EditScope::Curves, showPatch,
                    QString(), QStringList() << residualId);
    check(view.curveStyle(residualId).sampleInterval == 2,
          QStringLiteral("hiding does not forget the sampling interval"));
    check(view.drawnSampleCount(residualId) == 40,
          QStringLiteral("the remembered interval is applied again on redraw"));
}

// Every marker shape the public enum names has to reach the canvas. A shape that
// maps to nothing would pass a compile, a count check and a status check and
// still draw an empty plot, so the proof is one rendered image per shape: the
// bytes of the marks-only chart must differ from the chart with the marks
// switched off, and none of them may be blank.
void testEveryMarkerShapeDraws()
{
    const QString path = QDir(fixtureDir()).filePath(QStringLiteral("su2_history_steady.csv"));
    MonitorLog log;
    QString error;
    if (!MonitorLogReader::read(path, &log, &error))
        return;

    MonitorSeriesModel model;
    if (!model.setLog(log, &error))
        return;

    const QStringList columns = model.residualColumns();
    if (columns.isEmpty())
        return;

    MonitorChartView view;
    view.resize(900, 540);

    QVector<MonitorCurve> base =
        buildCurves(model, columns.mid(0, 1), MonitorAxis::IterationStep, MonitorInterpolation::None);

    // The reference: the same chart with no marker at all. Every other shape has
    // to add pixels on top of it.
    base[0].pointShape = MonitorPointShape::None;
    base[0].pointSize = 9.0;
    view.setCurves(base);
    const QImage bare = renderImage(view);
    check(!bare.isNull(), QStringLiteral("the marks-off reference renders"));
    check(view.pointSeriesCount() == 1, QStringLiteral("a marker-free curve still draws its series"));

    // Drawing content for the four shapes that cannot be generated from a size.
    MonitorPointContent content;
    QPainterPath path2;
    path2.moveTo(0.5, 0.0);
    path2.lineTo(1.0, 0.6);
    path2.lineTo(0.8, 1.0);
    path2.lineTo(0.2, 1.0);
    path2.lineTo(0.0, 0.6);
    path2.closeSubpath();
    content.path = path2;
    QImage stamp(16, 16, QImage::Format_ARGB32_Premultiplied);
    stamp.fill(Qt::transparent);
    {
        QPainter stampPainter(&stamp);
        stampPainter.setRenderHint(QPainter::Antialiasing, true);
        stampPainter.setBrush(QColor(Qt::darkGreen));
        stampPainter.setPen(QPen(QColor(Qt::black), 2.0));
        stampPainter.drawEllipse(1, 1, 14, 14);
        stampPainter.drawLine(1, 1, 14, 14);
    }
    content.image = stamp;
    content.svg = QByteArray(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\" width=\"10\" height=\"10\">"
        "<circle cx=\"5\" cy=\"5\" r=\"4\" fill=\"#204080\" stroke=\"#000000\"/>"
        "</svg>");

    const MonitorPointShape shapes[] = {
        MonitorPointShape::Circle,        MonitorPointShape::Square,
        MonitorPointShape::Diamond,       MonitorPointShape::Triangle,
        MonitorPointShape::DownTriangle,  MonitorPointShape::UpTriangle,
        MonitorPointShape::LeftTriangle,  MonitorPointShape::RightTriangle,
        MonitorPointShape::Cross,         MonitorPointShape::XCross,
        MonitorPointShape::HLine,         MonitorPointShape::VLine,
        MonitorPointShape::Star1,         MonitorPointShape::Star2,
        MonitorPointShape::Hexagon,       MonitorPointShape::Path,
        MonitorPointShape::Pixmap,        MonitorPointShape::Graphic,
        MonitorPointShape::Svg
    };

    // A gallery series: few samples, large markers. The real history has 79
    // samples, which at any readable marker size overplot into a solid band and
    // hides the very shapes this test exists to prove.
    QVector<MonitorCurve> gallery;
    MonitorCurve sample;
    sample.key = QStringLiteral("marker");
    sample.label = QStringLiteral("marker");
    sample.datasetId = QStringLiteral("shape-gallery");
    sample.color = QColor(30, 90, 170);
    for (int i = 0; i <= 5; ++i)
        sample.points.append(QPointF(i, 1.0 + 0.8 * i - 0.12 * i * i));
    sample.interpolation = MonitorInterpolation::None;
    sample.pointSize = 14.0;
    sample.groupKey = QStringLiteral("marker");
    sample.groupTitle = QStringLiteral("marker");
    gallery.append(sample);

    int drawn = 0;
    const QString shapeDir = outputDir();
    for (MonitorPointShape shape : shapes) {
        // The style must survive being read back, which is what a UI relies on
        // when it reopens the panel for a curve.
        QVector<MonitorCurve> curves = base;
        curves[0].pointShape = shape;
        curves[0].pointContent = content;
        curves[0].pointSize = 11.0;
        view.setCurves(curves);
        const MonitorCurveStyle stored = view.curveStyle(MonitorChartView::curveId(curves.at(0)));
        check(stored.pointShape == shape,
              QStringLiteral("shape %1 survives setCurves()").arg(int(shape)));

        const QImage withMarks = renderImage(view);
        check(!withMarks.isNull(), QStringLiteral("shape %1 renders").arg(int(shape)));
        check(withMarks != bare,
              QStringLiteral("shape %1 actually puts marks on the canvas").arg(int(shape)));
        ++drawn;

        // The readable artefact for this shape, on the small gallery series.
        QVector<MonitorCurve> one = gallery;
        one[0].pointShape = shape;
        one[0].pointContent = content;
        view.setCurves(one);
        check(view.pointSeriesCount() == 1,
              QStringLiteral("gallery draws shape %1").arg(int(shape)));
        const QString png =
            QDir(shapeDir).filePath(QStringLiteral("monitor_shape_%1.png").arg(int(shape), 2, 10, QLatin1Char('0')));
        check(view.savePng(png, QSize(480, 300)),
              QStringLiteral("gallery PNG for shape %1 written").arg(int(shape)));
    }

    check(drawn == int(sizeof(shapes) / sizeof(shapes[0])),
          QStringLiteral("every declared marker shape was exercised"));

    // One panel image showing all of them at once, so a reader can compare them
    // without opening nineteen files.
    QVector<MonitorCurve> sheet;
    const QStringList sheetColumns = columns.mid(0, 4);
    const QVector<MonitorCurve> four =
        buildCurves(model, sheetColumns, MonitorAxis::IterationStep, MonitorInterpolation::None);
    const MonitorPointShape sheetShapes[] = {
        MonitorPointShape::Circle, MonitorPointShape::Square, MonitorPointShape::Diamond,
        MonitorPointShape::Triangle
    };
    for (int i = 0; i < four.size() && i < 4; ++i) {
        MonitorCurve curve = four.at(i);
        curve.pointShape = sheetShapes[i];
        curve.pointSize = 9.0;
        sheet.append(curve);
    }
    view.setCurves(sheet);
    check(view.pointSeriesCount() == 4, QStringLiteral("a mixed-shape drawing keeps every series"));
}

// A refused fit has to be reported, not just left undrawn.
//
// The widget used to store a fit result only when a trend was actually produced,
// so fitResult() answered with a default-constructed value for a refusal and a
// caller could not tell "the fit was rejected" from "no fit was asked for". The
// note explaining the refusal was discarded exactly when it was the only thing
// worth showing. Both the full-rebuild path and the live style-edit path had the
// bug; they are separate code and are checked separately.
void testRefusedFitIsReported()
{
    MonitorChartView view;
    view.resize(1000, 600);

    // Eight samples over five distinct x values cannot carry a degree-7
    // polynomial: the design matrix has rank 5, not 8.
    MonitorCurve refused;
    refused.key = QStringLiteral("CD");
    refused.datasetId = QStringLiteral("degenerate");
    refused.label = QStringLiteral("CD");
    const double xs[8] = {0.0, 1.0, 1.0, 2.0, 2.0, 2.0, 3.0, 4.0};
    for (int i = 0; i < 8; ++i)
        refused.points.append(QPointF(xs[i], 1.0 + 0.1 * i));
    refused.fit.method = MonitorFitMethod::Polynomial;
    refused.fit.degree = 7;

    view.setCurves({refused});
    const QString id = MonitorChartView::curveId(refused);
    const MonitorFitResult viaSetCurves = view.fitResult(id);
    check(!viaSetCurves.isValid(), QStringLiteral("a refused fit stays refused on the widget"));
    check(!viaSetCurves.note.isEmpty(),
          QStringLiteral("setCurves() keeps the note explaining a refused fit"));
    check(view.fitSeriesCount() == 0, QStringLiteral("a refused fit draws no trend"));

    // The same request through the live edit path, which rebuilds only the
    // fitted layer rather than the whole drawing.
    MonitorCurveStyle impossible;
    impossible.fit.method = MonitorFitMethod::Polynomial;
    impossible.fit.degree = 7;
    view.setCurveStyle(id, impossible);
    const MonitorFitResult viaStyle = view.fitResult(id);
    check(!viaStyle.isValid() && !viaStyle.note.isEmpty(),
          QStringLiteral("setCurveStyle() keeps the note explaining a refused fit"));
    check(view.fitSeriesCount() == 0, QStringLiteral("the live edit path draws no trend either"));

    // And a request that is simply switched off must still report nothing, so the
    // two states stay distinguishable.
    MonitorCurveStyle off;
    off.fit.method = MonitorFitMethod::None;
    view.setCurveStyle(id, off);
    check(!view.fitResult(id).isValid(),
          QStringLiteral("switching the fit off reports no fit at all"));
}

// A live feed delivers rows faster than a frame, so the widget must not pay a
// full axis refit per row. These checks fail if appendPoints() goes back to
// calling replot() synchronously -- which is exactly what coalescing replaced.
void testLiveBurstCoalescesRepaints()
{
    MonitorChartView view;
    view.resize(1000, 600);

    MonitorCurve curve;
    curve.key = QStringLiteral("CD");
    curve.datasetId = QStringLiteral("burst");
    curve.label = QStringLiteral("CD");
    for (int i = 0; i < 10; ++i)
        curve.points.append(QPointF(i, 0.5 - 0.01 * i));

    view.setCurves({curve});
    const QString id = MonitorChartView::curveId(curve);
    const int afterBuild = view.replotCount();
    check(afterBuild >= 1, QStringLiteral("building the drawing repaints it once"));

    // The burst: no event loop turn is given, which is what a feed that delivers
    // a batch per slot actually looks like.
    constexpr int kBurst = 500;
    QVector<QPointF> batch;
    for (int i = 0; i < kBurst; ++i) {
        batch.clear();
        batch.append(QPointF(10.0 + i, 0.4 - 0.0001 * i));
        check(view.appendPoints(id, batch),
              QStringLiteral("every appended point is accepted"));
    }

    const int duringBurst = view.replotCount();
    check(duringBurst == afterBuild,
          QStringLiteral("a burst does not repaint once per point (%1 repaints for %2 appends)")
              .arg(duringBurst - afterBuild)
              .arg(kBurst));

    // The data itself must not be dropped just because the paint was deferred.
    check(view.curvePoints(id).size() == 10 + kBurst,
          QStringLiteral("the deferred frame still holds every appended point"));
    check(view.drawnSampleCount(id) >= 10 + kBurst,
          QStringLiteral("the drawn sample count follows the appended points"));

    // Now let the owed frame run. One wait is enough for the single-shot timer.
    QElapsedTimer wait;
    wait.start();
    while (view.replotCount() == afterBuild && wait.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    const int afterFlush = view.replotCount();
    check(afterFlush > afterBuild,
          QStringLiteral("the owed frame is drawn rather than dropped"));
    check(afterFlush - afterBuild <= 3,
          QStringLiteral("%1 appends coalesce into a handful of repaints (%2)")
              .arg(kBurst)
              .arg(afterFlush - afterBuild));

    // savePng() renders through QwtPlotRenderer, which reads the plot directly:
    // a frame still owed at that moment would be captured as the previous one.
    const int beforeRender = view.replotCount();
    batch.clear();
    batch.append(QPointF(9999.0, 0.3));
    view.appendPoints(id, batch);
    const QString rendered = outputDir() + QStringLiteral("/burst_after_append.png");
    check(view.savePng(rendered, QSize(900, 540)),
          QStringLiteral("a render after an append succeeds"));
    check(view.replotCount() > beforeRender,
          QStringLiteral("savePng() flushes the owed frame before rendering"));
}

// Every sample lives in exactly one place. The widget used to keep a second copy
// in its by-id metadata map, which Qt's implicit sharing left pointing at the same
// vector until the next append detached it -- doubling the memory each live curve
// cost while changing nothing observable. Deduplicating that is only safe if the
// by-id reads still see the samples, so these checks pin the observable contract:
// reads through the id keep returning every point, no matter how often the feeding
// side appends or how style edits shuffle the metadata around.
void testSamplesLiveInOneStore()
{
    MonitorChartView view;
    view.resize(1000, 600);

    MonitorCurve curve;
    curve.key = QStringLiteral("CD");
    curve.datasetId = QStringLiteral("oneshop");
    curve.label = QStringLiteral("CD");
    for (int i = 0; i < 5; ++i)
        curve.points.append(QPointF(i, 0.5 - 0.01 * i));

    view.setCurves({curve});
    const QString id = MonitorChartView::curveId(curve);
    check(view.curvePoints(id).size() == 5,
          QStringLiteral("the initial samples are readable by id"));

    // Append in several rounds rather than one: a detached second copy would show
    // up as the first round's points going missing from one of the two views.
    for (int round = 0; round < 4; ++round) {
        QVector<QPointF> batch;
        for (int i = 0; i < 10; ++i)
            batch.append(QPointF(5.0 + round * 10 + i, 0.4 - 0.0001 * i));
        check(view.appendPoints(id, batch),
              QStringLiteral("round %1 is accepted").arg(round));
    }

    const QVector<QPointF> all = view.curvePoints(id);
    check(all.size() == 5 + 40,
          QStringLiteral("every appended point survives in the id lookup (%1 of %2)")
              .arg(all.size()).arg(45));
    check(all.first() == QPointF(0.0, 0.5) && all.last() == QPointF(44.0, 0.4 - 0.0001 * 9),
          QStringLiteral("the surviving samples are the ones that were fed in"));
    check(view.drawnSampleCount(id) >= 45,
          QStringLiteral("drawnSampleCount still counts them"));
    check(view.drawnPointCount() == 45,
          QStringLiteral("drawnPointCount still counts them"));

    // Point lookup by value goes through the same store; a broken index there
    // falls back to "nearest" rather than failing loudly, so it needs an explicit
    // check while the full 45 samples are still the ones in hand.
    check(view.sampleIndexAt(id, QPointF(44.0, 0.4 - 0.0001 * 9)) == 44,
          QStringLiteral("sampleIndexAt finds a known point by value"));

    // A style edit rewrites the metadata the dedup split out. The samples must not
    // be disturbed by that, which is the failure mode this layout invites.
    MonitorCurveStyle style = view.curveStyle(id);
    style.color = QColor(Qt::darkGreen);
    check(view.setCurveStyle(id, style),
          QStringLiteral("restyling the curve succeeds"));
    check(view.curvePoints(id).size() == 45,
          QStringLiteral("a style edit does not discard the samples"));
    check(view.curveStyle(id).color == QColor(Qt::darkGreen),
          QStringLiteral("the style edit is visible through the id lookup"));

    // A rebuild hands the view a fresh curve list. The samples must be picked up
    // from it rather than left behind in whatever the previous list owned.
    QVector<MonitorCurve> rebuilt;
    MonitorCurve replacement = curve;
    for (int i = 0; i < 8; ++i)
        replacement.points.append(QPointF(i, 0.25));
    rebuilt.append(replacement);
    view.setCurves(rebuilt);
    check(view.curvePoints(id).size() == 13,
          QStringLiteral("a rebuild serves the new samples by id (%1 of %2)")
              .arg(view.curvePoints(id).size()).arg(13));
    check(view.visibleCurveCount() == 1 && view.drawnPointCount() == 13,
          QStringLiteral("a rebuild recounts the curves it now holds"));
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    testRendersRealHistory();
    testPointGranularityAndLabels();
    testLiveStyleEditing();
    testDegenerateTimeAxis();
    testScopedStyleEdits();
    testEveryMarkerShapeDraws();
    testRefusedFitIsReported();
    testLiveBurstCoalescesRepaints();
    testSamplesLiveInOneStore();

    if (failures == 0) {
        QTextStream(stdout) << "Monitor chart render OK; images in " << outputDir() << '\n';
        return 0;
    }
    QTextStream(stderr) << failures << " monitor chart check(s) failed\n";
    return 1;
}
