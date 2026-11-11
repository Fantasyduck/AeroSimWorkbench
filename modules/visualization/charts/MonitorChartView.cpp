#include "MonitorChartView.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

#include <qwt_graphic.h>
#include <qwt_legend.h>
#include <qwt_picker_machine.h>
#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_dict.h>
#include <qwt_plot_picker.h>
#include <qwt_plot_renderer.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_spline_curve_fitter.h>
#include <qwt_symbol.h>
#include <qwt_text.h>

namespace aerosim {
namespace monitor {
namespace {

// Same encoding-safe literal helper the rest of the project uses, so this file
// compiles identically whether or not the compiler assumes UTF-8 source.
template <size_t N> QString uiText(const char16_t (&text)[N])
{ return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1); }

// QwtLogScaleEngine refuses zero and negative values. A SU2 history file stores
// the residuals themselves and those are routinely negative, so a requested log
// axis must degrade to linear and say so instead of dropping the series.
bool allPositive(const QVector<QPointF> &points)
{
    if (points.isEmpty())
        return false;
    for (const QPointF &point : points) {
        if (point.y() <= 0.0)
            return false;
    }
    return true;
}

void extendFitRange(const MonitorFitResult &fit, bool logarithmic, double &low, double &high)
{
    for (const QPointF &point : fit.points) {
        const double value = point.y();
        if (std::isfinite(value) && (!logarithmic || value > 0.0)) {
            low = qMin(low, value);
            high = qMax(high, value);
        }
    }
}

// Everything the chart needs to know about one Y axis before the axis exists.
struct GroupBucket {
    QString key;
    QString title;
    QColor color = QColor(Qt::darkGray);
    bool logarithmic = false;
    QVector<int> curveIndexes;
    QVector<QPointF> points;
    double minY = 0.0;
    double maxY = 0.0;
};

QString interpolationName(MonitorInterpolation interpolation)
{
    switch (interpolation) {
    case MonitorInterpolation::Linear:
        return uiText(u"直线连接");
    case MonitorInterpolation::Spline:
        return uiText(u"样条拟合");
    case MonitorInterpolation::None:
        break;
    }
    return uiText(u"仅散点");
}

// Add emphasis to the selected stroke while keeping its colour and relative width.
constexpr qreal kSelectedLineExtraWidth = 1.5;
// Marker growth factor while a curve is selected, same reasoning as the width.
constexpr qreal kSelectedMarkerScale = 1.6;
// How close a click has to land to a sample before it counts as hitting it.
constexpr double kPickRadiusPx = 12.0;

// Qwt places exactly one scale on each side of a plot (QwtAxis::YLeft and
// QwtAxis::YRight), so a drawing can carry two magnitudes and no more.
constexpr int kMaxMagnitudeGroups = 2;

// The public shape enum names the markers on its own terms; this is the one place
// that translates them into the charting library's vocabulary. Everything that
// the library can generate from a size alone maps one to one.
QwtSymbol::Style toSymbolStyle(MonitorPointShape shape)
{
    switch (shape) {
    case MonitorPointShape::None:          return QwtSymbol::NoSymbol;
    case MonitorPointShape::Circle:        return QwtSymbol::Ellipse;
    case MonitorPointShape::Square:        return QwtSymbol::Rect;
    case MonitorPointShape::Diamond:       return QwtSymbol::Diamond;
    case MonitorPointShape::Triangle:      return QwtSymbol::Triangle;
    case MonitorPointShape::DownTriangle:  return QwtSymbol::DTriangle;
    case MonitorPointShape::UpTriangle:    return QwtSymbol::UTriangle;
    case MonitorPointShape::LeftTriangle:  return QwtSymbol::LTriangle;
    case MonitorPointShape::RightTriangle: return QwtSymbol::RTriangle;
    case MonitorPointShape::Cross:         return QwtSymbol::Cross;
    case MonitorPointShape::XCross:        return QwtSymbol::XCross;
    case MonitorPointShape::HLine:         return QwtSymbol::HLine;
    case MonitorPointShape::VLine:         return QwtSymbol::VLine;
    case MonitorPointShape::Star1:         return QwtSymbol::Star1;
    case MonitorPointShape::Star2:         return QwtSymbol::Star2;
    case MonitorPointShape::Hexagon:       return QwtSymbol::Hexagon;
    case MonitorPointShape::Path:          return QwtSymbol::Path;
    case MonitorPointShape::Pixmap:        return QwtSymbol::Pixmap;
    case MonitorPointShape::Graphic:       return QwtSymbol::Graphic;
    case MonitorPointShape::Svg:           return QwtSymbol::SvgDocument;
    }
    return QwtSymbol::Ellipse;
}

// QwtPlotCurve takes ownership of whatever setSymbol() is handed and deletes the
// previous symbol, so every call has to pass a freshly allocated one rather than
// mutating the current symbol in place.
void applySymbol(QwtPlotCurve *item, MonitorPointShape shape, const MonitorPointContent &content,
                 const QColor &color, qreal size)
{
    // No marker at all: hand the curve a null symbol, which every drawing and
    // legend path in the library already guards against.
    if (shape == MonitorPointShape::None) {
        item->setSymbol(nullptr);
        return;
    }

    const int px = int(size > 0.0 ? size : 1.0);

    // A content-bearing marker cannot be built from a style alone: the library
    // needs the drawing handed to it before it can render anything.
    if (shape == MonitorPointShape::Path && !content.path.isEmpty()) {
        auto *symbol = new QwtSymbol(content.path, QBrush(color), QPen(color, 1.0));
        symbol->setSize(px);
        item->setSymbol(symbol);
        return;
    }
    if (shape == MonitorPointShape::Svg && !content.svg.isEmpty()) {
        auto *symbol = new QwtSymbol(QwtSymbol::SvgDocument);
        // The setter forces the style to SvgDocument on its own, but stating the
        // style up front keeps the intent readable.
        symbol->setSvgDocument(content.svg);
        symbol->setSize(px);
        item->setSymbol(symbol);
        return;
    }
    if ((shape == MonitorPointShape::Pixmap || shape == MonitorPointShape::Graphic)
        && !content.image.isNull()) {
        if (shape == MonitorPointShape::Pixmap) {
            auto *symbol = new QwtSymbol(QwtSymbol::Pixmap);
            symbol->setPixmap(QPixmap::fromImage(content.image));
            symbol->setSize(px);
            item->setSymbol(symbol);
            return;
        }
        // The library's own vector graphic type would have to appear in this
        // signature otherwise, so the image travels as a QImage and is replayed
        // into a graphic here.
        QwtGraphic graphic;
        QPainter graphicPainter(&graphic);
        graphicPainter.drawImage(QRectF(QPointF(0.0, 0.0), QSizeF(content.image.size())),
                                 content.image);
        graphicPainter.end();
        auto *symbol = new QwtSymbol(QwtSymbol::Graphic);
        symbol->setGraphic(graphic);
        symbol->setSize(px);
        item->setSymbol(symbol);
        return;
    }

    // A content shape with nothing to draw falls back to the plain marker for its
    // style instead of vanishing without a trace.
    auto *symbol = new QwtSymbol(toSymbolStyle(shape));
    symbol->setSize(px);
    symbol->setPen(QPen(color, 1.0));
    symbol->setBrush(QBrush(color));
    item->setSymbol(symbol);
}

// The joining style. None removes the connecting stroke entirely and leaves the
// instrument with only its sample marks.
void applyJoiningStyle(QwtPlotCurve *item, MonitorInterpolation interpolation)
{
    switch (interpolation) {
    case MonitorInterpolation::None:
        item->setStyle(QwtPlotCurve::NoCurve);
        break;
    case MonitorInterpolation::Spline:
        item->setStyle(QwtPlotCurve::Lines);
        item->setCurveFitter(new QwtSplineCurveFitter);
        break;
    case MonitorInterpolation::Linear:
        item->setStyle(QwtPlotCurve::Lines);
        item->setCurveFitter(nullptr);
        break;
    }
}

} // namespace

void MonitorChartView::settle()
{
    // Qwt defers geometry work to the event loop, so processEvents() is needed
    // before the plot area reflects a new series list. The synchronous repaint
    // afterwards guarantees the canvas is cleared first: an asynchronous update
    // can leave the previous frame's axes visible.
    QApplication::processEvents();
    if (plot_) {
        plot_->update();
        QApplication::processEvents();
        plot_->repaint();
    }
}

void MonitorChartView::scheduleReplot()
{
    if (!plot_)
        return;
    if (replotPending_)
        return; // This frame is already covered; folding in more is free.
    replotPending_ = true;

    if (!replotTimer_) {
        replotTimer_ = new QTimer(this);
        replotTimer_->setSingleShot(true);
        // One frame at 60 Hz. Waiting longer than a frame would be visible as
        // stutter; waiting less would coalesce nothing, because the appends of
        // one burst all arrive before the event loop gets a turn anyway.
        replotTimer_->setInterval(16);
        connect(replotTimer_, &QTimer::timeout, this, &MonitorChartView::flushReplot);
    }
    replotTimer_->start();
}

void MonitorChartView::flushReplot()
{
    if (replotTimer_)
        replotTimer_->stop();
    replotPending_ = false;
    if (!plot_)
        return;
    ++replotCount_;
    plot_->replot();
}

int MonitorChartView::replotCount() const
{
    return replotCount_;
}

namespace {

// A plot configured the way every drawing needs it.
QwtPlot *createMonitorPlot()
{
    auto *plot = new QwtPlot;
    plot->setAutoReplot(false);
    plot->setCanvasBackground(QColor(QStringLiteral("#ffffff")));
    // Axis captions sit between the plot area and the plot edge, so the default
    // margins clip a rotated Y caption at the right edge. Reserve room for it
    // once here rather than per drawing.
    plot->setContentsMargins(8, 8, 72, 8);
    auto *legend = new QwtLegend(plot);
    legend->setMaxColumns(0);
    plot->insertLegend(legend, QwtPlot::BottomLegend);
    return plot;
}

} // namespace

MonitorChartView::MonitorChartView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    plot_ = createMonitorPlot();
    plot_->setObjectName(QStringLiteral("monitorChartView"));
    plot_->setTitle(uiText(u"尚未导入监控序列"));
    layout->addWidget(plot_);

