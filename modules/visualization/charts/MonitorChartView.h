#pragma once

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QPainterPath>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

#include "MonitorCurveFit.h"

#include <QtCore/qnamespace.h>

// The charting library is Qwt 7.5.2 (LGPL 2.1 plus the three Qwt exceptions,
// which permit static linking and closed-source use). Every Qwt type stays
// forward declared here so nothing outside this widget includes a Qwt header.
class QTimer;
class QwtPlot;
class QwtPlotCurve;
class QwtPlotPicker;

namespace aerosim {
namespace monitor {

// Marker drawn for every sample. Samples from a solver are discrete
// measurements, so a marker is the primary mark on the canvas and the joining
// curve is an optional overlay.
//
// Every drawable marker the charting library offers is named here, so a UI is
// never limited to the two shapes that happened to be wired up first. The
// numeric values are part of the API: Circle and Square keep 0 and 1 because
// they are the defaults every existing caller relies on.
//
// Four of these carry drawing content instead of a geometric outline and read
// it from MonitorPointContent: Path, Pixmap, Graphic and Svg.
enum class MonitorPointShape {
    None = -1,   // no marker at all, for a plain line with no sample marks
    Circle = 0,
    Square = 1,
    Diamond = 2,
    Triangle = 3,   // equilateral, apex up
    DownTriangle = 4,
    UpTriangle = 5,
    LeftTriangle = 6,
    RightTriangle = 7,
    Cross = 8,      // plus sign, stroke only
    XCross = 9,     // diagonal cross, stroke only
    HLine = 10,     // single horizontal stroke
    VLine = 11,     // single vertical stroke
    Star1 = 12,     // stroke star
    Star2 = 13,     // filled star
    Hexagon = 14,
    Path = 20,      // caller-supplied QPainterPath
    Pixmap = 21,    // caller-supplied QPixmap
    Graphic = 22,   // caller-supplied image, kept scalable
    Svg = 23        // caller-supplied SVG document bytes
};

// The drawing content for the four marker shapes that cannot be generated from
// a size alone. Ignored for every other shape.
//
// Anything Qt already models stays a Qt type, so this header keeps its promise
// of not exposing the charting library. In particular Graphic takes a QImage
// where the charting library itself would demand its own vector type.
struct MonitorPointContent {
    QPainterPath path;    // MonitorPointShape::Path, in a unit square
    QImage image;         // MonitorPointShape::Pixmap and ::Graphic
    QByteArray svg;       // MonitorPointShape::Svg, a complete SVG document

