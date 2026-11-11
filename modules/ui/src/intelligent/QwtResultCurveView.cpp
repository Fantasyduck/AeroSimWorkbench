#include "aerosim/ui/intelligent/QwtResultCurveView.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

namespace aerosim {
namespace {

// Two groups, matching the two prefixes of the variable tree: residuals and
// everything else. Two is also the chart widget's own ceiling, and it is not an
// arbitrary number -- residuals live many orders of magnitude away from the
// coefficients, and sharing one axis with them flattens the coefficients into a
// strip along the bottom.
const char *const kResidualGroup = "residual";
const char *const kSeriesGroup = "series";

constexpr qreal kDefaultPointSize = 2.5;
constexpr qreal kDefaultLineWidth = 0.5;

using monitor::MonitorFitMethod;
using monitor::MonitorInterpolation;

struct DrawingMode {
    const char16_t *label;
    MonitorInterpolation interpolation;
    MonitorFitMethod fit;
};

constexpr DrawingMode kDrawingModes[] = {
    {u"仅散点", MonitorInterpolation::None, MonitorFitMethod::None},
    {u"直线连接采样点", MonitorInterpolation::Linear, MonitorFitMethod::None},
    {u"样条连接采样点", MonitorInterpolation::Spline, MonitorFitMethod::None},
    {u"最小二乘直线", MonitorInterpolation::None, MonitorFitMethod::Linear},
    {u"多项式拟合", MonitorInterpolation::None, MonitorFitMethod::Polynomial},
    {u"移动平均", MonitorInterpolation::None, MonitorFitMethod::MovingAverage},
};

QColor colorAt(int index)
{
    // A fixed, readable sequence. Chosen by eye on a white canvas; the chart
    // widget keeps these when a series is hidden and shown again.
    static const QColor palette[] = {
        QColor("#165dba"), QColor("#c1440e"), QColor("#217346"), QColor("#7b3fa0"),
        QColor("#b3261e"), QColor("#0f766e"), QColor("#a16207"), QColor("#475569"),
        QColor("#be185d"), QColor("#1d4ed8"), QColor("#4d7c0f"), QColor("#9a3412")};
    const int count = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
    return palette[index % count];
}

} // namespace

QwtResultCurveView::QwtResultCurveView(QWidget *parent) : ResultCurveView(parent)
{
    setObjectName(QStringLiteral("resultCurveView"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    settingsPanel_ = new QWidget(this);
    settingsPanel_->hide();
    settingsPanel_->setObjectName(QStringLiteral("resultCurveSettingsPanel"));
    settingsPanel_->setMinimumWidth(252);
    settingsPanel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto *panelLayout = new QVBoxLayout(settingsPanel_);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    const auto addGroup = [this, panelLayout](const QString &title) {
        auto *group = new QGroupBox(title, settingsPanel_);
        auto *form = new QFormLayout(group);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        panelLayout->addWidget(group);
        return form;
    };
    auto *appearance = addGroup(QStringLiteral(u"曲线设置"));
    const auto addControl = [](QFormLayout *form, const QString &label, QWidget *control) {
        control->setAccessibleName(label);
        form->addRow(label, control);
    };

    curveChoice_ = new QComboBox(settingsPanel_);
    curveChoice_->setObjectName(QStringLiteral("resultCurveChoice"));
    curveChoice_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    curveChoice_->setMinimumContentsLength(12);
    addControl(appearance, QStringLiteral(u"编辑曲线"), curveChoice_);

    color_ = new QPushButton(QStringLiteral(u"选颜色"), this);
    color_->setObjectName(QStringLiteral("resultCurveColor"));
    addControl(appearance, QStringLiteral(u"颜色"), color_);

    shape_ = new QComboBox(settingsPanel_);
    shape_->setObjectName(QStringLiteral("resultCurvePointShape"));
    using monitor::MonitorPointShape;
    const struct { const char16_t *label; MonitorPointShape value; } shapes[] = {
        {u"圆点", MonitorPointShape::Circle}, {u"方块", MonitorPointShape::Square},
        {u"菱形", MonitorPointShape::Diamond}, {u"上三角", MonitorPointShape::Triangle},
        {u"下三角", MonitorPointShape::DownTriangle}, {u"左三角", MonitorPointShape::LeftTriangle},
        {u"右三角", MonitorPointShape::RightTriangle}, {u"十字", MonitorPointShape::Cross},
        {u"斜十字", MonitorPointShape::XCross}, {u"横线", MonitorPointShape::HLine},
        {u"竖线", MonitorPointShape::VLine}, {u"六角星", MonitorPointShape::Star1},
        {u"八角星", MonitorPointShape::Star2}, {u"六边形", MonitorPointShape::Hexagon},
        {u"不显示", MonitorPointShape::None}};
    for (const auto &shape : shapes)
        shape_->addItem(QString::fromUtf16(reinterpret_cast<const ushort *>(shape.label)),
                        static_cast<int>(shape.value));
    addControl(appearance, QStringLiteral(u"散点样式"), shape_);

    pointSize_ = new QDoubleSpinBox(settingsPanel_);
    pointSize_->setObjectName(QStringLiteral("resultCurvePointSize"));
    pointSize_->setRange(1.0, 24.0);
    pointSize_->setDecimals(1);
    pointSize_->setSingleStep(0.5);
    addControl(appearance, QStringLiteral(u"点大小"), pointSize_);

    auto *drawing = addGroup(QStringLiteral(u"连线与拟合"));
    drawingMode_ = new QComboBox(settingsPanel_);
    drawingMode_->setObjectName(QStringLiteral("resultCurveDrawingMode"));
    for (const auto &mode : kDrawingModes)
        drawingMode_->addItem(QString::fromUtf16(reinterpret_cast<const ushort *>(mode.label)));
    addControl(drawing, QStringLiteral(u"绘制方式"), drawingMode_);

    lineStyle_ = new QComboBox(settingsPanel_);
    lineStyle_->setObjectName(QStringLiteral("resultCurveLineStyle"));
    lineStyle_->addItem(QStringLiteral(u"实线"), static_cast<int>(Qt::SolidLine));
    lineStyle_->addItem(QStringLiteral(u"虚线"), static_cast<int>(Qt::DashLine));
    lineStyle_->addItem(QStringLiteral(u"点线"), static_cast<int>(Qt::DotLine));
    lineStyle_->addItem(QStringLiteral(u"点划线"), static_cast<int>(Qt::DashDotLine));
    addControl(drawing, QStringLiteral(u"线型"), lineStyle_);

    lineWidth_ = new QDoubleSpinBox(settingsPanel_);
    lineWidth_->setObjectName(QStringLiteral("resultCurveLineWidth"));
    lineWidth_->setRange(0.5, 12.0);
    lineWidth_->setDecimals(1);
    lineWidth_->setSingleStep(0.5);
    lineWidth_->setSuffix(QStringLiteral(" px"));
    addControl(drawing, QStringLiteral(u"线宽"), lineWidth_);

    fitDegree_ = new QSpinBox(settingsPanel_);
    fitDegree_->setObjectName(QStringLiteral("resultCurveFitDegree"));
    fitDegree_->setRange(1, monitor::MonitorCurveFit::maxDegree());
    addControl(drawing, QStringLiteral(u"多项式阶数"), fitDegree_);

    fitWindow_ = new QSpinBox(settingsPanel_);
    fitWindow_->setObjectName(QStringLiteral("resultCurveFitWindow"));
    fitWindow_->setRange(3, 61);
    fitWindow_->setSingleStep(2);
    addControl(drawing, QStringLiteral(u"移动平均窗口"), fitWindow_);

    fitFeedback_ = new QLabel(settingsPanel_);
    fitFeedback_->setObjectName(QStringLiteral("resultCurveFitFeedback"));
    fitFeedback_->setTextFormat(Qt::PlainText);
    fitFeedback_->setWordWrap(true);
    drawing->addRow(fitFeedback_);
    panelLayout->addStretch(1);

    chart_ = new monitor::MonitorChartView(this);
    chart_->setObjectName(QStringLiteral("resultCurveChart"));
    chart_->setMinimumHeight(200);
    chart_->enableViewNavigation();

    auto *viewTools = new QHBoxLayout;
    viewTools->addStretch();
    auto *resetView = new QPushButton(QStringLiteral(u"重置视图"), this);
    resetView->setObjectName(QStringLiteral("resultCurveResetView"));
    resetView->setToolTip(QStringLiteral(u"滚轮缩放，按住中键拖动，点击恢复全图"));
    connect(resetView, &QPushButton::clicked, chart_, &monitor::MonitorChartView::resetView);
    viewTools->addWidget(resetView);
    layout->addLayout(viewTools);
    layout->addWidget(chart_, 1);

    connect(curveChoice_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        selectCurve();
    });
    connect(chart_, &monitor::MonitorChartView::curveClicked, this, [this](const QString &id) {
        curveChoice_->setCurrentIndex(curveChoice_->findData(id));
    });
    connect(chart_, &monitor::MonitorChartView::curveStyleChanged, this, [this](const QString &id) {
        styles_.insert(id, chart_->curveStyle(id));
        if (curveChoice_->currentData().toString() == id)
            loadControls();
    });
    connect(color_, &QPushButton::clicked, this, &QwtResultCurveView::chooseColor);
    for (auto *combo : {shape_, drawingMode_, lineStyle_})
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &QwtResultCurveView::applyControls);
    for (auto *spin : {pointSize_, lineWidth_})
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &QwtResultCurveView::applyControls);
    for (auto *spin : {fitDegree_, fitWindow_})
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &QwtResultCurveView::applyControls);
    refreshCurveChoices();
}