    // A click has to become "which curve, which sample". Qwt reports the raw
    // canvas pixel, so the nearest sample is resolved below where the samples and
    // their axis maps are both known.
    picker_ = new QwtPlotPicker(QwtAxis::XBottom, QwtAxis::YLeft, QwtPicker::NoRubberBand,
                                QwtPicker::AlwaysOff, plot_->canvas());
    picker_->setStateMachine(new QwtPickerClickPointMachine);
    connect(picker_, static_cast<void (QwtPicker::*)(const QPoint &)>(&QwtPicker::appended),
            this, [this](const QPoint &pos) { handlePickedPixel(pos); });

    status_ = uiText(u"尚未导入监控序列。");
}

void MonitorChartView::enableViewNavigation()
{
    viewNavigationEnabled_ = true;
    plot_->canvas()->installEventFilter(this);
}

void MonitorChartView::resetView()
{
    if (fullAxisRanges_.isEmpty())
        return;
    for (auto range = fullAxisRanges_.constBegin(); range != fullAxisRanges_.constEnd(); ++range)
        plot_->setAxisScale(range.key(), range.value().first, range.value().second);
    fullAxisRanges_.clear();
    zoomSteps_ = 0.0;
    flushReplot();
}

bool MonitorChartView::eventFilter(QObject *watched, QEvent *event)
{
    const auto type = event->type();
    if (type != QEvent::Wheel && type != QEvent::MouseButtonPress
        && type != QEvent::MouseButtonRelease && type != QEvent::MouseMove
        && type != QEvent::UngrabMouse) {
        return QWidget::eventFilter(watched, event);
    }

    auto *canvas = plot_->canvas();
    if (watched != canvas)
        return QWidget::eventFilter(watched, event);

    if (type == QEvent::UngrabMouse) {
        if (panning_) {
            panning_ = false;
            canvas->unsetCursor();
        }
        return QWidget::eventFilter(watched, event);
    }
    if (!plot_->isEnabled())
        return QWidget::eventFilter(watched, event);

    if (type == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        const int delta = wheel->angleDelta().y() != 0
            ? wheel->angleDelta().y() : wheel->pixelDelta().y();
        if (delta == 0)
            return QWidget::eventFilter(watched, event);

        const double next = qBound(-12.0, zoomSteps_ + delta / 120.0, 28.0);
        if (next != zoomSteps_) {
            const double factor = std::pow(1.2, zoomSteps_ - next);
            zoomSteps_ = next;
            transformView(wheel->posF(), factor, {});
        }
    } else {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (type == QEvent::MouseButtonPress && mouse->button() == Qt::MiddleButton) {
            panning_ = true;
            lastPanPosition_ = mouse->screenPos();
            canvas->setCursor(Qt::ClosedHandCursor);
        } else if (panning_ && type == QEvent::MouseMove && mouse->buttons().testFlag(Qt::MiddleButton)) {
            const QPointF movement = mouse->screenPos() - lastPanPosition_;
            lastPanPosition_ = mouse->screenPos();
            if (!movement.isNull())
                transformView({}, 1.0, movement);
        } else if (panning_ && type == QEvent::MouseButtonRelease && mouse->button() == Qt::MiddleButton) {
            panning_ = false;
            canvas->unsetCursor();
        } else {
            return QWidget::eventFilter(watched, event);
        }
    }
    event->accept();
    return true;
}

void MonitorChartView::transformView(const QPointF &anchor, double factor, const QPointF &translation)
{
    for (int axis : {QwtAxis::XBottom, QwtAxis::YLeft, QwtAxis::YRight}) {
        if (!plot_->axisEnabled(axis))
            continue;
        const auto map = plot_->canvasMap(axis);
        const bool horizontal = axis == QwtAxis::XBottom;
        const double origin = horizontal ? anchor.x() : anchor.y();
        const double offset = horizontal ? translation.x() : translation.y();
        if (factor == 1.0 && offset == 0.0)
            continue;
        // Transform canvas coordinates so logarithmic axes pan and zoom in screen space too.
        const double lower = map.invTransform(origin + (map.p1() - origin) * factor - offset);
        const double upper = map.invTransform(origin + (map.p2() - origin) * factor - offset);
        if (!std::isfinite(lower) || !std::isfinite(upper) || lower >= upper)
            continue;
        if (!fullAxisRanges_.contains(axis))
            fullAxisRanges_.insert(axis, qMakePair(map.s1(), map.s2()));
        plot_->setAxisScale(axis, lower, upper);
    }
    flushReplot();
}

QString MonitorChartView::makeCurveId(const QString &datasetId, const QString &key)
{
    // Dataset first, key second: a baseline file and a variant file both report
    // "CD", and without the dataset in front the second curve would be treated as
    // the same curve as the first. '|' cannot appear in either part, so the pair
    // stays unambiguous.
    if (datasetId.isEmpty())
        return key;
    return datasetId + QLatin1Char('|') + key;
}

QString MonitorChartView::curveId(const MonitorCurve &curve)
{
    return makeCurveId(curve.datasetId, curve.key);
}

QStringList MonitorChartView::curveIds() const
{
    QStringList ids;
    ids.reserve(curvesById_.size());
    // Drawing order, not hash order: callers index into legendLabels() with this.
    for (const MonitorCurve &curve : curves_) {
        if (curve.visible && !curve.points.isEmpty())
            ids.append(curveId(curve));
    }
    return ids;
}

QVector<QPointF> MonitorChartView::curvePoints(const QString &curveId) const
{
    const QVector<QPointF> *points = storedPoints(curveId);
    return points ? *points : QVector<QPointF>();
}

// The sample store lives in curves_ alone. Keeping a second copy in
// curvesById_ cost one extra QPointF per row per curve the moment an append
// detached the shared vector, and a live feed appends forever.
const QVector<QPointF> *MonitorChartView::storedPoints(const QString &curveId) const
{
    const auto it = curveIndexById_.constFind(curveId);
    if (it == curveIndexById_.constEnd())
        return nullptr;
    if (it.value() < 0 || it.value() >= curves_.size())
        return nullptr;
    return &curves_.at(it.value()).points;
}