    bool isEmpty() const
    {
        return path.isEmpty() && image.isNull() && svg.isEmpty();
    }
};

// How the samples are joined into a curve. None keeps the raw samples unjoined,
// which is the honest default for discrete solver output: a straight line
// between two iterations asserts continuity the data may not have.
enum class MonitorInterpolation {
    None = 0,    // points only
    Linear = 1,  // straight segments between neighbouring samples
    Spline = 2   // smooth interpolation through the samples
};

// A fitted trend drawn on top of a series, as opposed to the joining stroke.
//
// MonitorInterpolation connects neighbouring samples and therefore passes
// through every one of them; a fit approximates the whole series and normally
// touches none. Both are optional and independent: a chart can show raw marks
// with a fitted trend and no joining stroke at all, which is what a solver
// history usually wants.
struct MonitorFitStyle {
    MonitorFitMethod method = MonitorFitMethod::None;
    // Polynomial degree for MonitorFitMethod::Polynomial.
    int degree = 3;
    // Width for MonitorFitMethod::MovingAverage, in samples.
    int window = 7;
    // The fit is drawn dashed by default: it is a summary of the data, not a
    // measurement, and must not be mistaken for one more sample series.
    Qt::PenStyle lineStyle = Qt::DashLine;
    qreal lineWidth = 2.5;
};

// One series handed to the chart view. Deliberately free of charting types: the
// caller builds these from MonitorSeriesModel, so the plotting library stays
// behind MonitorChartView.cpp.
struct MonitorCurve {
    QString key;                  // monitor column key, e.g. "CD"
    QString label;                // text shown in the legend, overridable per caller
    QString datasetId;            // which file/run this column came from
    QVector<QPointF> points;
    QColor color = QColor(Qt::blue);
    Qt::PenStyle lineStyle = Qt::SolidLine;
    qreal lineWidth = 2.0;
    bool logarithmic = false;     // residual group defaults to a log Y axis
    bool visible = true;
    // Point appearance. One mark per sample unless pointShape is None.
    qreal pointSize = 5.0;
    MonitorPointShape pointShape = MonitorPointShape::Circle;
    // Backing content for Path, Pixmap, Graphic and Svg markers.
    MonitorPointContent pointContent;
    // Optional joining curve.
    MonitorInterpolation interpolation = MonitorInterpolation::None;
    // Optional fitted trend, drawn on top of the samples. Independent of the
    // joining stroke.
    MonitorFitStyle fit;
    // Thinning step: every Nth sample is drawn, plus the last one. 1 draws every
    // sample. Declared per curve, not per plot, so one magnitude group can be
    // thinned while another stays complete -- and so a UI can scope the setting
    // to a whole group or to the few curves it has selected. The batch setters
    // on MonitorChartView are the entry points for those scopes.
    int sampleInterval = 1;
    // Curves that share a group share one Y axis. Residuals and aerodynamic
    // coefficients get separate groups so the two magnitudes never flatten each
    // other (docs/PostProcessMonitorSeriesPlan.md 5.4).
    QString groupKey;
    QString groupTitle;           // Y axis caption for the group, e.g. "残差（对数轴）"
};

// The appearance of one curve as a UI may edit it at run time. Everything here is
// a property of the drawn series, not of the data, so changing it never needs the
// samples again.
struct MonitorCurveStyle {
    QString label;                // legend caption; empty keeps the resolver/column name
    QColor color = QColor(Qt::blue);
    Qt::PenStyle lineStyle = Qt::SolidLine;
    qreal lineWidth = 2.0;
    qreal pointSize = 5.0;
    MonitorPointShape pointShape = MonitorPointShape::Circle;
    // Backing content for Path, Pixmap, Graphic and Svg markers.
    MonitorPointContent pointContent;
    MonitorInterpolation interpolation = MonitorInterpolation::None;
    MonitorFitStyle fit;
    // Thinning step, as in MonitorCurve::sampleInterval.
    int sampleInterval = 1;
};
// One Y axis as it was actually built, so callers can show the grouping instead
// of leaving the reader to guess which scale a curve ended up on.
struct MonitorAxisSummary {
    QString groupKey;
    QString title;
    bool logarithmic = false;
    bool onRight = false;
    int curveCount = 0;
};

// Thin wrapper around the charting library (Qwt 7.5.2). Keeping every Qwt type
// behind this file is what makes the library choice reversible: callers only see
// MonitorCurve, MonitorCurveStyle and the accessors below.
class MonitorChartView final : public QWidget {
    Q_OBJECT
public:
    explicit MonitorChartView(QWidget *parent = nullptr);

    // X axis caption, e.g. "迭代步 (Outer/Inner)" or "时间索引 (Time_Iter)".
    void setAxisLabel(const QString &label);
    // Caption drawn above the plot. A title should name what the drawing shows
    // -- "残差", "系数" -- rather than restate how many series happened to be
    // drawn; the counts live in statusText() for whoever wants them. An empty
    // string restores the built-in summary title. The caption survives
    // setCurves(), which installs a fresh plot underneath it.
    void setChartTitle(const QString &title);
    QString chartTitle() const { return chartTitle_; }
    void setCurves(const QVector<MonitorCurve> &curves);
    void clearCurves();
    void enableViewNavigation();
    void resetView();