QwtResultCurveView::~QwtResultCurveView()
{
    delete settingsPanel_.data();
}

void QwtResultCurveView::refreshCurveChoices()
{
    const auto ids = chart_->curveIds();
    const auto labels = chart_->legendLabels();
    const QString previous = curveChoice_->currentData().toString();
    const QSignalBlocker blocker(curveChoice_);
    curveChoice_->clear();
    for (int index = 0; index < ids.size(); ++index)
        curveChoice_->addItem(labels.at(index), ids.at(index));
    if (ids.isEmpty())
        curveChoice_->addItem(QStringLiteral(u"请先选择表格行"));
    const int previousIndex = curveChoice_->findData(previous);
    curveChoice_->setCurrentIndex(previousIndex < 0 ? 0 : previousIndex);
    curveChoice_->setEnabled(!ids.isEmpty());
    selectCurve();
}

void QwtResultCurveView::selectCurve()
{
    chart_->setSelectedCurve(curveChoice_->currentData().toString());
    loadControls();
}

void QwtResultCurveView::loadControls()
{
    using namespace monitor;
    const QScopedValueRollback<bool> loading(loadingControls_, true);
    const QString id = curveChoice_->currentData().toString();
    const bool active = !id.isEmpty();
    auto style = chart_->curveStyle(id);
    if (!active) {
        style.pointSize = kDefaultPointSize;
        style.lineWidth = kDefaultLineWidth;
        style.fit.lineWidth = kDefaultLineWidth;
    }
    shape_->setCurrentIndex(shape_->findData(static_cast<int>(style.pointShape)));
    pointSize_->setValue(style.pointSize);
    const bool fitting = style.fit.method != MonitorFitMethod::None;
    const auto interpolation = fitting ? MonitorInterpolation::None : style.interpolation;
    for (int i = 0; i < static_cast<int>(sizeof(kDrawingModes) / sizeof(kDrawingModes[0])); ++i) {
        if (kDrawingModes[i].interpolation == interpolation && kDrawingModes[i].fit == style.fit.method) {
            drawingMode_->setCurrentIndex(i);
            break;
        }
    }
    const auto pen = fitting ? style.fit.lineStyle : style.lineStyle;
    lineStyle_->setCurrentIndex(lineStyle_->findData(static_cast<int>(pen)));
    lineWidth_->setValue(fitting ? style.fit.lineWidth : style.lineWidth);
    fitDegree_->setValue(style.fit.degree);
    fitWindow_->setValue(style.fit.window);

    QPixmap swatch(14, 14);
    swatch.fill(style.color);
    color_->setIcon(active ? QIcon(swatch) : QIcon());
    color_->setEnabled(active);
    shape_->setEnabled(active);
    pointSize_->setEnabled(active && style.pointShape != MonitorPointShape::None);
    drawingMode_->setEnabled(active);
    const bool hasLine = active && (fitting || interpolation != MonitorInterpolation::None);
    lineStyle_->setEnabled(hasLine);
    lineWidth_->setEnabled(hasLine);
    fitDegree_->setEnabled(active && style.fit.method == MonitorFitMethod::Polynomial);
    fitWindow_->setEnabled(active && style.fit.method == MonitorFitMethod::MovingAverage);

    const auto fit = chart_->fitResult(id);
    QString feedback = fit.note;
    if (fit.isValid()) {
        QString summary;
        switch (style.fit.method) {
        case MonitorFitMethod::Linear:
            summary = QStringLiteral(u"最小二乘直线 · R² = %1").arg(fit.rSquared, 0, 'f', 3);
            break;
        case MonitorFitMethod::Polynomial:
            summary = QStringLiteral(u"%1 阶多项式 · R² = %2").arg(fit.degree).arg(fit.rSquared, 0, 'f', 3);
            break;
        case MonitorFitMethod::MovingAverage:
            summary = QStringLiteral(u"移动平均 · 窗口 %1 点").arg(fit.window);
            break;
        case MonitorFitMethod::None:
            break;
        }
        if (!feedback.isEmpty())
            summary += QLatin1Char('\n') + feedback;
        feedback = summary;
    }
    fitFeedback_->setText(feedback);
    fitFeedback_->setVisible(!feedback.isEmpty());
}