MonitorCurve MonitorChartView::curveWithPoints(const QString &curveId) const
{
    const auto it = curveIndexById_.constFind(curveId);
    if (it == curveIndexById_.constEnd() || it.value() < 0 || it.value() >= curves_.size())
        return curvesById_.value(curveId);
    // curves_ is the only store with samples, but it is not where a style edit
    // lands while the drawing is live: setCurveStyle() writes curvesById_ first.
    // Reading the metadata from there and the samples from here keeps both true.
    MonitorCurve curve = curvesById_.value(curveId, curves_.at(it.value()));
    curve.points = curves_.at(it.value()).points;
    return curve;
}

void MonitorChartView::setAxisLabel(const QString &label)
{
    axisLabel_ = label;
}

void MonitorChartView::setChartTitle(const QString &title)
{
    if (chartTitle_ == title)
        return;
    chartTitle_ = title;
    // Only the caption changes, so the drawing is not rebuilt -- a caller
    // retitling a chart must not lose its selection or its zoom.
    if (plot_)
        plot_->setTitle(chartTitle_.isEmpty() ? builtInTitle_ : chartTitle_);
}

void MonitorChartView::setLabelResolver(LabelResolver resolver)
{
    labelResolver_ = std::move(resolver);
}

void MonitorChartView::setCurveFactory(CurveFactory factory)
{
    curveFactory_ = std::move(factory);
}

void MonitorChartView::setPointBudget(int budget)
{
    pointBudget_ = budget > 0 ? budget : 0;
}

int MonitorChartView::pointBudget() const
{
    return pointBudget_;
}

QString MonitorChartView::labelFor(const MonitorCurve &curve) const
{
    // A caption set at run time by setCurveLabel() outranks everything: the caller
    // just renamed this one series and expects its own text back.
    const auto explicitIt = explicitLabels_.constFind(curveId(curve));
    if (explicitIt != explicitLabels_.constEnd() && !explicitIt.value().isEmpty())
        return explicitIt.value();

    // Otherwise the caller owns the caption through the resolver: one record can
    // carry several physical quantities, and one quantity can arrive in several
    // records, so only the caller knows what distinguishes them. The resolver
    // returning nothing means "no opinion".
    if (labelResolver_) {
        const QString resolved = labelResolver_(curve);
        if (!resolved.isEmpty())
            return resolved;
    }
    if (!curve.label.isEmpty())
        return curve.label;
    return curve.key;
}

QVector<QPointF> MonitorChartView::drawnSamples(const MonitorCurve &curve,
                                               int *stride, bool *truncated) const
{
    if (stride)
        *stride = 1;
    if (truncated)
        *truncated = false;

    const int total = curve.points.size();

    // Two independent reasons to thin a series, combined into a single stride:
    // the per-curve sampling interval the user set, and the global point budget
    // that only exists to stop a pathological file from freezing the drawing.
    // The interval is the one a UI exposes; the budget stays a safety cap.
    int interval = curve.sampleInterval > 0 ? curve.sampleInterval : 1;
    if (pointBudget_ > 0 && total > pointBudget_) {
        const int budgetStep = (total + pointBudget_ - 1) / pointBudget_;
        if (budgetStep > interval)
            interval = budgetStep;
    }
    if (interval <= 1 || total <= 1)
        return curve.points;

    // Keep an even stride plus the final sample: the last iteration is usually
    // the converged value a reader looks for, and a plain stride would drop it.
    const int step = interval;
    if (stride)
        *stride = step;
    if (truncated)
        *truncated = true;

    QVector<QPointF> reduced;
    reduced.reserve(total / step + 1);
    for (int i = 0; i < total; i += step)
        reduced.append(curve.points.at(i));
    if (reduced.isEmpty() || reduced.last() != curve.points.last())
        reduced.append(curve.points.last());
    return reduced;
}

QStringList MonitorChartView::groupKeys() const
{
    QStringList keys;
    for (const MonitorCurve &curve : curves_) {
        if (!curve.visible || curve.points.isEmpty())
            continue;
        const QString key = curve.groupKey.isEmpty() ? curve.key : curve.groupKey;
        if (!keys.contains(key))
            keys.append(key);
    }
    return keys;
}

QStringList MonitorChartView::curveIdsInGroup(const QString &groupKey) const
{
    QStringList ids;
    for (const MonitorCurve &curve : curves_) {
        if (!curve.visible || curve.points.isEmpty())
            continue;
        const QString key = curve.groupKey.isEmpty() ? curve.key : curve.groupKey;
        if (key == groupKey)
            ids.append(curveId(curve));
    }
    return ids;
}

QVector<int> MonitorChartView::resolveScope(EditScope scope, const QString &groupKey,
                                            const QStringList &curveIds) const
{
    QVector<int> indices;
    for (int i = 0; i < curves_.size(); ++i) {
        const MonitorCurve &curve = curves_.at(i);
        bool selected = false;
        switch (scope) {
        case EditScope::All:
            selected = true;
            break;
        case EditScope::Group: {
            const QString key = curve.groupKey.isEmpty() ? curve.key : curve.groupKey;
            selected = key == groupKey;
            break;
        }
        case EditScope::Curves:
            selected = curveIds.contains(curveId(curve));
            break;
        }
        if (selected)
            indices.append(i);
    }
    return indices;
}

int MonitorChartView::applyStyle(EditScope scope, const StylePatch &patch, const QString &groupKey,
                                 const QStringList &curveIds)
{
    const QVector<int> indices = resolveScope(scope, groupKey, curveIds);
    if (indices.isEmpty())
        return 0;

    const int interval = patch.hasSampleInterval ? qMax(1, patch.sampleInterval) : 1;
    bool needRebuild = false;
    bool changed = false;
    QStringList touched;

    for (int index : indices) {
        MonitorCurve &curve = curves_[index];
        const QString id = curveId(curve);
        MonitorCurveStyle style = curveStyle(id);
        // A label is only written when the patch carries one. Handing back the
        // resolved caption would turn every touched curve into an explicitly
        // named one, freezing a caption the caller meant to stay derived.
        style.label = patch.hasLabel ? patch.label : QString();

        bool curveChanged = false;
        if (patch.hasColor && style.color != patch.color) {
            style.color = patch.color;
            curveChanged = true;
        }
        if (patch.hasLineStyle && style.lineStyle != patch.lineStyle) {
            style.lineStyle = patch.lineStyle;
            curveChanged = true;
        }
        if (patch.hasPointSize && !qFuzzyCompare(style.pointSize + 1.0, patch.pointSize + 1.0)) {
            style.pointSize = patch.pointSize;
            curveChanged = true;
        }
        if (patch.hasPointShape && style.pointShape != patch.pointShape) {
            style.pointShape = patch.pointShape;
            curveChanged = true;
        }
        if (patch.hasPointContent) {
            style.pointContent = patch.pointContent;
            curveChanged = true;
        }
        if (patch.hasInterpolation && style.interpolation != patch.interpolation) {
            style.interpolation = patch.interpolation;
            curveChanged = true;
        }
        if (patch.hasFit) {
            const MonitorFitStyle &incoming = patch.fit;
            if (style.fit.method != incoming.method || style.fit.degree != incoming.degree
                || style.fit.window != incoming.window || style.fit.lineStyle != incoming.lineStyle
                || !qFuzzyCompare(style.fit.lineWidth, incoming.lineWidth)) {
                style.fit = incoming;
                curveChanged = true;
            }
        }
        if (patch.hasLabel && !patch.label.isEmpty()) {
            style.label = patch.label;
            curveChanged = true;
        }
        if (patch.hasVisible && curve.visible != patch.visible) {
            curve.visible = patch.visible;
            curveChanged = true;
            // Visibility decides which marks exist at all, so this is the one
            // flag that cannot be pushed onto the live items.
            needRebuild = true;
        }

        bool intervalChanged = false;
        if (patch.hasSampleInterval) {
            const int previous = curve.sampleInterval > 0 ? curve.sampleInterval : 1;
            if (previous != interval) {
                // Stride is baked into the samples an item holds, so the marks
                // have to be laid down again; there is no live edit for it.
                curve.sampleInterval = interval;
                sampleIntervalsById_.insert(id, interval);
                intervalChanged = true;
                needRebuild = true;
                curveChanged = true;
            }
        }

        if (curveChanged || intervalChanged) {
            touched.append(id);
            changed = true;
        }
        // setCurveStyle() routes the rest: it mirrors the values into curves_ and
        // rebuilds only the layers whose samples depend on the change.
        if (curveChanged)
            setCurveStyle(id, style);
    }

    if (needRebuild) {
        // Marks, legend and the axis assignment are all derived from the curve
        // set, so one rebuild covers visibility and sampling changes together.
        // Explicit captions are kept: these are the same curves, not new data.
        keepExplicitLabels_ = true;
        setCurves(curves_);
        keepExplicitLabels_ = false;
    }

    if (changed)
        settle();
    return touched.size();
}