    // ---- Live appends ------------------------------------------------------
    // Adds samples to one existing curve without rebuilding the drawing.
    // setCurves() installs a fresh plot, which would throw away the zoom, the
    // selection and every style the user set; a solver that reports a row per
    // iteration cannot pay that cost once per row. This keeps the plot, the axes
    // and the other curves as they are and only pushes the new samples into the
    // curve that grew.
    //
    // The X axis and the curve's own Y axis are rescaled to cover what has been
    // drawn so far. During zoom or pan, the new range takes effect on reset.
    // Returns false when the curve is not on the drawing, in which case the
    // caller is expected to fall back to setCurves().
    bool appendPoints(const QString &curveId, const QVector<QPointF> &points);

    // Stable identity of a drawn curve. Two datasets routinely carry the same
    // physical quantity (both a baseline and a variant report "CD"), so the
    // column key alone does not identify a curve -- the dataset has to be part of
    // the identity or the second one would overwrite the first.
    static QString curveId(const MonitorCurve &curve);
    static QString makeCurveId(const QString &datasetId, const QString &key);
    // Identities of the curves known to the last setCurves(), in drawing order.
    QStringList curveIds() const;
    // Samples the widget holds for one curve, in the order they were given.
    // Read-back for callers that build the series themselves and need to prove
    // what was actually retained. Empty for an unknown id.
    QVector<QPointF> curvePoints(const QString &curveId) const;

    // Legend text per curve. A record may carry several physical quantities and
    // one quantity may arrive in several records, so the caption is a decision
    // the caller owns, not something this widget can guess. Returning an empty
    // string falls back to the curve's own label.
    using LabelResolver = std::function<QString(const MonitorCurve &)>;
    void setLabelResolver(LabelResolver resolver);
    // The legend captions the last setCurves() produced, in drawing order.
    QStringList legendLabels() const;

    // Reserved hook for a joining method the library does not ship (a fitted
    // polynomial, a monotone spline, a regression, ...). The factory returns a
    // curve item owned by the plot; the widget fills it with the samples when it
    // is empty, applies the pen and attaches the axes exactly like the built-in
    // ones. It may install its own QwtCurveFitter to reshape the joining line.
    using CurveFactory = std::function<QwtPlotCurve *(const MonitorCurve &, QwtPlot *plot)>;
    void setCurveFactory(CurveFactory factory);

    // ---- Run-time appearance edits -----------------------------------------
    // These mutate the series already on the chart and deliberately do NOT
    // rebuild it: a UI that recolours one curve must not reset the selection,
    // the zoom or the other curves' styles. chartBuildCount() proves it.
    bool setCurveStyle(const QString &curveId, const MonitorCurveStyle &style);
    // Convenience for the common case of retitling one series.
    bool setCurveLabel(const QString &curveId, const QString &label);
    // Current appearance of a drawn curve; an invalid style for an unknown id.
    MonitorCurveStyle curveStyle(const QString &curveId) const;

    // ---- Selection ---------------------------------------------------------
    // Selecting a curve thickens its stroke and grows its markers, so the
    // highlight is visible without a separate list widget.
    void setSelectedCurve(const QString &curveId);
    QString selectedCurve() const;
    // Sample-level selection. index is a position in the curve's own points, not
    // a pixel: callers get the exact sample the user hit.
    void setSelectedPoint(const QString &curveId, int sampleIndex);
    QString selectedPointCurve() const;
    int selectedPointSample() const;
    // Index of the sample closest to a chart coordinate, or -1.
    int sampleIndexAt(const QString &curveId, const QPointF &value) const;
    // Selecting by value, exactly as a click on the drawing does. An outside list
    // or table can drive the same selection, and the click handler routes through
    // here so both paths behave identically.
    void selectAt(const QString &curveId, const QPointF &value);