void QwtResultCurveView::applyControls()
{
    const QString id = curveChoice_->currentData().toString();
    if (loadingControls_ || id.isEmpty() || drawingMode_->currentIndex() < 0)
        return;

    auto style = chart_->curveStyle(id);
    style.label.clear();
    style.pointShape = static_cast<monitor::MonitorPointShape>(shape_->currentData().toInt());
    style.pointSize = pointSize_->value();
    const auto &mode = kDrawingModes[drawingMode_->currentIndex()];
    if (mode.interpolation == style.interpolation && mode.fit == style.fit.method) {
        const auto pen = static_cast<Qt::PenStyle>(lineStyle_->currentData().toInt());
        if (mode.fit != MonitorFitMethod::None) {
            style.fit.lineStyle = pen;
            style.fit.lineWidth = lineWidth_->value();
        } else if (mode.interpolation != MonitorInterpolation::None) {
            style.lineStyle = pen;
            style.lineWidth = lineWidth_->value();
        }
    }
    style.interpolation = mode.interpolation;
    style.fit.method = mode.fit;
    style.fit.degree = fitDegree_->value();
    style.fit.window = fitWindow_->value();
    chart_->setCurveStyle(id, style);
}

void QwtResultCurveView::chooseColor()
{
    const QString id = curveChoice_->currentData().toString();
    auto style = chart_->curveStyle(id);
    const QColor picked = QColorDialog::getColor(style.color, settingsPanel_, QStringLiteral(u"选择曲线颜色"));
    if (!picked.isValid())
        return;
    style.label.clear();
    style.color = picked;
    chart_->setCurveStyle(id, style);
}