int MonitorChartView::setSampleIntervalForGroup(const QString &groupKey, int interval)
{
    StylePatch patch;
    patch.hasSampleInterval = true;
    patch.sampleInterval = interval;
    return applyStyle(EditScope::Group, patch, groupKey);
}

int MonitorChartView::setSampleIntervalForCurves(const QStringList &curveIds, int interval)
{
    StylePatch patch;
    patch.hasSampleInterval = true;
    patch.sampleInterval = interval;
    return applyStyle(EditScope::Curves, patch, QString(), curveIds);
}

void MonitorChartView::clearCurves()
{
    curves_.clear();
    setCurves(curves_);
}

int MonitorChartView::visibleCurveCount() const
{
    int count = 0;
    for (const MonitorCurve &curve : curves_) {
        if (curve.visible && !curve.points.isEmpty())
            ++count;
    }
    return count;
}

int MonitorChartView::drawnPointCount() const
{
    // Counts every sample the caller supplied, not only the ones painted: this
    // is the number callers assert on, and decimation is reported separately.
    int count = 0;
    for (const MonitorCurve &curve : curves_) {
        if (curve.visible && !curve.points.isEmpty())
            count += curve.points.size();
    }
    return count;
}

int MonitorChartView::drawnSampleCount(const QString &curveId) const
{
    if (!storedPoints(curveId))
        return 0;
    int stride = 1;
    bool truncated = false;
    return drawnSamples(curveWithPoints(curveId), &stride, &truncated).size();
}

int MonitorChartView::pointSeriesCount() const
{
    return pointSeriesCount_;
}

int MonitorChartView::lineSeriesCount() const
{
    return lineSeriesCount_;
}

int MonitorChartView::axisCount() const
{
    return axes_.size();
}

QVector<MonitorAxisSummary> MonitorChartView::axisSummaries() const
{
    return axes_;
}

QStringList MonitorChartView::legendLabels() const
{
    return legendLabels_;
}

QString MonitorChartView::statusText() const
{
    return status_;
}

int MonitorChartView::sceneItemCount() const
{
    if (!plot_)
        return 0;
    // Every drawable thing Qwt owns is a plot item; itemList() is the honest
    // observable form of the redraw-leak bug, which used to grow with each call.
    return plot_->itemList().size();
}

int MonitorChartView::chartBuildCount() const
{
    return chartBuildCount_;
}

bool MonitorChartView::savePng(const QString &path, const QSize &size)
{
    if (!plot_)
        return false;
    const QFileInfo info(path);
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath()))
        return false;

    // A live append may have left a repaint pending. Rendering now would capture
    // the frame before it, so the owed frame is drawn first.
    flushReplot();

    const QSize target = size.isValid() ? size : QSize(1200, 720);
    const QSize previousSize = plot_->size();

    // Render the plot into an image of the requested size. QwtPlotRenderer takes
    // its target rectangle from the paint device, so the plot itself has to be
    // laid out at that size first or the drawing is framed for the wrong extent.
    plot_->resize(target);
    settle();

    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    {
        // renderTo() hands the plot the whole image as its target rectangle, which
        // leaves the title's ascenders and a right-hand axis title flush with the
        // border and therefore half cut off. Inset the rectangle so the decoration
        // has room to land inside the image.
        constexpr int kRenderMargin = 10;
        const QRectF frame(kRenderMargin, kRenderMargin,
                           qMax(1, target.width() - 2 * kRenderMargin),
                           qMax(1, target.height() - 2 * kRenderMargin));
        QPainter painter(&image);
        QwtPlotRenderer renderer;
        renderer.setDiscardFlag(QwtPlotRenderer::DiscardBackground, true);
        renderer.render(plot_, &painter, frame);
    }

    plot_->resize(previousSize);
    settle();

    return image.save(path, "PNG");
}