    // Upper bound on drawn samples per curve; 0 draws every sample. This is a
    // global safety cap, applied after the per-curve sampling interval below.
    // Decimation keeps every Nth sample plus the last one and is reported through
    // statusText(), so a thinned drawing never pretends to be complete.
    void setPointBudget(int budget);
    int pointBudget() const;

    // ---- Scoped edits ------------------------------------------------------
    // A user thinks in groups: "the residuals", "these three coefficients".
    // Applying a setting to the whole drawing, or to one curve at a time, is the
    // wrong unit. These three scopes cover what a chart panel needs, and the
    // widget owns the iteration so that no caller has to re-implement it:
    //   All     every curve in the drawing
    //   Group   every curve whose groupKey matches
    //   Curves  exactly the listed curve ids (one or many)
    enum class EditScope { All, Group, Curves };

    // A partial style. Only the fields flagged as present are written, so a
    // group-wide "make every marker bigger" does not stamp one colour onto a
    // group whose members are distinguished by colour.
    struct StylePatch {
        bool hasColor = false;
        QColor color;
        bool hasLineStyle = false;
        Qt::PenStyle lineStyle = Qt::SolidLine;
        bool hasPointSize = false;
        qreal pointSize = 5.0;
        bool hasPointShape = false;
        MonitorPointShape pointShape = MonitorPointShape::Circle;
        bool hasPointContent = false;
        MonitorPointContent pointContent;
        bool hasInterpolation = false;
        MonitorInterpolation interpolation = MonitorInterpolation::None;
        bool hasFit = false;
        MonitorFitStyle fit;
        // Sampling interval: 1 draws every sample, N draws every Nth. This is the
        // setting a user means by "downsample"; a point budget is a property of
        // the data volume, an interval is a property of the view.
        bool hasSampleInterval = false;
        int sampleInterval = 1;
        bool hasVisible = false;
        bool visible = true;
        bool hasLabel = false;
        QString label;
    };

    // Writes every present field of the patch onto every curve the scope selects
    // and returns how many curves changed. curveIds is consulted only by
    // EditScope::Curves, groupKey only by EditScope::Group.
    int applyStyle(EditScope scope, const StylePatch &patch, const QString &groupKey = QString(),
                   const QStringList &curveIds = QStringList());
    // Convenience wrappers over applyStyle(), because these two are what a tree
    // panel does on almost every interaction.
    int setSampleIntervalForGroup(const QString &groupKey, int interval);
    int setSampleIntervalForCurves(const QStringList &curveIds, int interval);

    // The grouping as the widget sees it, so a panel can build its scope lists
    // without keeping a second copy of the data.
    QStringList groupKeys() const;
    QStringList curveIdsInGroup(const QString &groupKey) const;

    int visibleCurveCount() const;
    int drawnPointCount() const;
    // Samples actually laid down for one curve after the sampling interval and
    // the global budget. drawnPointCount() counts what the caller supplied; this
    // counts what the drawing holds, which is what a decimation check needs.
    int drawnSampleCount(const QString &curveId) const;
    // Marks laid down for the samples (one series per curve).
    int pointSeriesCount() const;
    // Joining curves actually drawn (one per curve whose interpolation is not
    // None).
    int lineSeriesCount() const;
    // Fitted trends actually drawn (one per curve whose fit method is not None).
    int fitSeriesCount() const;
    // The fit that was produced for one curve, so a caller can show rSquared or
    // the reason a fit was reduced. An OK-but-empty result means no fit is drawn.
    MonitorFitResult fitResult(const QString &curveId) const;
    // Number of Y axes built by the last setCurves() call: 1 for a single group,
    // 2 when residuals and coefficients are overlaid (plan 5.4).
    int axisCount() const;
    QVector<MonitorAxisSummary> axisSummaries() const;
    // Human-readable drawing summary, so a degenerate axis never ends in a
    // silent empty chart.
    QString statusText() const;

    // Number of graphics items currently in the chart scene. This is the
    // observable form of the redraw-leak bug: it used to grow with every
    // setCurves() call instead of staying near the fresh-drawing value.
    int sceneItemCount() const;