void QwtResultCurveView::replaceSeries(const ResultTable &table, const QStringList &keys,
                                       const QString &axisLabel)
{
    if (chart_ == nullptr)
        return;

    const QString datasetId = table.sourcePath.isEmpty()
        ? table.datasetId : QFileInfo(table.sourcePath).absoluteFilePath();
    QVector<monitor::MonitorCurve> curves;
    curves.reserve(keys.size());
    bool hasResidual = false;
    bool hasSeries = false;

    for (int index = 0; index < keys.size(); ++index) {
        const QString &key = keys.at(index);
        const QVector<double> values = table.samples.value(key);
        if (values.isEmpty())
            continue;

        monitor::MonitorCurve curve;
        curve.key = key;
        curve.groupKey = QString::fromLatin1(kResidualGroup);
        curve.groupTitle = QStringLiteral(u"残差");
        curve.datasetId = datasetId;
        int colorIndex = index;
        for (int column = 0; column < table.variables.size(); ++column) {
            if (table.variables.at(column).key == key) {
                colorIndex = column;
                break;
            }
        }
        curve.color = colorAt(colorIndex);
        curve.pointSize = kDefaultPointSize;
        curve.lineWidth = kDefaultLineWidth;
        curve.fit.lineWidth = kDefaultLineWidth;
        const auto saved = styles_.constFind(monitor::MonitorChartView::curveId(curve));
        if (saved != styles_.constEnd()) {
            curve.color = saved->color;
            curve.lineStyle = saved->lineStyle;
            curve.lineWidth = saved->lineWidth;
            curve.pointShape = saved->pointShape;
            curve.pointSize = saved->pointSize;
            curve.interpolation = saved->interpolation;
            curve.fit = saved->fit;
        }

        for (int row = 0; row < values.size() && row < table.axisValues.size(); ++row) {
            const double value = values.at(row);
            if (!std::isfinite(value) || !std::isfinite(table.axisValues.at(row)))
                continue;
            curve.points.append(QPointF(table.axisValues.at(row), value));
        }
        if (curve.points.isEmpty())
            continue;

        // Residuals get their own axis and a logarithmic scale, because that is
        // how convergence is read; everything else shares a linear axis. The
        // chart widget degrades the logarithmic request to linear on its own
        // when a value is not positive, so a signed residual file still draws.
        bool residual = false;
        for (const ResultVariable &variable : table.variables) {
            if (variable.key == key) {
                residual = variable.isResidual;
                break;
            }
        }
        if (residual) {
            curve.groupKey = QString::fromLatin1(kResidualGroup);
            curve.groupTitle = QStringLiteral(u"残差");
            curve.logarithmic = true;
            hasResidual = true;
        } else {
            curve.groupKey = QString::fromLatin1(kSeriesGroup);
            curve.groupTitle = QStringLiteral(u"变量");
            curve.logarithmic = false;
            hasSeries = true;
        }
        if (!table.variables.isEmpty()) {
            for (const ResultVariable &variable : table.variables) {
                if (variable.key == key && !variable.unit.isEmpty()) {
                    curve.label = QStringLiteral("%1 (%2)").arg(key, variable.unit);
                    break;
                }
            }
        }
        curves.append(curve);
    }

    if (curves.isEmpty()) {
        clearSeries();
        return;
    }

    // Label each Y axis with the quantities actually selected in the table.
    QStringList residualLabels;
    QStringList seriesLabels;
    for (const auto &curve : curves) {
        const QString label = curve.label.isEmpty() ? curve.key : curve.label;
        (curve.logarithmic ? residualLabels : seriesLabels).append(label);
    }
    for (auto &curve : curves)
        curve.groupTitle = (curve.logarithmic ? residualLabels : seriesLabels).join(QStringLiteral(", "));

    chart_->setAxisLabel(axisLabel);
    // The title names the quantities, not the count: "残差" and "变量" are what
    // the reader is looking at, and the status line already carries the counts.
    if (hasResidual && hasSeries)
        chart_->setChartTitle(QStringLiteral(u"残差与变量"));
    else if (hasResidual)
        chart_->setChartTitle(QStringLiteral(u"残差"));
    else
        chart_->setChartTitle(QStringLiteral(u"变量"));
    chart_->setCurves(curves);
    refreshCurveChoices();
}

void QwtResultCurveView::clearSeries()
{
    chart_->clearCurves();
    refreshCurveChoices();
}

} // namespace aerosim