void MonitorChartView::setCurves(const QVector<MonitorCurve> &curves)
{
    curves_ = curves;
    // The sampling interval belongs to the view, not to one snapshot of the data:
    // a caller that rebuilds its curve list after a settings change must not have
    // to remember and re-send what the user already chose. Applied here, before
    // any item is created, so the marks are laid down at the right stride.
    for (MonitorCurve &curve : curves_) {
        const auto it = sampleIntervalsById_.constFind(curveId(curve));
        if (it != sampleIntervalsById_.constEnd() && it.value() > 0)
            curve.sampleInterval = it.value();
    }
    axes_.clear();
    legendLabels_.clear();
    curvesById_.clear();
    curveIndexById_.clear();
    curveItemsById_.clear();
    fitResultsById_.clear();
    pointSeriesCount_ = 0;
    lineSeriesCount_ = 0;
    fitSeriesCount_ = 0;
    drawnPointCount_ = 0;
    liveAppendedRows_ = 0;
    // Captions survive a redraw only while their curve does; a new data set is a
    // new set of subjects to name. A rebuild this widget triggers for its own
    // scoped edits is the exception -- there the subjects are the same curves.
    if (!keepExplicitLabels_)
        explicitLabels_.clear();

    // Start from a clean plot. Rebuilding rather than detaching keeps the axis
    // scale widgets from outliving their axes, which is what once left a second
    // drawing showing the previous axis labels underneath the new ones.
    QwtPlot *plot = createMonitorPlot();
    ++chartBuildCount_;

    double minX = 0.0;
    double maxX = 0.0;
    bool haveRange = false;
    int drawnSeries = 0;
    int decimatedCurves = 0;
    int sampleStride = 1;
    QStringList nonPositiveLog;
    QStringList interpolationNotes;
    QStringList fitTrendNotes;
    QStringList fitNotes;

    // Qwt owns exactly one scale per side, so a drawing can carry two magnitudes.
    // The surviving groups are chosen before any curve is created: a group beyond
    // the second is dropped outright rather than merged onto the right axis,
    // because sharing a scale between two different magnitudes is precisely the
    // flattening this grouping exists to prevent.
    QStringList keptGroupKeys;
    for (const MonitorCurve &curve : curves_) {
        if (!curve.visible || curve.points.isEmpty())
            continue;
        const QString key = curve.groupKey.isEmpty() ? curve.key : curve.groupKey;
        if (!keptGroupKeys.contains(key) && keptGroupKeys.size() < kMaxMagnitudeGroups)
            keptGroupKeys.append(key);
    }

    // Group the visible curves by their declared dimension so each magnitude
    // gets its own Y axis instead of being flattened onto a shared scale.
    QVector<GroupBucket> groups;
    QStringList drawnIds;
    QStringList droppedGroups;

    // Index-based so the drawing can record where each drawn curve's samples live
    // without a second lookup by identity.
    for (int curveIndex = 0; curveIndex < curves_.size(); ++curveIndex) {
        const MonitorCurve &curve = curves_.at(curveIndex);
        if (!curve.visible || curve.points.isEmpty())
            continue;

        const QString key = curve.groupKey.isEmpty() ? curve.key : curve.groupKey;
        // A dropped group is skipped entirely: no curve item is created, so it can
        // never be counted, picked or reported as drawn.
        if (!keptGroupKeys.contains(key)) {
            const QString title = curve.groupTitle.isEmpty() ? key : curve.groupTitle;
            if (!droppedGroups.contains(title))
                droppedGroups.append(title);
            continue;
        }

        int bucketIndex = -1;
        for (int i = 0; i < groups.size(); ++i) {
            if (groups.at(i).key == key) { bucketIndex = i; break; }
        }
        if (bucketIndex < 0) {
            GroupBucket bucket;
            bucket.key = key;
            bucket.title = curve.groupTitle;
            bucket.logarithmic = curve.logarithmic;
            bucket.color = curve.color;
            groups.append(bucket);
            bucketIndex = groups.size() - 1;
        }
        GroupBucket &bucket = groups[bucketIndex];
        bucket.logarithmic = bucket.logarithmic || curve.logarithmic;
        if (bucket.title.isEmpty())
            bucket.title = curve.groupTitle;

        int stride = 1;
        bool truncated = false;
        const QVector<QPointF> samples = drawnSamples(curve, &stride, &truncated);
        if (truncated) {
            ++decimatedCurves;
            sampleStride = qMax(sampleStride, stride);
        }
        drawnPointCount_ += samples.size();

        const QString id = curveId(curve);
        const QString label = labelFor(curve);
        QPen pen(curve.color, curve.lineWidth);
        pen.setStyle(curve.lineStyle);

        // A record is a series of discrete samples, so the marks are their own
        // item and the joining stroke is an optional second one on top of them.
        auto *marks = new QwtPlotCurve(label);
        marks->setSamples(samples);
        marks->setStyle(QwtPlotCurve::NoCurve);
        marks->setRenderHint(QwtPlotItem::RenderAntialiased, true);
        applySymbol(marks, curve.pointShape, curve.pointContent, curve.color, curve.pointSize);

        QwtPlotCurve *joining = nullptr;
        if (curve.interpolation != MonitorInterpolation::None) {
            if (curveFactory_) {
                // Caller-supplied joining method: filled in below like the
                // built-in ones so it inherits pens, axes and legend handling.
                joining = curveFactory_(curve, plot);
            } else {
                joining = new QwtPlotCurve(label);
            }
            if (joining) {
                applyJoiningStyle(joining, curve.interpolation);
                joining->setPen(pen);
                joining->setRenderHint(QwtPlotItem::RenderAntialiased, true);
                // Only fill a factory result that came back empty, so a factory
                // that already shaped its own samples is left alone.
                if (joining->dataSize() == 0)
                    joining->setSamples(samples);
            }
        }

        // Third layer: the fitted trend. It is a summary of the series, not one
        // more series, so it never takes the legend entry -- two entries under one
        // quantity would suggest two measurements.
        QwtPlotCurve *fitted = nullptr;
        MonitorFitResult fitResult;
        if (curve.fit.method != MonitorFitMethod::None) {
            // Fit the drawn samples, not the raw ones: the fitted curve has to
            // describe what is actually on the canvas.
            fitResult = MonitorCurveFit::fit(samples, curve.fit.method,
                                             curve.fit.degree, curve.fit.window);
            if (fitResult.isValid()) {
                fitted = new QwtPlotCurve(label);
                fitted->setStyle(QwtPlotCurve::Lines);
                fitted->setSamples(fitResult.points);
                QPen fitPen(curve.color, curve.fit.lineWidth);
                fitPen.setStyle(curve.fit.lineStyle);
                fitted->setPen(fitPen);
                fitted->setRenderHint(QwtPlotItem::RenderAntialiased, true);
                fitted->setItemAttribute(QwtPlotItem::Legend, false);
            } else if (!fitResult.note.isEmpty()) {
                fitNotes.append(uiText(u"%1：%2").arg(label, fitResult.note));
            }
        }

        // Only one legend entry per curve: the joining stroke carries the caption,
        // so the duplicate mark entry is switched off when a curve is joined.
        marks->setItemAttribute(QwtPlotItem::Legend, joining == nullptr);
        if (joining)
            joining->setItemAttribute(QwtPlotItem::Legend, true);

        marks->attach(plot);
        if (joining)
            joining->attach(plot);
        if (fitted)
            fitted->attach(plot);
        ++pointSeriesCount_;
        if (joining)
            ++lineSeriesCount_;
        if (fitted)
            ++fitSeriesCount_;
        if (joining)
            interpolationNotes.append(uiText(u"%1（%2）")
                                          .arg(label, interpolationName(curve.interpolation)));
        if (fitted) {
            fitTrendNotes.append(uiText(u"%1（%2，R²=%3）")
                                     .arg(label, MonitorCurveFit::methodName(curve.fit.method))
                                     .arg(fitResult.rSquared, 0, 'f', 3));
        }

        for (const QPointF &point : samples) {
            if (!haveRange) {
                minX = maxX = point.x();
                haveRange = true;
            } else {
                minX = qMin(minX, point.x());
                maxX = qMax(maxX, point.x());
            }
        }
        for (const QPointF &point : curve.points) {
            if (bucket.points.isEmpty()) {
                bucket.minY = bucket.maxY = point.y();
            } else {
                bucket.minY = qMin(bucket.minY, point.y());
                bucket.maxY = qMax(bucket.maxY, point.y());
            }
            bucket.points.append(point);
        }

        bucket.curveIndexes.append(drawnIds.size());
        legendLabels_.append(label);
        drawnIds.append(id);
        // Metadata only: the samples stay in curves_ and are reached through
        // curveIndexById_. A copy here would share the vector until the next
        // append detached it, and that detach was half of the per-row cost.
        MonitorCurve meta = curve;
        meta.points.clear();
        meta.points.squeeze();
        curvesById_.insert(id, meta);
        curveIndexById_.insert(id, curveIndex);
        CurveItems items;
        items.marks = marks;
        items.joining = joining;
        items.fitted = fitted;
        curveItemsById_.insert(id, items);
        // Recorded whether or not a trend was drawn: a refused fit is the one
        // case where the caller most needs the answer, and the note explaining
        // it is the only thing that distinguishes "the fit was rejected" from
        // "the fit is simply off".
        if (curve.fit.method != MonitorFitMethod::None)
            fitResultsById_.insert(id, fitResult);
        ++drawnSeries;
    }

    // The counts are a self-description of the drawing, not a caption: they stay
    // available through statusText() and only stand in as the title when the
    // caller has not named the chart. A title is expected to name the quantity
    // being drawn.
    builtInTitle_ = drawnSeries == 0
        ? uiText(u"未选择监控序列")
        : uiText(u"%1 条序列 · %2 条纵轴").arg(drawnSeries).arg(qMin(groups.size(), kMaxMagnitudeGroups));
    plot->setTitle(chartTitle_.isEmpty() ? builtInTitle_ : chartTitle_);

    // Install the new plot and drop the old one. The picker lives on the canvas,
    // so it is rebuilt with the plot it belongs to.
    QVBoxLayout *hostLayout = qobject_cast<QVBoxLayout *>(layout());
    QwtPlot *previousPlot = plot_;
    if (hostLayout) {
        hostLayout->addWidget(plot);
        if (previousPlot)
            hostLayout->removeWidget(previousPlot);
    } else {
        plot->setParent(this);
        plot->show();
    }
    plot_ = plot;
    panning_ = false;
    zoomSteps_ = 0.0;
    fullAxisRanges_.clear();
    if (viewNavigationEnabled_)
        plot_->canvas()->installEventFilter(this);
    picker_ = new QwtPlotPicker(QwtAxis::XBottom, QwtAxis::YLeft, QwtPicker::NoRubberBand,
                                QwtPicker::AlwaysOff, plot_->canvas());
    picker_->setStateMachine(new QwtPickerClickPointMachine);
    connect(picker_, static_cast<void (QwtPicker::*)(const QPoint &)>(&QwtPicker::appended),
            this, [this](const QPoint &pos) { handlePickedPixel(pos); });
    if (previousPlot)
        delete previousPlot;

    // A selection only survives if its curve still exists in the new drawing.
    if (!selectedCurveId_.isEmpty() && !curvesById_.contains(selectedCurveId_))
        selectedCurveId_.clear();
    if (!selectedPointCurveId_.isEmpty() && !curvesById_.contains(selectedPointCurveId_)) {
        selectedPointCurveId_.clear();
        selectedPointSample_ = -1;
    }

    if (drawnSeries == 0) {
        status_ = curves_.isEmpty() ? uiText(u"尚未导入监控序列。")
                                    : uiText(u"当前没有勾选任何监控序列。");
        plot_->setEnabled(false);
        flushReplot();
        return;
    }
    plot_->setEnabled(true);

    if (maxX == minX)
        maxX = minX + 1.0;

    setAutomaticAxisScale(QwtAxis::XBottom, minX, maxX);
    plot_->setAxisTitle(QwtAxis::XBottom, axisLabel_.isEmpty() ? uiText(u"迭代步") : axisLabel_);

    // Each surviving group gets one side of the plot: the first on the left, the
    // second on the right. There is no third case -- a group that would need one
    // was dropped before any curve was created, so a shared scale between two
    // different magnitudes cannot happen here.
    QVector<int> groupAxis;   // QwtAxis::Position index chosen for each group
    for (int i = 0; i < groups.size(); ++i) {
        GroupBucket &bucket = groups[i];
        const int axisPos = (i == 0) ? QwtAxis::YLeft : QwtAxis::YRight;
        groupAxis.append(axisPos);

        bool wantLog = bucket.logarithmic;
        if (wantLog && !allPositive(bucket.points)) {
            wantLog = false;
            for (int index : bucket.curveIndexes) {
                const QString id = drawnIds.value(index);
                if (!id.isEmpty())
                    nonPositiveLog.append(labelFor(curvesById_.value(id)));
            }
        }

        // The caption states the scale the axis really got, so a group whose log
        // request had to be degraded never advertises itself as logarithmic.
        const QString baseTitle = bucket.title.isEmpty() ? uiText(u"数值") : bucket.title;
        const QString scaleSuffix = wantLog ? uiText(u"（对数轴）") : uiText(u"（线性轴）");

        if (wantLog)
            plot_->setAxisScaleEngine(axisPos, new QwtLogScaleEngine(10));
        else
            plot_->setAxisScaleEngine(axisPos, new QwtLinearScaleEngine);

        double lower = bucket.minY;
        double upper = bucket.maxY;
        for (int index : bucket.curveIndexes) {
            const auto fit = fitResultsById_.constFind(drawnIds.value(index));
            if (fit != fitResultsById_.constEnd())
                extendFitRange(fit.value(), wantLog, lower, upper);
        }
        setAutomaticYAxisRange(axisPos, wantLog, lower, upper);

        plot_->enableAxis(axisPos, true);
        QwtText title(baseTitle + scaleSuffix);
        // The caption is tinted with the group's own colour, which is how a
        // two-axis drawing says which curve belongs to which scale.
        title.setColor(bucket.color);
        plot_->setAxisTitle(axisPos, title);

        MonitorAxisSummary summary;
        summary.groupKey = bucket.key;
        summary.title = baseTitle + scaleSuffix;
        summary.logarithmic = wantLog;
        summary.onRight = (axisPos == QwtAxis::YRight);
        summary.curveCount = bucket.curveIndexes.size();
        axes_.append(summary);
    }

    for (int i = 0; i < groups.size(); ++i) {
        const int axisPos = groupAxis.value(i, QwtAxis::YLeft);
        for (int index : groups.at(i).curveIndexes) {
            const QString id = drawnIds.value(index);
            CurveItems items = curveItemsById_.value(id);
            if (items.marks) {
                items.marks->setAxes(QwtAxis::XBottom, axisPos);
                items.axisId = axisPos;
            }
            if (items.joining)
                items.joining->setAxes(QwtAxis::XBottom, axisPos);
            if (items.fitted)
                items.fitted->setAxes(QwtAxis::XBottom, axisPos);
            curveItemsById_.insert(id, items);
        }
    }

    flushReplot();

    status_ = uiText(u"已绘制 %1 条序列 / %2 个数据点，%3 条纵轴。")
                  .arg(drawnSeries)
                  .arg(drawnPointCount_)
                  .arg(axes_.size());
    // State the grouping explicitly: without this the two scales are invisible
    // and the user has to guess why one curve sits lower than the other.
    QStringList grouping;
    for (const MonitorAxisSummary &summary : axes_) {
        grouping.append(uiText(u"%1 → %2%3（%4 条）")
                            .arg(summary.title)
                            .arg(summary.onRight ? uiText(u"右轴") : uiText(u"左轴"))
                            .arg(summary.logarithmic ? uiText(u" 对数") : uiText(u" 线性"))
                            .arg(summary.curveCount));
    }
    status_ += uiText(u" 分组：") + grouping.join(uiText(u"；")) + uiText(u"。");
    // A group beyond the second is not drawn at all. Say which ones and why, so a
    // missing curve is never mistaken for a rendering fault.
    if (!droppedGroups.isEmpty()) {
        status_ += uiText(u" 超出两条纵轴上限、未绘制的分组：%1。")
                       .arg(droppedGroups.join(uiText(u"、")));
    }
    // Say which curves are joined and how, so "points only" is never mistaken
    // for a rendering failure.
    status_ += interpolationNotes.isEmpty()
        ? uiText(u" 全部序列仅绘制离散采样点，未连接。")
        : uiText(u" 连接方式：") + interpolationNotes.join(uiText(u"、")) + uiText(u"。");
    // A fit changes the meaning of the drawing, so which curves were fitted and
    // how well is never left for the reader to guess.
    if (!fitTrendNotes.isEmpty())
        status_ += uiText(u" 拟合：") + fitTrendNotes.join(uiText(u"、")) + uiText(u"。");
    if (!fitNotes.isEmpty())
        status_ += uiText(u" 未能拟合：") + fitNotes.join(uiText(u"、")) + uiText(u"。");
    if (decimatedCurves > 0) {
        status_ += uiText(u" 已按每 %1 个采样点取 1 点抽稀 %2 条序列，末点保留。")
                       .arg(sampleStride)
                       .arg(decimatedCurves);
    }
    if (!nonPositiveLog.isEmpty())
        status_ += uiText(u" 含非正值无法取对数、该组已按线性绘制：%1。").arg(nonPositiveLog.join(uiText(u"、")));

    // Remembered so appendPoints() can report the added rows without rebuilding
    // this paragraph: every later append only replaces the tail.
    statusBase_ = status_;

    // Re-apply the highlight so a selection made before a redraw keeps showing.
    if (!selectedCurveId_.isEmpty())
        applyLiveStyle(selectedCurveId_);
}