    // How many times setCurves() installed a fresh plot. Style edits must leave
    // this untouched -- that is what "changed without redrawing" means.
    int chartBuildCount() const;
    // How many times this widget has actually asked the plot to redraw. A live
    // burst is expected to fold thousands of appends into a handful of these,
    // and counting them is the only way to tell coalescing apart from a drawing
    // that simply stopped updating.
    int replotCount() const;

    // Saved PNG of the current chart, used by the render test and by anyone who
    // needs a picture without a screen.
    bool savePng(const QString &path, const QSize &size = QSize(1200, 720));

signals:
    // A curve was clicked anywhere on its marks or joining line.
    void curveClicked(const QString &curveId);
    // A specific sample was clicked; sampleIndex indexes the curve's own points.
    void pointClicked(const QString &curveId, int sampleIndex, const QPointF &value);
    // setCurveStyle()/setCurveLabel() actually changed something.
    void curveStyleChanged(const QString &curveId);
    // The highlighted curve changed; an empty id means "nothing selected".
    void curveSelectionChanged(const QString &curveId);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void transformView(const QPointF &anchor, double factor, const QPointF &translation);
    void setAutomaticAxisScale(int axis, double lower, double upper);
    void setAutomaticYAxisRange(int axis, bool logarithmic, double lower, double upper);

    // Lets the plot finish laying out series and axes before pixels are taken.
    // Without it a render can return a stale title over fresh curves.
    void settle();
    // Legend caption for one curve, honouring the caller's resolver.
    QString labelFor(const MonitorCurve &curve) const;
    // Samples actually drawn for one curve, after the point budget.
    QVector<QPointF> drawnSamples(const MonitorCurve &curve, int *stride, bool *truncated) const;
    // The samples of one curve, from the single vector that owns them. Never
    // null for a curve that was drawn; null is the "not drawn" answer.
    const QVector<QPointF> *storedPoints(const QString &curveId) const;
    // Metadata of one curve with its samples attached. Returns a default curve
    // when the id names nothing that was drawn, so callers that only read style
    // fields do not need a separate existence test.
    MonitorCurve curveWithPoints(const QString &curveId) const;
    // Pushes the stored style of one curve onto its live series.
    void applyLiveStyle(const QString &curveId);
    // Replaces only the joining layer of one curve, for the one style change
    // (interpolation) that needs a different curve fitting. Data, axes and every
    // other curve stay as they are.
    void rebuildLineSeriesFor(const QString &curveId);
    // Replaces only the fitted layer of one curve, for the style change that
    // needs different fitted samples. Data, axes and every other curve stay.
    void rebuildFitSeriesFor(const QString &curveId);
    // Click routing from the plot picker to this widget's own selection model.
    // The picker reports canvas pixels, so the nearest sample is resolved in
    // pixel space where distances are what the user actually sees.
    void handlePickedPixel(const QPoint &pos);
    // Rebuilds legendLabels_ from the stored curves, in drawing order.
    void refreshLegendLabels();
    // Refits both axes of one curve's group to the samples now stored, used by
    // appendPoints() so a live drawing keeps filling its frame.
    void rescaleForCurve(const QString &curveId);
    void rescaleYAxis(int axisId);
    // Asks for a repaint at the end of the current frame instead of now. A live
    // feed can deliver thousands of rows a second, and QwtPlot::replot() is
    // synchronous: it refits every axis to every series and flushes pending
    // layout requests before it even queues the paint. Paying that once per
    // point is what makes a burst quadratic. Coalescing keeps the same pixels.
    void scheduleReplot();
    // Runs the repaint scheduleReplot() deferred, and clears the pending flag so
    // the next append can schedule again. Also called directly by savePng(): a
    // render taken while a repaint is still pending would capture the old frame.
    void flushReplot();
    // Indices into curves_ that an edit scope selects.
    QVector<int> resolveScope(EditScope scope, const QString &groupKey,
                              const QStringList &curveIds) const;