bool MonitorChartView::appendPoints(const QString &id, const QVector<QPointF> &points)
{
    if (!plot_ || points.isEmpty())
        return false;

    const auto itemsIt = curveItemsById_.constFind(id);
    if (itemsIt == curveItemsById_.constEnd() || !itemsIt.value().marks)
        return false;

    // The stored curve is the source of truth for everything a rebuild would
    // re-derive (labels, styles, axes), so the new samples have to land there as
    // well as on the items -- otherwise the next setCurves() would silently drop
    // them and the counters would report a stale row count.
    int storedIndex = -1;
    for (int i = 0; i < curves_.size(); ++i) {
        if (curveId(curves_.at(i)) == id) {
            storedIndex = i;
            break;
        }
    }
    if (storedIndex < 0)
        return false;

    // One store only. The second copy this used to keep shared the vector until
    // this very line detached it, doubling the per-row footprint of every curve
    // that a live feed keeps growing.
    curves_[storedIndex].points += points;

    const MonitorCurve &curve = curves_.at(storedIndex);
    int stride = 1;
    bool truncated = false;
    const QVector<QPointF> samples = drawnSamples(curve, &stride, &truncated);

    CurveItems items = itemsIt.value();
    // setSamples() on a live item replaces the stored data in place: no plot is
    // built, so the axes, the other curves and the selection all survive.
    items.marks->setSamples(samples);
    if (items.joining)
        items.joining->setSamples(samples);
    if (items.fitted) {
        const MonitorFitResult fitResult = MonitorCurveFit::fit(samples, curve.fit.method,
                                                                curve.fit.degree, curve.fit.window);
        if (fitResult.isValid()) {
            items.fitted->setSamples(fitResult.points);
            fitResultsById_.insert(id, fitResult);
        }
    }
    curveItemsById_.insert(id, items);

    liveAppendedRows_ += points.size();
    drawnPointCount_ += points.size();

    rescaleForCurve(id);
    // Not replot() directly: one burst can deliver thousands of rows, and
    // replot() refits every axis and flushes layout synchronously. The pending
    // flag folds the whole burst into the frame that is already owed.
    scheduleReplot();

    // Only the reported counts change; the rest of the paragraph (grouping,
    // degradation notes, decimation) still describes this drawing exactly.
    status_ = statusBase_ + uiText(u" 已接收实时追加 %1 行，未重建图表。").arg(liveAppendedRows_);
    return true;
}