    QwtPlot *plot_ = nullptr;
    QwtPlotPicker *picker_ = nullptr;
    bool viewNavigationEnabled_ = false;
    bool panning_ = false;
    QPointF lastPanPosition_;
    double zoomSteps_ = 0.0;
    QHash<int, QPair<double, double>> fullAxisRanges_;
    // Created on first use so a widget that never sees live data never owns a
    // timer. Single shot at one frame interval: the flag below is what actually
    // coalesces, the timer only decides when the coalesced frame is drawn.
    QTimer *replotTimer_ = nullptr;
    // A repaint is owed. Set by the first append of a frame, cleared by the
    // timer's single shot, so later appends of the same frame are free.
    bool replotPending_ = false;
    QVector<MonitorCurve> curves_;
    // Index into curves_ by curve identity. curves_ is the one store that owns
    // sample data; every other map below keeps metadata only. Two copies of a
    // growing series detached on every append, which is where a live feed's
    // memory went: one QPointF per row per curve became two.
    QHash<QString, int> curveIndexById_;
    QVector<MonitorAxisSummary> axes_;
    QString axisLabel_;
    // Sampling interval per curve identity. Kept apart from curves_ so a redraw
    // with the same data does not silently reset what the user chose: setCurves()
    // re-applies these onto every incoming curve whose id matches.
    QHash<QString, int> sampleIntervalsById_;
    // Set while this widget rebuilds itself for a scoped edit: such a rebuild must
    // keep the captions the user just set, unlike a rebuild caused by new data.
    bool keepExplicitLabels_ = false;
    // Caller-supplied plot caption; empty means the built-in summary stands.
    QString chartTitle_;
    // The caption the last setCurves() would use on its own. Kept so clearing a
    // caller title can restore it without rebuilding the drawing.
    QString builtInTitle_;
    QString status_;
    // status_ as the last setCurves() left it. Appends rebuild the reported row
    // count on top of this instead of re-deriving the whole paragraph.
    QString statusBase_;
    // Rows added through appendPoints() since that rebuild.
    qint64 liveAppendedRows_ = 0;
    QStringList legendLabels_;
    LabelResolver labelResolver_;
    CurveFactory curveFactory_;
    int pointBudget_ = 0;
    int pointSeriesCount_ = 0;
    int lineSeriesCount_ = 0;
    int fitSeriesCount_ = 0;
    int drawnPointCount_ = 0;
    int chartBuildCount_ = 0;
    int replotCount_ = 0;

    // Live items by curve identity, so a style edit can find the marks and the
    // joining line without walking the plot. Qwt draws a symbol and a connecting
    // line on the same curve item, so one entry covers both roles.
    QHash<QString, MonitorCurve> curvesById_;
    // Marks and joining line stay two separate items, exactly as the point and
    // line series did: a symbol-only item never draws a connecting line, and the
    // joining item can carry its own fitter. Only one of the two ever owns the
    // legend entry, so a joined curve still shows a single caption.
    struct CurveItems {
        QwtPlotCurve *marks = nullptr;
        QwtPlotCurve *joining = nullptr;
        // Third layer: the fitted trend. Kept apart from the joining stroke
        // because the two answer different questions and are styled differently.
        QwtPlotCurve *fitted = nullptr;
        int axisId = 0;
    };
    QHash<QString, CurveItems> curveItemsById_;
    // What the last fit produced for each curve, exposed through fitResult().
    QHash<QString, MonitorFitResult> fitResultsById_;
    // Captions set explicitly through setCurveLabel(). They outrank the resolver:
    // a caller that retitles one curve at run time means it.
    QHash<QString, QString> explicitLabels_;

    QString selectedCurveId_;
    QString selectedPointCurveId_;
    int selectedPointSample_ = -1;
};

} // namespace monitor
} // namespace aerosim