void MonitorChartView::rescaleForCurve(const QString &id)
{
    if (!plot_)
        return;
    const auto itemsIt = curveItemsById_.constFind(id);
    if (itemsIt == curveItemsById_.constEnd())
        return;
    const int axisId = itemsIt.value().axisId;

    const QVector<QPointF> *stored = storedPoints(id);
    if (!stored || stored->isEmpty())
        return;
    const QVector<QPointF> &points = *stored;

    double minX = points.first().x();
    double maxX = minX;
    for (const QPointF &point : points) {
        minX = qMin(minX, point.x());
        maxX = qMax(maxX, point.x());
    }
    if (maxX == minX)
        maxX = minX + 1.0;
    setAutomaticAxisScale(QwtAxis::XBottom, minX, maxX);

    rescaleYAxis(axisId);
}

void MonitorChartView::setAutomaticAxisScale(int axis, double lower, double upper)
{
    auto range = fullAxisRanges_.find(axis);
    if (range != fullAxisRanges_.end())
        range.value() = qMakePair(lower, upper);
    else
        plot_->setAxisScale(axis, lower, upper);
}

void MonitorChartView::setAutomaticYAxisRange(int axis, bool logarithmic, double low, double high)
{
    if (logarithmic) {
        low = low > 0.0 ? low : 1e-16;
        high = high > low ? high : low * 10.0;
    } else if (high == low) {
        const double pad = qAbs(low) > 0.0 ? qAbs(low) * 0.05 : 0.5;
        low -= pad;
        high += pad;
    }
    setAutomaticAxisScale(axis, low, high);
}

void MonitorChartView::rescaleYAxis(int axisId)
{
    bool logarithmic = false;
    for (const MonitorAxisSummary &summary : axes_) {
        if ((summary.onRight ? QwtAxis::YRight : QwtAxis::YLeft) == axisId) {
            logarithmic = summary.logarithmic;
            break;
        }
    }

    double minY = 0.0;
    double maxY = 0.0;
    bool haveRange = false;
    for (const MonitorCurve &curve : curves_) {
        const QString id = curveId(curve);
        const auto items = curveItemsById_.constFind(id);
        if (items == curveItemsById_.constEnd() || items.value().axisId != axisId)
            continue;
        for (const QPointF &point : curve.points) {
            const double value = point.y();
            if (!std::isfinite(value) || (logarithmic && value <= 0.0))
                continue;
            minY = haveRange ? qMin(minY, value) : value;
            maxY = haveRange ? qMax(maxY, value) : value;
            haveRange = true;
        }
    }
    if (!haveRange)
        return;
    for (auto it = curveItemsById_.constBegin(); it != curveItemsById_.constEnd(); ++it) {
        if (it.value().axisId != axisId || !it.value().fitted)
            continue;
        const auto fit = fitResultsById_.constFind(it.key());
        if (fit != fitResultsById_.constEnd())
            extendFitRange(fit.value(), logarithmic, minY, maxY);
    }
    setAutomaticYAxisRange(axisId, logarithmic, minY, maxY);
}

void MonitorChartView::applyLiveStyle(const QString &id)
{
    const CurveItems items = curveItemsById_.value(id);
    if (!items.marks)
        return;
    const MonitorCurve curve = curvesById_.value(id);
    const bool selected = (id == selectedCurveId_);
    const QString label = labelFor(curve);

    items.marks->setTitle(label);
    applySymbol(items.marks, curve.pointShape, curve.pointContent,
                curve.color, curve.pointSize * (selected ? kSelectedMarkerScale : 1.0));

    if (items.joining) {
        QPen pen(curve.color, curve.lineWidth + (selected ? kSelectedLineExtraWidth : 0.0));
        pen.setStyle(curve.lineStyle);
        items.joining->setPen(pen);
        items.joining->setTitle(label);
    }
    if (items.fitted) {
        QPen pen(curve.color, curve.fit.lineWidth + (selected ? kSelectedLineExtraWidth : 0.0));
        pen.setStyle(curve.fit.lineStyle);
        items.fitted->setPen(pen);
        items.fitted->setTitle(label);
    }
    if (plot_)
        flushReplot();
}

int MonitorChartView::fitSeriesCount() const
{
    return fitSeriesCount_;
}

MonitorFitResult MonitorChartView::fitResult(const QString &curveId) const
{
    return fitResultsById_.value(curveId);
}

void MonitorChartView::refreshLegendLabels()
{
    legendLabels_.clear();
    for (const MonitorCurve &curve : curves_) {
        if (curve.visible && !curve.points.isEmpty())
            legendLabels_.append(labelFor(curve));
    }
}

bool MonitorChartView::setCurveStyle(const QString &curveId, const MonitorCurveStyle &style)
{
    const auto it = curvesById_.constFind(curveId);
    if (it == curvesById_.constEnd())
        return false;

    MonitorCurve &stored = curvesById_[curveId];
    const MonitorInterpolation previousInterpolation = stored.interpolation;
    const bool fitChanged = stored.fit.method != style.fit.method
        || stored.fit.degree != style.fit.degree
        || stored.fit.window != style.fit.window;

    stored.color = style.color;
    stored.lineStyle = style.lineStyle;
    stored.lineWidth = style.lineWidth;
    stored.pointSize = style.pointSize;
    stored.pointShape = style.pointShape;
    stored.pointContent = style.pointContent;
    stored.interpolation = style.interpolation;
    stored.fit = style.fit;
    stored.sampleInterval = style.sampleInterval > 0 ? style.sampleInterval : 1;
    if (!style.label.isEmpty()) {
        explicitLabels_.insert(curveId, style.label);
        stored.label = style.label;
    }

    // Mirror the edit into the source list as well, so a later setCurves() with
    // the same data does not silently revert what the user just chose.
    for (MonitorCurve &curve : curves_) {
        if (MonitorChartView::curveId(curve) != curveId)
            continue;
        curve.color = stored.color;
        curve.lineStyle = stored.lineStyle;
        curve.lineWidth = stored.lineWidth;
        curve.pointSize = stored.pointSize;
        curve.pointShape = stored.pointShape;
        curve.pointContent = stored.pointContent;
        curve.interpolation = stored.interpolation;
        curve.fit = stored.fit;
        curve.sampleInterval = stored.sampleInterval;
        if (!style.label.isEmpty())
            curve.label = style.label;
        break;
    }

    // Colour, stroke, marker and caption are applied to the live items. Only a
    // change of joining method needs a different curve item -- Qwt keeps the
    // fitter on the item -- so that one case rebuilds just this curve's joining
    // stroke and leaves the data, axes and every other curve untouched.
    if (previousInterpolation != stored.interpolation)
        rebuildLineSeriesFor(curveId);
    // A different fit means different fitted samples, and Qwt keeps the samples
    // on the item, so this layer is rebuilt for that case too. A change of the
    // fit's pen style alone is handled by applyLiveStyle() below.
    if (fitChanged)
        rebuildFitSeriesFor(curveId);

    applyLiveStyle(curveId);
    refreshLegendLabels();
    settle();
    emit curveStyleChanged(curveId);
    return true;
}

bool MonitorChartView::setCurveLabel(const QString &curveId, const QString &label)
{
    const auto it = curvesById_.constFind(curveId);
    if (it == curvesById_.constEnd())
        return false;

    MonitorCurveStyle style;
    style.label = label;
    style.color = it.value().color;
    style.lineStyle = it.value().lineStyle;
    style.lineWidth = it.value().lineWidth;
    style.pointSize = it.value().pointSize;
    style.pointShape = it.value().pointShape;
    style.pointContent = it.value().pointContent;
    style.interpolation = it.value().interpolation;
    style.fit = it.value().fit;
    style.sampleInterval = it.value().sampleInterval;
    return setCurveStyle(curveId, style);
}

MonitorCurveStyle MonitorChartView::curveStyle(const QString &curveId) const
{
    MonitorCurveStyle style;
    const auto it = curvesById_.constFind(curveId);
    if (it == curvesById_.constEnd())
        return style;
    const MonitorCurve &curve = it.value();
    style.label = labelFor(curve);
    style.color = curve.color;
    style.lineStyle = curve.lineStyle;
    style.lineWidth = curve.lineWidth;
    style.pointSize = curve.pointSize;
    style.pointShape = curve.pointShape;
    style.pointContent = curve.pointContent;
    style.interpolation = curve.interpolation;
    style.fit = curve.fit;
    style.sampleInterval = curve.sampleInterval;
    return style;
}

void MonitorChartView::rebuildLineSeriesFor(const QString &id)
{
    if (!plot_)
        return;
    CurveItems items = curveItemsById_.value(id);
    if (!items.marks)
        return;
    const MonitorCurve curve = curveWithPoints(id);

    if (items.joining) {
        items.joining->detach();
        delete items.joining;
        items.joining = nullptr;
        --lineSeriesCount_;
    }

    if (curve.interpolation == MonitorInterpolation::None) {
        // A curve back to points-only: the mark item takes the legend entry again.
        items.marks->setItemAttribute(QwtPlotItem::Legend, true);
        curveItemsById_.insert(id, items);
        flushReplot();
        return;
    }

    QwtPlotCurve *joining = nullptr;
    if (curveFactory_) {
        joining = curveFactory_(curve, plot_);
    } else {
        joining = new QwtPlotCurve(labelFor(curve));
    }
    if (!joining) {
        curveItemsById_.insert(id, items);
        flushReplot();
        return;
    }

    applyJoiningStyle(joining, curve.interpolation);
    if (joining->dataSize() == 0) {
        int stride = 1;
        bool truncated = false;
        joining->setSamples(drawnSamples(curve, &stride, &truncated));
    }
    QPen pen(curve.color, curve.lineWidth + (id == selectedCurveId_ ? kSelectedLineExtraWidth : 0.0));
    pen.setStyle(curve.lineStyle);
    joining->setPen(pen);
    joining->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    joining->setTitle(labelFor(curve));

    // Same legend rule as setCurves(): the joining stroke carries the caption.
    joining->setItemAttribute(QwtPlotItem::Legend, true);
    items.marks->setItemAttribute(QwtPlotItem::Legend, false);

    joining->attach(plot_);
    joining->setAxes(QwtAxis::XBottom, items.axisId);

    items.joining = joining;
    curveItemsById_.insert(id, items);
    ++lineSeriesCount_;
    flushReplot();
}

void MonitorChartView::rebuildFitSeriesFor(const QString &id)
{
    if (!plot_)
        return;
    CurveItems items = curveItemsById_.value(id);
    if (!items.marks)
        return;
    const MonitorCurve curve = curveWithPoints(id);

    if (items.fitted) {
        items.fitted->detach();
        delete items.fitted;
        items.fitted = nullptr;
        --fitSeriesCount_;
    }
    fitResultsById_.remove(id);

    if (curve.fit.method == MonitorFitMethod::None) {
        curveItemsById_.insert(id, items);
        rescaleYAxis(items.axisId);
        flushReplot();
        return;
    }

    // The fit describes what is drawn, so it is computed from the same decimated
    // samples the marks show.
    int stride = 1;
    bool truncated = false;
    const QVector<QPointF> samples = drawnSamples(curve, &stride, &truncated);
    const MonitorFitResult fitResult = MonitorCurveFit::fit(samples, curve.fit.method,
                                                            curve.fit.degree, curve.fit.window);
    if (!fitResult.isValid()) {
        // Kept even though nothing is drawn. Dropping it here made fitResult()
        // answer with a default-constructed value, so callers could not tell a
        // refused fit from one that was never requested -- and the note saying
        // why was discarded exactly when it mattered.
        fitResultsById_.insert(id, fitResult);
        curveItemsById_.insert(id, items);
        rescaleYAxis(items.axisId);
        flushReplot();
        return;
    }

    auto *fitted = new QwtPlotCurve(labelFor(curve));
    fitted->setStyle(QwtPlotCurve::Lines);
    fitted->setSamples(fitResult.points);
    QPen pen(curve.color, curve.fit.lineWidth + (id == selectedCurveId_ ? kSelectedLineExtraWidth : 0.0));
    pen.setStyle(curve.fit.lineStyle);
    fitted->setPen(pen);
    fitted->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    // A fitted trend never takes the legend entry; the curve itself already has one.
    fitted->setItemAttribute(QwtPlotItem::Legend, false);
    fitted->attach(plot_);
    fitted->setAxes(QwtAxis::XBottom, items.axisId);

    items.fitted = fitted;
    curveItemsById_.insert(id, items);
    fitResultsById_.insert(id, fitResult);
    ++fitSeriesCount_;
    rescaleYAxis(items.axisId);
    flushReplot();
}

void MonitorChartView::setSelectedCurve(const QString &curveId)
{
    if (selectedCurveId_ == curveId)
        return;
    const QString previous = selectedCurveId_;
    selectedCurveId_ = curveId;
    if (!previous.isEmpty())
        applyLiveStyle(previous);
    if (!curveId.isEmpty())
        applyLiveStyle(curveId);
    settle();
    emit curveSelectionChanged(selectedCurveId_);
}

QString MonitorChartView::selectedCurve() const
{
    return selectedCurveId_;
}

void MonitorChartView::setSelectedPoint(const QString &curveId, int sampleIndex)
{
    const QVector<QPointF> *points = storedPoints(curveId);
    if (!points || sampleIndex < 0 || sampleIndex >= points->size()) {
        selectedPointCurveId_.clear();
        selectedPointSample_ = -1;
        return;
    }
    selectedPointCurveId_ = curveId;
    selectedPointSample_ = sampleIndex;
}

QString MonitorChartView::selectedPointCurve() const
{
    return selectedPointCurveId_;
}

int MonitorChartView::selectedPointSample() const
{
    return selectedPointSample_;
}

int MonitorChartView::sampleIndexAt(const QString &curveId, const QPointF &value) const
{
    const QVector<QPointF> *stored = storedPoints(curveId);
    if (!stored)
        return -1;
    const QVector<QPointF> &samples = *stored;
    if (samples.isEmpty())
        return -1;

    // Nearest sample by distance. A solver column can hold several samples at the
    // same x (a restarted run repeats an iteration), so the closest point in the
    // plane is the honest answer rather than the first matching x.
    int best = 0;
    double bestDistance = -1.0;
    for (int i = 0; i < samples.size(); ++i) {
        const QPointF delta = samples.at(i) - value;
        const double distance = delta.x() * delta.x() + delta.y() * delta.y();
        if (bestDistance < 0.0 || distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

void MonitorChartView::selectAt(const QString &curveId, const QPointF &value)
{
    const int sample = sampleIndexAt(curveId, value);
    setSelectedCurve(curveId);
    if (sample >= 0)
        setSelectedPoint(curveId, sample);
    emit curveClicked(curveId);
    if (sample >= 0) {
        const QVector<QPointF> *points = storedPoints(curveId);
        const QPointF actual = points ? points->value(sample) : QPointF();
        emit pointClicked(curveId, sample, actual);
    }
}

void MonitorChartView::handlePickedPixel(const QPoint &pos)
{
    if (!plot_ || curveItemsById_.isEmpty())
        return;

    // Resolve the hit in pixel space: two curves sharing a drawing can live on
    // different Y scales, so a data-space distance would compare seconds with
    // coefficients. Pixels are what the user actually aimed at.
    const double clickX = pos.x();
    const double clickY = pos.y();

    QString bestId;
    int bestIndex = -1;
    double bestDistance = kPickRadiusPx * kPickRadiusPx;

    for (auto it = curveItemsById_.constBegin(); it != curveItemsById_.constEnd(); ++it) {
        const QString id = it.key();
        const CurveItems items = it.value();
        if (!items.marks)
            continue;
        const QVector<QPointF> *stored = storedPoints(id);
        if (!stored || stored->isEmpty())
            continue;
        const QwtScaleMap xMap = plot_->canvasMap(QwtAxis::XBottom);
        const QwtScaleMap yMap = plot_->canvasMap(items.axisId);
        const QVector<QPointF> &samples = *stored;
        for (int i = 0; i < samples.size(); ++i) {
            const double dx = xMap.transform(samples.at(i).x()) - clickX;
            const double dy = yMap.transform(samples.at(i).y()) - clickY;
            const double distance = dx * dx + dy * dy;
            if (distance <= bestDistance) {
                bestDistance = distance;
                bestId = id;
                bestIndex = i;
            }
        }
    }

    if (bestId.isEmpty() || bestIndex < 0)
        return;

    setSelectedCurve(bestId);
    setSelectedPoint(bestId, bestIndex);
    emit curveClicked(bestId);
    const QVector<QPointF> *bestPoints = storedPoints(bestId);
    emit pointClicked(bestId, bestIndex, bestPoints ? bestPoints->value(bestIndex) : QPointF());
}

} // namespace monitor
} // namespace aerosim
