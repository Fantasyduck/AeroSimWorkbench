#include "MonitorChartDemoWindow.h"

#include "MonitorCurveFit.h"

#include "MonitorFileTailSource.h"
#include "MonitorLogReader.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace aerosim {
namespace monitor {
namespace demo {
namespace {

// Two magnitudes belong on one axis only when they are within one decade of each
// other; wider than that and the larger one flattens the smaller ones into a
// band along the bottom -- the exact defect docs/PostProcessMonitorSeriesPlan.md
// 5.4 warns about. One chart per magnitude avoids it without dropping data.
constexpr double kMagnitudeRatio = 10.0;

const char *const kPalette[] = {
    "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd",
    "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"
};
constexpr int kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);

QColor colorAt(int index)
{
    return QColor(QString::fromLatin1(kPalette[index % kPaletteSize]));
}

QString idOf(const MonitorCurve &curve)
{
    return MonitorChartView::makeCurveId(curve.datasetId, curve.key);
}

// Compared element by element on purpose: QList<QString>::operator== expands a
// Qt 5.12 macro that routes through stdext, which MSVC 19.51 removed.
bool sameKeys(const QStringList &left, const QStringList &right)
{
    if (left.size() != right.size())
        return false;
    for (int i = 0; i < left.size(); ++i) {
        if (left.at(i) != right.at(i))
            return false;
    }
    return true;
}

} // namespace

MonitorChartDemoWindow::MonitorChartDemoWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("监控曲线演示 — MonitorChartView / Qwt 7.5.2"));

    auto *central = new QWidget(this);
    auto *outer = new QVBoxLayout(central);

    // ---- Toolbar ---------------------------------------------------------
    auto *toolbar = new QHBoxLayout;
    auto *importButton = new QPushButton(QStringLiteral("导入历史文件…"), central);
    auto *clearButton = new QPushButton(QStringLiteral("清除"), central);
    auto *exportButton = new QPushButton(QStringLiteral("导出 PNG…"), central);
    // Following turns the same file into a live source: the solver writes rows,
    // the timer picks them up, and the curves grow without being rebuilt.
    followButton_ = new QPushButton(QStringLiteral("跟随实时文件"), central);
    followButton_->setCheckable(true);
    followButton_->setToolTip(QStringLiteral("按 500 ms 轮询文件新增的行，增量追加到图上"));
    liveLabel_ = new QLabel(QStringLiteral("未跟随实时文件"), central);
    fileLabel_ = new QLabel(QStringLiteral("尚未加载数据"), central);

    axisCombo_ = new QComboBox(central);
    logCheck_ = new QCheckBox(QStringLiteral("残差对数轴"), central);
    logCheck_->setChecked(residualLogarithmic_);
    overlayCheck_ = new QCheckBox(QStringLiteral("叠加成一张图"), central);
    // Sampling interval: every Nth sample. This is the control a chart panel
    // offers, because it says how finely to read the series rather than how much
    // data happens to be in the file.
    intervalSpin_ = new QSpinBox(central);
    intervalSpin_->setRange(1, 1000);
    intervalSpin_->setSingleStep(1);
    intervalSpin_->setValue(1);
    intervalSpin_->setToolTip(QStringLiteral("每 N 个采样点画 1 个；末尾点始终保留"));
    // What an edit reaches. A user works in groups -- "the residuals", "these
    // three coefficients" -- so the scope is part of the edit, not a loop the
    // caller has to write.
    scopeCombo_ = new QComboBox(central);
    scopeCombo_->addItem(QStringLiteral("选中曲线"), static_cast<int>(MonitorChartView::EditScope::Curves));
    scopeCombo_->addItem(QStringLiteral("所在组"), static_cast<int>(MonitorChartView::EditScope::Group));
    scopeCombo_->addItem(QStringLiteral("全部曲线"), static_cast<int>(MonitorChartView::EditScope::All));

    toolbar->addWidget(importButton);
    toolbar->addWidget(clearButton);
    toolbar->addWidget(exportButton);
    toolbar->addWidget(followButton_);
    toolbar->addSpacing(12);
    toolbar->addWidget(fileLabel_, 1);
    toolbar->addWidget(liveLabel_);
    toolbar->addWidget(new QLabel(QStringLiteral("横轴"), central));
    toolbar->addWidget(axisCombo_);
    toolbar->addWidget(logCheck_);
    toolbar->addWidget(overlayCheck_);
    toolbar->addWidget(new QLabel(QStringLiteral("采样间隔"), central));
    toolbar->addWidget(intervalSpin_);
    outer->addLayout(toolbar);

    // ---- Curves tree + charts -------------------------------------------
    auto *splitter = new QSplitter(Qt::Horizontal, central);
    curveTree_ = new QTreeWidget(splitter);
    curveTree_->setHeaderLabels(QStringList() << QStringLiteral("曲线") << QStringLiteral("单位"));
    curveTree_->setRootIsDecorated(true);
    curveTree_->header()->setStretchLastSection(false);
    curveTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    // Group captions list their quantity names, which can run long; give the
    // column room before the user has to drag the splitter.
    curveTree_->setMinimumWidth(240);
    curveTree_->setColumnWidth(0, 300);
    // Ctrl-click instead of one-at-a-time selection: "these three curves" is the
    // unit the scoped edits are built on.
    curveTree_->setSelectionMode(QAbstractItemView::ExtendedSelection);

    chartScroll_ = new QScrollArea(splitter);
    chartScroll_->setWidgetResizable(true);
    chartHost_ = new QWidget(chartScroll_);
    chartHostLayout_ = new QVBoxLayout(chartHost_);
    chartHostLayout_->setContentsMargins(6, 6, 6, 6);
    chartScroll_->setWidget(chartHost_);
    splitter->addWidget(curveTree_);
    splitter->addWidget(chartScroll_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes(QList<int>() << 330 << 900);
    outer->addWidget(splitter, 1);

    // ---- Style panel -----------------------------------------------------
    auto *stylePanel = new QHBoxLayout;
    selectionLabel_ = new QLabel(QStringLiteral("未选中曲线"), central);
    selectionLabel_->setMinimumWidth(200);
    labelEdit_ = new QLineEdit(central);
    labelEdit_->setPlaceholderText(QStringLiteral("图例名称"));
    colorButton_ = new QPushButton(QStringLiteral("颜色"), central);
    colorButton_->setFixedWidth(64);
    lineStyleCombo_ = new QComboBox(central);
    lineStyleCombo_->addItem(QStringLiteral("实线"), static_cast<int>(Qt::SolidLine));
    lineStyleCombo_->addItem(QStringLiteral("虚线"), static_cast<int>(Qt::DashLine));
    lineStyleCombo_->addItem(QStringLiteral("点线"), static_cast<int>(Qt::DotLine));
    lineStyleCombo_->addItem(QStringLiteral("点划线"), static_cast<int>(Qt::DashDotLine));
    // Every marker Qwt can draw. The four content-driven shapes (path, pixmap,
    // graphic, SVG) are fed by demoPointContent_; a caller with real artwork
    // would pass its own through MonitorCurveStyle::pointContent.
    demoPointContent_ = buildDemoPointContent();
    shapeCombo_ = new QComboBox(central);
    addShapeItem(QStringLiteral("不画标记"), MonitorPointShape::None);
    addShapeItem(QStringLiteral("圆点"), MonitorPointShape::Circle);
    addShapeItem(QStringLiteral("方块"), MonitorPointShape::Square);
    addShapeItem(QStringLiteral("菱形"), MonitorPointShape::Diamond);
    addShapeItem(QStringLiteral("上三角"), MonitorPointShape::Triangle);
    addShapeItem(QStringLiteral("下三角"), MonitorPointShape::DownTriangle);
    addShapeItem(QStringLiteral("上三角（同三角形）"), MonitorPointShape::UpTriangle);
    addShapeItem(QStringLiteral("左三角"), MonitorPointShape::LeftTriangle);
    addShapeItem(QStringLiteral("右三角"), MonitorPointShape::RightTriangle);
    addShapeItem(QStringLiteral("十字"), MonitorPointShape::Cross);
    addShapeItem(QStringLiteral("斜十字"), MonitorPointShape::XCross);
    addShapeItem(QStringLiteral("横线"), MonitorPointShape::HLine);
    addShapeItem(QStringLiteral("竖线"), MonitorPointShape::VLine);
    addShapeItem(QStringLiteral("六角星"), MonitorPointShape::Star1);
    addShapeItem(QStringLiteral("八角星"), MonitorPointShape::Star2);
    addShapeItem(QStringLiteral("六边形"), MonitorPointShape::Hexagon);
    addShapeItem(QStringLiteral("自定义路径"), MonitorPointShape::Path);
    addShapeItem(QStringLiteral("位图"), MonitorPointShape::Pixmap);
    addShapeItem(QStringLiteral("绘制图形"), MonitorPointShape::Graphic);
    addShapeItem(QStringLiteral("SVG"), MonitorPointShape::Svg);
    pointSizeSpin_ = new QDoubleSpinBox(central);
    pointSizeSpin_->setRange(1.0, 24.0);
    pointSizeSpin_->setSingleStep(0.5);
    pointSizeSpin_->setValue(5.0);
    interpolationCombo_ = new QComboBox(central);
    interpolationCombo_->addItem(QStringLiteral("仅散点"), static_cast<int>(MonitorInterpolation::None));
    interpolationCombo_->addItem(QStringLiteral("直线连接"), static_cast<int>(MonitorInterpolation::Linear));
    interpolationCombo_->addItem(QStringLiteral("样条连接"), static_cast<int>(MonitorInterpolation::Spline));

    stylePanel->addWidget(selectionLabel_);
    stylePanel->addWidget(new QLabel(QStringLiteral("名称"), central));
    stylePanel->addWidget(labelEdit_, 1);
    stylePanel->addWidget(colorButton_);
    stylePanel->addWidget(new QLabel(QStringLiteral("线型"), central));
    stylePanel->addWidget(lineStyleCombo_);
    stylePanel->addWidget(new QLabel(QStringLiteral("点形"), central));
    stylePanel->addWidget(shapeCombo_);
    stylePanel->addWidget(new QLabel(QStringLiteral("点大小"), central));
    stylePanel->addWidget(pointSizeSpin_);
    stylePanel->addWidget(new QLabel(QStringLiteral("作用范围"), central));
    stylePanel->addWidget(scopeCombo_);
    stylePanel->addWidget(new QLabel(QStringLiteral("连接方式"), central));
    stylePanel->addWidget(interpolationCombo_);

    // Fitting. Kept apart from the joining combo above because the two answer
    // different questions: joining connects neighbouring samples, a fit
    // approximates the whole trend and normally passes through no sample at all.
    fitCombo_ = new QComboBox(central);
    fitCombo_->addItem(QStringLiteral("不拟合"), static_cast<int>(MonitorFitMethod::None));
    fitCombo_->addItem(QStringLiteral("直线拟合"), static_cast<int>(MonitorFitMethod::Linear));
    fitCombo_->addItem(QStringLiteral("多项式拟合"), static_cast<int>(MonitorFitMethod::Polynomial));
    fitCombo_->addItem(QStringLiteral("移动平均"), static_cast<int>(MonitorFitMethod::MovingAverage));
    fitDegreeSpin_ = new QSpinBox(central);
    fitDegreeSpin_->setRange(1, MonitorCurveFit::maxDegree());
    fitDegreeSpin_->setValue(3);
    fitDegreeSpin_->setToolTip(QStringLiteral("多项式阶数"));
    fitWindowSpin_ = new QSpinBox(central);
    fitWindowSpin_->setRange(3, 61);
    fitWindowSpin_->setSingleStep(2);
    fitWindowSpin_->setValue(7);
    fitWindowSpin_->setToolTip(QStringLiteral("移动平均窗口（采样点数）"));
    fitLineStyleCombo_ = new QComboBox(central);
    fitLineStyleCombo_->addItem(QStringLiteral("虚线"), static_cast<int>(Qt::DashLine));
    fitLineStyleCombo_->addItem(QStringLiteral("点线"), static_cast<int>(Qt::DotLine));
    fitLineStyleCombo_->addItem(QStringLiteral("实线"), static_cast<int>(Qt::SolidLine));
    stylePanel->addWidget(new QLabel(QStringLiteral("拟合方式"), central));
    stylePanel->addWidget(fitCombo_);
    stylePanel->addWidget(new QLabel(QStringLiteral("拟合阶数/窗口"), central));
    stylePanel->addWidget(fitDegreeSpin_);
    stylePanel->addWidget(fitWindowSpin_);
    stylePanel->addWidget(new QLabel(QStringLiteral("拟合线型"), central));
    stylePanel->addWidget(fitLineStyleCombo_);
    outer->addLayout(stylePanel);

    setCentralWidget(central);
    resize(1280, 860);

    connect(importButton, &QPushButton::clicked, this, &MonitorChartDemoWindow::onImportClicked);
    connect(clearButton, &QPushButton::clicked, this, &MonitorChartDemoWindow::onClearClicked);
    connect(exportButton, &QPushButton::clicked, this, &MonitorChartDemoWindow::onExportClicked);
    connect(followButton_, &QPushButton::clicked, this, &MonitorChartDemoWindow::onFollowClicked);

    followTimer_ = new QTimer(this);
    followTimer_->setInterval(500);
    connect(followTimer_, &QTimer::timeout, this, &MonitorChartDemoWindow::onFollowTicked);
    connect(axisCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MonitorChartDemoWindow::onAxisChanged);
    connect(logCheck_, &QCheckBox::toggled, this, &MonitorChartDemoWindow::onLogToggled);
    connect(overlayCheck_, &QCheckBox::toggled, this, &MonitorChartDemoWindow::onOverlayToggled);
    connect(intervalSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MonitorChartDemoWindow::onIntervalChanged);
    connect(curveTree_, &QTreeWidget::itemChanged, this, &MonitorChartDemoWindow::onItemChanged);
    connect(curveTree_, &QTreeWidget::currentItemChanged, this, &MonitorChartDemoWindow::onCurrentItemChanged);
    connect(labelEdit_, &QLineEdit::textEdited, this, &MonitorChartDemoWindow::onStyleEdited);
    connect(colorButton_, &QPushButton::clicked, this, &MonitorChartDemoWindow::onColorClicked);
    connect(lineStyleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MonitorChartDemoWindow::onStyleEdited);
    connect(shapeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MonitorChartDemoWindow::onStyleEdited);
    connect(pointSizeSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MonitorChartDemoWindow::onStyleEdited);
    connect(interpolationCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MonitorChartDemoWindow::onStyleEdited);
    connect(fitCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MonitorChartDemoWindow::onStyleEdited);
    connect(fitDegreeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MonitorChartDemoWindow::onStyleEdited);
    connect(fitWindowSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MonitorChartDemoWindow::onStyleEdited);
    connect(fitLineStyleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MonitorChartDemoWindow::onStyleEdited);

    loadStylePanel(QString());
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

bool MonitorChartDemoWindow::loadFile(const QString &path, QString *error)
{
    MonitorLog log;
    QString message;
    if (!MonitorLogReader::read(path, &log, &message)) {
        if (error) *error = message;
        return false;
    }
    if (!model_.setLog(log, &message)) {
        if (error) *error = message;
        return false;
    }

    sourcePath_ = path;
    fileLabel_->setText(QStringLiteral("%1 — %2 行 · %3 条曲线列")
                            .arg(QFileInfo(path).fileName())
                            .arg(model_.rowCount())
                            .arg(model_.seriesColumns().size()));
    if (isFollowing()) {
        stopFollowing();
        followButton_->setChecked(false);
        followButton_->setText(QStringLiteral("跟随实时文件"));
    }
    refreshAxisCombo();
    buildCurves();
    curveKeys_.clear();
    for (const MonitorCurve &curve : curves_)
        curveKeys_.append(curve.key);
    rebuildCharts();
    populateTree();
    refreshStatus();
    // Start on the first curve so the style panel below the charts has something
    // to show; otherwise it sits on "no curve selected" until the user clicks.
    currentId_.clear();
    if (!curves_.isEmpty()) selectCurve(idOf(curves_.first()));
    return true;
}

void MonitorChartDemoWindow::refreshAxisCombo()
{
    updating_ = true;
    const int previous = axisCombo_->currentIndex();
    axisCombo_->clear();
    QString reason;
    if (model_.axisAvailable(MonitorAxis::IterationStep, &reason)) {
        axisCombo_->addItem(model_.axisLabel(MonitorAxis::IterationStep),
                            static_cast<int>(MonitorAxis::IterationStep));
    }
    if (model_.axisAvailable(MonitorAxis::TimeIndex, &reason)) {
        axisCombo_->addItem(model_.axisLabel(MonitorAxis::TimeIndex),
                            static_cast<int>(MonitorAxis::TimeIndex));
    }
    if (axisCombo_->count() == 0) {
        axisCombo_->addItem(QStringLiteral("无可用横轴"), static_cast<int>(MonitorAxis::IterationStep));
    }
    if (previous >= 0 && previous < axisCombo_->count()) {
        axisCombo_->setCurrentIndex(previous);
    }
    updating_ = false;
}

void MonitorChartDemoWindow::buildCurves()
{
    curves_.clear();
    const QStringList keys = model_.seriesColumns();
    for (int i = 0; i < keys.size(); ++i) {
        const QString &key = keys.at(i);
        MonitorCurve curve;
        curve.key = key;
        curve.label = key;
        curve.datasetId = model_.datasetId();
        curve.points = model_.points(key, axis_);
        curve.color = colorAt(i);
        curve.pointSize = 5.0;
        curve.pointShape = MonitorPointShape::Circle;
        curve.interpolation = MonitorInterpolation::None;
        const bool suggestedLog = model_.suggestedScale(key) == MonitorScale::Logarithmic;
        curve.logarithmic = model_.isResidual(key) ? (suggestedLog && residualLogarithmic_) : suggestedLog;
        curve.visible = true;
        curves_.append(curve);
    }
}

// ---------------------------------------------------------------------------
// Magnitude grouping
// ---------------------------------------------------------------------------

namespace {

double maxAbsOf(const MonitorCurve &curve)
{
    double peak = 0.0;
    for (const QPointF &p : curve.points) {
        const double magnitude = qAbs(p.y());
        if (magnitude > peak) peak = magnitude;
    }
    return peak;
}

} // namespace

void MonitorChartDemoWindow::rebuildCharts()
{
    // Tear the old layout down. Widgets are hidden and deferred-deleted because a
    // rebuild can be triggered from inside one of those very widgets' signals.
    while (QLayoutItem *item = chartHostLayout_->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    charts_.clear();
    chartStatus_.clear();
    groupOrder_.clear();
    curveGroup_.clear();
    visibleGroups_.clear();

    if (curves_.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("导入一份求解器历史文件即可显示曲线。"), chartHost_);
        empty->setAlignment(Qt::AlignCenter);
        chartHostLayout_->addWidget(empty, 1);
        return;
    }

    // Overlay is the honest baseline: one drawing, and Qwt can carry exactly two
    // magnitudes there (a scale on each side), so the rest has to be dropped --
    // which is why the default layout is one chart per magnitude.
    if (overlayMode_) {
        for (const MonitorCurve &c : curves_) {
            curveGroup_.insert(idOf(c), QStringLiteral("overlay"));
        }
        groupOrder_.append(QStringLiteral("overlay"));
        auto *chart = new MonitorChartView(chartHost_);
        // The drawing is captioned by what it shows, not by how much of it there
        // is; the counts are in the status line under the chart.
        chart->setChartTitle(QStringLiteral("残差与气动系数"));
        chart->setMinimumHeight(360);
        chartHostLayout_->addWidget(chart);
        auto *status = new QLabel(chartHost_);
        status->setWordWrap(true);
        QFont small = status->font();
        small.setPointSize(9);
        status->setFont(small);
        chartHostLayout_->addWidget(status);
        charts_.insert(QStringLiteral("overlay"), chart);
        chartStatus_.insert(QStringLiteral("overlay"), status);
        connect(chart, &MonitorChartView::curveClicked, this, &MonitorChartDemoWindow::onSelectionFromChart);
        chartHostLayout_->addStretch(1);
        applyCurves();
        return;
    }

    // Split layout: residuals on their own chart, coefficients bucketed by
    // magnitude so a large quantity cannot flatten its neighbours.
    QStringList residualIds;
    QStringList coefficientIds;
    for (const MonitorCurve &c : curves_) {
        if (model_.isResidual(c.key)) residualIds.append(idOf(c)); else coefficientIds.append(idOf(c));
    }

    struct Bucket {
        QStringList ids;
        QString title;
    };
    QVector<Bucket> buckets;

    if (!residualIds.isEmpty()) {
        Bucket b;
        b.ids = residualIds;
        b.title = QStringLiteral("残差");
        buckets.append(b);
    }

    if (!coefficientIds.isEmpty()) {
        QVector<QPair<double, QString> > byMagnitude;
        for (const MonitorCurve &c : curves_) {
            if (model_.isResidual(c.key)) continue;
            byMagnitude.append(qMakePair(maxAbsOf(c), idOf(c)));
        }
        std::sort(byMagnitude.begin(), byMagnitude.end(),
                  [](const QPair<double, QString> &a, const QPair<double, QString> &b) {
                      return a.first > b.first;
                  });

        QVector<double> seeds;
        QVector<QStringList> grouped;
        for (const QPair<double, QString> &entry : byMagnitude) {
            int target = -1;
            if (entry.first <= 0.0) {
                // A column that never leaves zero has no magnitude of its own; it
                // rides along with the smallest bucket rather than inventing one.
                if (!grouped.isEmpty()) target = grouped.size() - 1;
            } else {
                for (int i = 0; i < seeds.size(); ++i) {
                    if (entry.first * kMagnitudeRatio >= seeds.at(i)) {
                        target = i;
                        break;
                    }
                }
            }
            if (target < 0) {
                seeds.append(entry.first);
                grouped.append(QStringList());
                target = grouped.size() - 1;
            }
            grouped[target].append(entry.second);
        }

        for (int i = 0; i < grouped.size(); ++i) {
            Bucket b;
            b.ids = grouped.at(i);
            QStringList keys;
            for (const QString &id : b.ids) {
                for (const MonitorCurve &c : curves_) {
                    if (idOf(c) == id) { keys.append(c.key); break; }
                }
            }
            // The caption is the list of quantities in the bucket. Counting them
            // ("6 条同量级") says nothing about what is plotted; the reader can
            // count the curves in the legend if that is what they want to know.
            b.title = QStringLiteral("系数：") + keys.join(QStringLiteral(" · "));
            buckets.append(b);
        }
    }

    for (int i = 0; i < buckets.size(); ++i) {
        const QString chartKey = QStringLiteral("g%1").arg(i);
        for (const QString &id : buckets.at(i).ids) {
            curveGroup_.insert(id, chartKey);
        }
        groupOrder_.append(chartKey);

        auto *chart = new MonitorChartView(chartHost_);
        // The bucket title names the quantities in the drawing; series counts go
        // to the status line, where they do not compete with the subject.
        chart->setChartTitle(buckets.at(i).title);
        chart->setMinimumHeight(330);
        chartHostLayout_->addWidget(chart);

        auto *status = new QLabel(chartHost_);
        status->setWordWrap(true);
        QFont small = status->font();
        small.setPointSize(9);
        status->setFont(small);
        chartHostLayout_->addWidget(status);

        charts_.insert(chartKey, chart);
        chartStatus_.insert(chartKey, status);
        connect(chart, &MonitorChartView::curveClicked, this, &MonitorChartDemoWindow::onSelectionFromChart);
    }

    chartHostLayout_->addStretch(1);
    applyCurves();
}

void MonitorChartDemoWindow::applyCurves()
{
    // The interval travels with the curve styles below, per curve, so nothing
    // plot-wide is needed here. The global budget stays as the safety cap a
    // scripted run can still set through setPointBudget(); read it back rather
    // than passing 0, or a rebuild would quietly drop the cap.
    int budget = pointBudget_;
    for (const QString &chartKey : groupOrder_) {
        MonitorChartView *chart = charts_.value(chartKey);
        if (!chart) continue;

        QVector<MonitorCurve> subset;
        QStringList keys;
        for (const MonitorCurve &c : curves_) {
            if (curveGroup_.value(idOf(c)) != chartKey) continue;
            subset.append(c);
            keys.append(c.key);
        }

        for (MonitorCurve &c : subset) {
            // Every curve in a split chart shares one scale; in the overlay the
            // two axes are residuals and coefficients (plan 5.4).
            if (chartKey == QLatin1String("overlay")) {
                c.groupKey = model_.isResidual(c.key) ? QStringLiteral("residual")
                                                      : QStringLiteral("coefficient");
                c.groupTitle = model_.isResidual(c.key) ? QStringLiteral("残差")
                                                        : QStringLiteral("气动系数");
            } else {
                c.groupKey = chartKey;
                c.groupTitle = model_.isResidual(c.key)
                                   ? QStringLiteral("残差")
                                   : (keys.size() <= 3 ? QStringLiteral("系数：") + keys.join(QStringLiteral(" · "))
                                                       : QStringLiteral("系数"));
            }
        }

        chart->setAxisLabel(model_.axisLabel(axis_));
        chart->setPointBudget(budget);
        chart->setCurves(subset);
        // setCurves() installs a fresh plot, so styles and captions have to be
        // handed back -- the widget keeps them, but only for the current drawing.
        for (const MonitorCurve &c : subset) {
            const QString id = idOf(c);
            MonitorCurveStyle style;
            style.label = c.label;
            style.color = c.color;
            style.lineStyle = c.lineStyle;
            style.pointSize = c.pointSize;
            style.pointShape = c.pointShape;
            style.interpolation = c.interpolation;
            style.fit = c.fit;
            style.sampleInterval = c.sampleInterval;
            chart->setCurveStyle(id, style);
        }
        if (!currentId_.isEmpty() && chart->curveIds().contains(currentId_)) {
            chart->setSelectedCurve(currentId_);
        }

        QLabel *status = chartStatus_.value(chartKey);
        if (status) status->setText(chart->statusText());

        // A group whose curves are all unchecked keeps its chart but empties it;
        // hiding the whole block is what makes "only the residuals" read as a
        // single drawing instead of a stack of blank frames.
        int visible = 0;
        for (const MonitorCurve &c : subset) {
            if (c.visible) ++visible;
        }
        const bool showGroup = visible > 0;
        if (showGroup) visibleGroups_.insert(chartKey); else visibleGroups_.remove(chartKey);
        chart->setVisible(showGroup);
        if (status) status->setVisible(showGroup);
    }
}

void MonitorChartDemoWindow::populateTree()
{
    updating_ = true;
    curveTree_->clear();
    for (const QString &chartKey : groupOrder_) {
        QStringList keys;
        QVector<const MonitorCurve *> members;
        for (const MonitorCurve &c : curves_) {
            if (curveGroup_.value(idOf(c)) != chartKey) continue;
            members.append(&c);
            keys.append(c.key);
        }
        // Same naming as the chart captions: a group says which quantities it
        // holds, never how many curves it happens to contain.
        QString title;
        if (chartKey == QLatin1String("overlay")) {
            title = QStringLiteral("叠加图");
        } else if (!keys.isEmpty()) {
            title = QStringLiteral("系数：") + keys.join(QStringLiteral(" · "));
            if (model_.isResidual(members.first()->key)) title = QStringLiteral("残差");
        }

        auto *group = new QTreeWidgetItem(curveTree_);
        group->setText(0, title);
        // A narrow panel elides the caption; the tooltip keeps it readable.
        group->setToolTip(0, title);
        group->setFlags(group->flags() & ~Qt::ItemIsUserCheckable);
        group->setExpanded(true);

        for (const MonitorCurve *c : members) {
            auto *item = new QTreeWidgetItem(group);
            item->setText(0, c->key);
            item->setText(1, model_.unit(c->key));
            item->setData(0, Qt::UserRole, idOf(*c));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, c->visible ? Qt::Checked : Qt::Unchecked);
            if (idOf(*c) == currentId_) curveTree_->setCurrentItem(item);
        }
    }
    updating_ = false;
}

// ---------------------------------------------------------------------------
// Widget callbacks
// ---------------------------------------------------------------------------

QTreeWidgetItem *MonitorChartDemoWindow::treeItemFor(const QString &curveId) const
{
    for (int i = 0; i < curveTree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem *group = curveTree_->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j) {
            if (group->child(j)->data(0, Qt::UserRole).toString() == curveId) return group->child(j);
        }
    }
    return nullptr;
}

int MonitorChartDemoWindow::indexOfCurve(const QString &curveId) const
{
    for (int i = 0; i < curves_.size(); ++i) {
        if (idOf(curves_.at(i)) == curveId) return i;
    }
    return -1;
}

MonitorChartView *MonitorChartDemoWindow::chartFor(const QString &curveId) const
{
    return charts_.value(curveGroup_.value(curveId));
}

void MonitorChartDemoWindow::syncTreeCheckState(const QString &curveId, bool visible)
{
    QTreeWidgetItem *item = treeItemFor(curveId);
    if (!item) return;
    updating_ = true;
    item->setCheckState(0, visible ? Qt::Checked : Qt::Unchecked);
    updating_ = false;
}

void MonitorChartDemoWindow::onFollowClicked()
{
    if (followButton_->isChecked()) {
        if (sourcePath_.isEmpty()) {
            followButton_->setChecked(false);
            liveLabel_->setText(QStringLiteral("先导入一个历史文件"));
            return;
        }
        QString error;
        if (!startFollowing(sourcePath_, &error)) {
            followButton_->setChecked(false);
            liveLabel_->setText(QStringLiteral("无法跟随：%1").arg(error));
            return;
        }
        followButton_->setText(QStringLiteral("停止跟随"));
    } else {
        stopFollowing();
        followButton_->setText(QStringLiteral("跟随实时文件"));
    }
}

void MonitorChartDemoWindow::onFollowTicked()
{
    if (!isFollowing())
        return;
    QString error;
    const bool ok = pollLiveOnce(nullptr, &error);
    if (!ok) {
        liveLabel_->setText(QStringLiteral("轮询失败：%1").arg(error));
        stopFollowing();
        followButton_->setChecked(false);
        followButton_->setText(QStringLiteral("跟随实时文件"));
    }
}

bool MonitorChartDemoWindow::startFollowing(const QString &path, QString *error)
{
    if (error)
        error->clear();
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("没有可跟随的文件。");
        return false;
    }

    // Follow from the first byte rather than from the current end: the demo is
    // usually pointed at a finished history and then hands the same path to a
    // running solver, and reading it from the start is what makes the first
    // poll reproduce the imported drawing instead of appending to it twice.
    auto source = std::unique_ptr<MonitorLiveSource>(new MonitorFileTailSource(path));
    if (!source->start(error))
        return false;

    liveSource_ = std::move(source);
    liveAccumulator_.setDatasetId(liveSource_->sourceId());
    liveAccumulator_.reset();
    livePolls_ = 0;
    liveAppendedRows_ = 0;
    followTimer_->start();
    refreshLiveLabel();
    return true;
}

void MonitorChartDemoWindow::stopFollowing()
{
    if (followTimer_)
        followTimer_->stop();
    if (liveSource_) {
        liveSource_->stop();
        liveSource_.reset();
    }
    refreshLiveLabel();
}

bool MonitorChartDemoWindow::isFollowing() const
{
    return liveSource_ && liveSource_->isRunning();
}

QString MonitorChartDemoWindow::liveStatusText() const
{
    return liveLabel_ ? liveLabel_->text() : QString();
}

void MonitorChartDemoWindow::refreshLiveLabel()
{
    if (!liveLabel_)
        return;
    if (!isFollowing()) {
        liveLabel_->setText(QStringLiteral("未跟随实时文件"));
        return;
    }
    liveLabel_->setText(QStringLiteral("跟随中：%1 行 · 已追加 %2 行 · 轮询 %3 次")
                            .arg(liveAccumulator_.rowCount())
                            .arg(liveAppendedRows_)
                            .arg(livePolls_));
}

bool MonitorChartDemoWindow::pollLiveOnce(int *appendedRows, QString *error)
{
    if (appendedRows)
        *appendedRows = 0;
    if (error)
        error->clear();
    if (!liveSource_) {
        if (error)
            *error = QStringLiteral("没有正在跟随的实时源。");
        return false;
    }

    MonitorUpdate update;
    if (!liveSource_->poll(&update, error))
        return false;

    ++livePolls_;
    const bool ok = applyLiveUpdate(update, appendedRows, error);
    refreshLiveLabel();
    return ok;
}

bool MonitorChartDemoWindow::applyLiveUpdate(const MonitorUpdate &update, int *appendedRows, QString *error)
{
    if (appendedRows)
        *appendedRows = 0;
    if (error)
        error->clear();

    // A delivery that carries nothing but a restart still has to be applied.
    if (update.isEmpty() && update.warnings.isEmpty() && !update.restarted)
        return true;

    // Measured against the model, not the accumulator: following starts with an
    // empty accumulator while the window may already be showing rows read from
    // disk, and those rows must not be reported as freshly appended.
    const int rowsBefore = model_.log().iterationStep.size();
    if (!liveAccumulator_.append(update, error))
        return false;

    const MonitorLog &log = liveAccumulator_.log();
    if (log.iterationStep.isEmpty())
        return true;

    // Structural change: the column set moved, or the run restarted. Either way
    // the curves on screen no longer describe the accumulated log, and the only
    // honest move is to replace them.
    QStringList liveKeys;
    for (const MonitorColumn &column : log.columns) {
        if (column.isAxis)
            continue;
        liveKeys.append(column.key);
    }
    const bool restarted = update.restarted || liveAccumulator_.rowCount() < rowsBefore;
    const bool columnsMoved = !sameKeys(liveKeys, curveKeys_);

    if (restarted || columnsMoved || curves_.isEmpty()) {
        QString modelError;
        if (!model_.setLog(log, &modelError)) {
            if (error)
                *error = modelError;
            return false;
        }
        buildCurves();
        rebuildCharts();
        populateTree();
        refreshStatus();
        curveKeys_ = liveKeys;
        return true;
    }

    // Nothing structural moved, so the rows go straight onto the curves. This is
    // the case a solver produces a thousand times a run, and it must not rebuild
    // the plot: a rebuild would drop the zoom, the selection and any style the
    // user had just edited.
    QString modelError;
    if (!model_.setLog(log, &modelError)) {
        if (error)
            *error = modelError;
        return false;
    }

    const int added = log.iterationStep.size() - rowsBefore;
    int appended = 0;
    for (int i = 0; i < curves_.size(); ++i) {
        MonitorCurve &curve = curves_[i];
        const QVector<QPointF> points = model_.points(curve.key, axis_);
        if (points.size() <= curve.points.size())
            continue;
        const int from = curve.points.size();
        QVector<QPointF> tail;
        tail.reserve(points.size() - from);
        for (int p = from; p < points.size(); ++p)
            tail.append(points.at(p));

        MonitorChartView *chart = chartFor(idOf(curve));
        if (chart && chart->appendPoints(idOf(curve), tail)) {
            curve.points = points;
            ++appended;
        } else {
            // The chart cannot append to this curve (it is hidden, or its items
            // are gone), so take the data and let the next rebuild draw it.
            curve.points = points;
        }
    }

    if (appendedRows)
        *appendedRows = added;
    liveAppendedRows_ += added;
    refreshStatus();
    return true;
}

void MonitorChartDemoWindow::refreshStatus()
{
    for (const QString &chartKey : groupOrder_) {
        MonitorChartView *chart = charts_.value(chartKey);
        QLabel *status = chartStatus_.value(chartKey);
        if (chart && status) status->setText(chart->statusText());
    }
}

void MonitorChartDemoWindow::onImportClicked()
{
    const QString initial = sourcePath_.isEmpty() ? QString() : QFileInfo(sourcePath_).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入历史文件"), initial,
                                                      QStringLiteral("求解器历史 (*.csv *.dat *.log);;所有文件 (*.*)"));
    if (path.isEmpty()) return;
    QString error;
    if (!loadFile(path, &error)) {
        fileLabel_->setText(QStringLiteral("读取失败：%1").arg(error));
    }
}

void MonitorChartDemoWindow::onClearClicked()
{
    curves_.clear();
    sourcePath_.clear();
    currentId_.clear();
    fileLabel_->setText(QStringLiteral("尚未加载数据"));
    updating_ = true;
    curveTree_->clear();
    updating_ = false;
    rebuildCharts();
    loadStylePanel(QString());
}

void MonitorChartDemoWindow::onExportClicked()
{
    if (charts_.isEmpty()) return;
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 PNG"), QStringLiteral("chart.png"),
                                                      QStringLiteral("PNG (*.png)"));
    if (path.isEmpty()) return;
    const QFileInfo info(path);
    int index = 1;
    for (const QString &chartKey : groupOrder_) {
        MonitorChartView *chart = charts_.value(chartKey);
        if (!chart) continue;
        const QString target = QStringLiteral("%1/%2_%3.png")
                                   .arg(info.absolutePath())
                                   .arg(info.completeBaseName())
                                   .arg(index++);
        chart->savePng(target);
    }
}

void MonitorChartDemoWindow::onAxisChanged(int index)
{
    if (updating_ || index < 0) return;
    const MonitorAxis axis = static_cast<MonitorAxis>(axisCombo_->itemData(index).toInt());
    if (!setAxis(axis)) return;
}

void MonitorChartDemoWindow::onLogToggled(bool on)
{
    if (updating_) return;
    setResidualLogarithmic(on);
}

void MonitorChartDemoWindow::onOverlayToggled(bool on)
{
    if (updating_) return;
    setOverlayMode(on);
}

void MonitorChartDemoWindow::onIntervalChanged(int value)
{
    if (updating_) return;
    setSampleInterval(value);
}

void MonitorChartDemoWindow::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (updating_ || column != 0 || !item || item->parent() == nullptr) return;
    const QString id = item->data(0, Qt::UserRole).toString();
    if (id.isEmpty()) return;
    setCurveVisible(id, item->checkState(0) == Qt::Checked);
}

void MonitorChartDemoWindow::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *)
{
    if (updating_ || !current || current->parent() == nullptr) return;
    const QString id = current->data(0, Qt::UserRole).toString();
    if (id.isEmpty()) return;
    selectCurve(id);
}

void MonitorChartDemoWindow::onSelectionFromChart(const QString &curveId)
{
    selectCurve(curveId);
}

void MonitorChartDemoWindow::onColorClicked()
{
    if (currentId_.isEmpty()) return;
    const QColor picked = QColorDialog::getColor(currentColor_, this, QStringLiteral("选择曲线颜色"));
    if (!picked.isValid()) return;
    currentColor_ = picked;
    onStyleEdited();
}

// The panel is loaded from one curve, but an edit reaches whatever the scope
// combo says. The difference between what the panel now shows and what that
// reference curve holds is exactly the user's intent, so only those fields are
// pushed -- a group-wide "point size" change must not repaint the group one
// colour, and colour is how the members of a group are told apart.
void MonitorChartDemoWindow::onStyleEdited()
{
    if (updating_ || currentId_.isEmpty()) return;
    MonitorChartView *chart = chartFor(currentId_);
    if (!chart) return;
    const MonitorCurveStyle base = chart->curveStyle(currentId_);
    const MonitorChartView::StylePatch patch = patchFromPanel(base);
    applyPatch(patch);
}

// ---------------------------------------------------------------------------
// Programmatic control
// ---------------------------------------------------------------------------

QStringList MonitorChartDemoWindow::selectedCurveIds() const
{
    QStringList ids;
    const QList<QTreeWidgetItem *> items = curveTree_->selectedItems();
    for (QTreeWidgetItem *item : items) {
        const QString id = item->data(0, Qt::UserRole).toString();
        if (!id.isEmpty() && !ids.contains(id)) ids.append(id);
    }
    // Nothing ticked in the tree: fall back to the curve the panel is showing, so
    // "selected curve" still means something when the user edits from the chart.
    if (ids.isEmpty() && !currentId_.isEmpty()) ids.append(currentId_);
    return ids;
}

void MonitorChartDemoWindow::setSelectedCurves(const QStringList &curveIds)
{
    curveTree_->clearSelection();
    for (const QString &id : curveIds) {
        if (QTreeWidgetItem *item = treeItemFor(id)) item->setSelected(true);
    }
}

// Interval is a property of the view rather than of the data, so it is pushed
// through the same scoped path as every other style field: the chart has to
// rebuild the items it holds before a new stride means anything on screen.
void MonitorChartDemoWindow::setSampleInterval(int interval)
{
    MonitorChartView::StylePatch patch;
    patch.hasSampleInterval = true;
    patch.sampleInterval = qMax(1, interval);
    applyPatch(patch);
}

bool MonitorChartDemoWindow::setCurveVisible(const QString &curveId, bool visible)
{
    const int index = indexOfCurve(curveId);
    if (index < 0) return false;
    if (curves_.at(index).visible == visible) {
        syncTreeCheckState(curveId, visible);
        return true;
    }
    curves_[index].visible = visible;
    applyCurves();
    syncTreeCheckState(curveId, visible);
    return true;
}

// Only the fields the user actually moved, measured against the curve the panel
// is showing. A group edit that stamped every field would repaint three curves
// one colour precisely where colour is the thing telling them apart.
MonitorChartView::StylePatch
MonitorChartDemoWindow::patchFromPanel(const MonitorCurveStyle &base) const
{
    MonitorChartView::StylePatch patch;
    const MonitorCurveStyle panel = styleFromPanel();

    if (panel.color != base.color) {
        patch.hasColor = true;
        patch.color = panel.color;
    }
    if (panel.lineStyle != base.lineStyle) {
        patch.hasLineStyle = true;
        patch.lineStyle = panel.lineStyle;
    }
    if (!qFuzzyCompare(panel.pointSize + 1.0, base.pointSize + 1.0)) {
        patch.hasPointSize = true;
        patch.pointSize = panel.pointSize;
    }
    if (panel.pointShape != base.pointShape) {
        patch.hasPointShape = true;
        patch.pointShape = panel.pointShape;
        patch.hasPointContent = true;
        patch.pointContent = panel.pointContent;
    }
    if (panel.interpolation != base.interpolation) {
        patch.hasInterpolation = true;
        patch.interpolation = panel.interpolation;
    }
    if (panel.fit.method != base.fit.method || panel.fit.degree != base.fit.degree
        || panel.fit.window != base.fit.window || panel.fit.lineStyle != base.fit.lineStyle) {
        patch.hasFit = true;
        patch.fit = panel.fit;
    }
    // A caption is only pushed when the user typed one that differs: the field
    // starts out holding the derived caption, and sending that back would turn a
    // derived caption into a frozen one.
    if (!panel.label.isEmpty() && panel.label != base.label) {
        patch.hasLabel = true;
        patch.label = panel.label;
    }
    return patch;
}

void MonitorChartDemoWindow::applyPatch(const MonitorChartView::StylePatch &patch)
{
    const auto scope = static_cast<MonitorChartView::EditScope>(scopeCombo_->currentData().toInt());
    QStringList ids;
    QString groupKey;
    if (scope == MonitorChartView::EditScope::Curves) {
        ids = selectedCurveIds();
        if (ids.isEmpty() && !currentId_.isEmpty())
            ids.append(currentId_);
    } else if (scope == MonitorChartView::EditScope::Group) {
        if (!currentId_.isEmpty())
            groupKey = curveGroup_.value(currentId_);
    }

    int total = 0;
    for (const QString &chartKey : groupOrder_) {
        MonitorChartView *chart = charts_.value(chartKey);
        if (!chart) continue;
        total += chart->applyStyle(scope, patch, groupKey, ids);
    }
    if (total == 0) return;

    // Mirror what the charts now hold back into the demo's own list: the charts
    // are rebuilt on every axis change and every layout switch, and they are only
    // handed a fresh copy of this list at that point.
    QStringList touched;
    if (scope == MonitorChartView::EditScope::Curves) {
        touched = ids;
    } else {
        for (const MonitorCurve &curve : curves_) {
            const QString id = idOf(curve);
            if (scope == MonitorChartView::EditScope::Group && curveGroup_.value(id) != groupKey)
                continue;
            touched.append(id);
        }
    }
    syncStyleFromCharts(touched);
    refreshStatus();
    for (const QString &chartKey : groupOrder_) {
        if (MonitorChartView *chart = charts_.value(chartKey)) {
            if (QLabel *status = chartStatus_.value(chartKey))
                status->setText(chart->statusText());
        }
    }
}

void MonitorChartDemoWindow::syncStyleFromCharts(const QStringList &curveIds)
{
    for (const QString &id : curveIds) {
        MonitorChartView *chart = chartFor(id);
        if (!chart) continue;
        const int index = indexOfCurve(id);
        if (index < 0) continue;
        const MonitorCurveStyle style = chart->curveStyle(id);
        MonitorCurve &curve = curves_[index];
        curve.color = style.color;
        curve.lineStyle = style.lineStyle;
        curve.pointSize = style.pointSize;
        curve.pointShape = style.pointShape;
        curve.interpolation = style.interpolation;
        curve.fit = style.fit;
        curve.sampleInterval = style.sampleInterval;
        if (!style.label.isEmpty()) curve.label = style.label;
    }
}

bool MonitorChartDemoWindow::applyStyle(const QString &curveId, const MonitorCurveStyle &style)
{
    const int index = indexOfCurve(curveId);
    if (index < 0) return false;
    MonitorCurve &curve = curves_[index];
    curve.color = style.color;
    curve.lineStyle = style.lineStyle;
    curve.pointSize = style.pointSize;
    curve.pointShape = style.pointShape;
    curve.interpolation = style.interpolation;
    // Kept apart from interpolation on purpose: a series can be drawn as bare
    // samples with a fitted trend and no joining stroke at all.
    curve.fit = style.fit;
    // Needed by Path, Pixmap, Graphic and Svg; the other shapes ignore it.
    curve.pointContent = style.pointContent;
    if (!style.label.isEmpty()) curve.label = style.label;

    MonitorChartView *chart = chartFor(curveId);
    if (chart) {
        // Deliberately the run-time edit path: the drawing is not rebuilt, so
        // selection and the other curves stay exactly as they were.
        MonitorCurveStyle live = style;
        live.label = curve.label;
        chart->setCurveStyle(curveId, live);
        QLabel *status = chartStatus_.value(curveGroup_.value(curveId));
        if (status) status->setText(chart->statusText());
    }
    return chart != nullptr;
}

bool MonitorChartDemoWindow::applyLabel(const QString &curveId, const QString &label)
{
    const int index = indexOfCurve(curveId);
    if (index < 0) return false;
    curves_[index].label = label;
    MonitorChartView *chart = chartFor(curveId);
    if (chart) chart->setCurveLabel(curveId, label);
    return chart != nullptr;
}

void MonitorChartDemoWindow::selectCurve(const QString &curveId)
{
    if (curveId == currentId_) return;
    currentId_ = curveId;
    for (const QString &chartKey : groupOrder_) {
        MonitorChartView *chart = charts_.value(chartKey);
        if (!chart) continue;
        chart->setSelectedCurve(chart->curveIds().contains(curveId) ? curveId : QString());
    }
    QTreeWidgetItem *item = treeItemFor(curveId);
    if (item && curveTree_->currentItem() != item) {
        updating_ = true;
        curveTree_->setCurrentItem(item);
        updating_ = false;
    }
    loadStylePanel(curveId);
}

bool MonitorChartDemoWindow::setAxis(MonitorAxis axis)
{
    if (axis_ == axis && !curves_.isEmpty()) return true;
    QString reason;
    if (!model_.axisAvailable(axis, &reason)) {
        fileLabel_->setText(QStringLiteral("%1：%2").arg(model_.axisLabel(axis), reason));
        return false;
    }
    axis_ = axis;
    buildCurves();
    applyCurves();
    updating_ = true;
    for (int i = 0; i < axisCombo_->count(); ++i) {
        if (static_cast<MonitorAxis>(axisCombo_->itemData(i).toInt()) == axis) {
            axisCombo_->setCurrentIndex(i);
            break;
        }
    }
    updating_ = false;
    populateTree();
    refreshStatus();
    return true;
}

bool MonitorChartDemoWindow::setResidualLogarithmic(bool on)
{
    residualLogarithmic_ = on;
    updating_ = true;
    logCheck_->setChecked(on);
    updating_ = false;
    for (MonitorCurve &c : curves_) {
        if (!model_.isResidual(c.key)) continue;
        c.logarithmic = on && model_.suggestedScale(c.key) == MonitorScale::Logarithmic;
    }
    applyCurves();
    refreshStatus();
    return true;
}

void MonitorChartDemoWindow::setPointBudget(int budget)
{
    // The view only records the value; the samples a plot holds are fixed when
    // setCurves() builds it. Without the rebuild below a caller would set the
    // budget and see nothing change on screen.
    pointBudget_ = budget;
    applyCurves();
}

void MonitorChartDemoWindow::setOverlayMode(bool on)
{
    if (overlayMode_ == on) return;
    overlayMode_ = on;
    updating_ = true;
    overlayCheck_->setChecked(on);
    updating_ = false;
    // Toggling the layout rebuilds every chart, which drops the selection. The
    // curve the user was looking at has to survive the switch, so re-apply it
    // after the tree has been repopulated.
    const QString keep = currentId_;
    rebuildCharts();
    populateTree();
    refreshStatus();
    currentId_.clear();
    if (!keep.isEmpty()) selectCurve(keep);
    loadStylePanel(keep);
}

// ---------------------------------------------------------------------------
// Style panel
// ---------------------------------------------------------------------------

void MonitorChartDemoWindow::loadStylePanel(const QString &curveId)
{
    const int index = indexOfCurve(curveId);
    updating_ = true;
    if (index < 0) {
        selectionLabel_->setText(QStringLiteral("未选中曲线"));
        labelEdit_->clear();
        labelEdit_->setEnabled(false);
        colorButton_->setEnabled(false);
        lineStyleCombo_->setEnabled(false);
        shapeCombo_->setEnabled(false);
        pointSizeSpin_->setEnabled(false);
        interpolationCombo_->setEnabled(false);
        fitCombo_->setEnabled(false);
        fitDegreeSpin_->setEnabled(false);
        fitWindowSpin_->setEnabled(false);
        fitLineStyleCombo_->setEnabled(false);
        updating_ = false;
        return;
    }
    const MonitorCurve &curve = curves_.at(index);
    selectionLabel_->setText(QStringLiteral("当前曲线：%1").arg(curve.key));
    labelEdit_->setEnabled(true);
    colorButton_->setEnabled(true);
    lineStyleCombo_->setEnabled(true);
    shapeCombo_->setEnabled(true);
    pointSizeSpin_->setEnabled(true);
    interpolationCombo_->setEnabled(true);
    fitCombo_->setEnabled(true);
    fitDegreeSpin_->setEnabled(true);
    fitWindowSpin_->setEnabled(true);
    fitLineStyleCombo_->setEnabled(true);

    labelEdit_->setText(curve.label);
    currentColor_ = curve.color;
    colorButton_->setStyleSheet(QStringLiteral("background-color: %1;").arg(curve.color.name()));
    lineStyleCombo_->setCurrentIndex(lineStyleCombo_->findData(static_cast<int>(curve.lineStyle)));
    shapeCombo_->setCurrentIndex(shapeCombo_->findData(static_cast<int>(curve.pointShape)));
    pointSizeSpin_->setValue(curve.pointSize);
    interpolationCombo_->setCurrentIndex(interpolationCombo_->findData(static_cast<int>(curve.interpolation)));
    fitCombo_->setCurrentIndex(qMax(0, fitCombo_->findData(static_cast<int>(curve.fit.method))));
    fitDegreeSpin_->setValue(curve.fit.degree);
    fitWindowSpin_->setValue(curve.fit.window);
    fitLineStyleCombo_->setCurrentIndex(qMax(0, fitLineStyleCombo_->findData(static_cast<int>(curve.fit.lineStyle))));
    updating_ = false;
}

MonitorCurveStyle MonitorChartDemoWindow::styleFromPanel() const
{
    MonitorCurveStyle style;
    style.label = labelEdit_->text();
    style.color = currentColor_;
    style.lineStyle = static_cast<Qt::PenStyle>(lineStyleCombo_->currentData().toInt());
    style.pointShape = static_cast<MonitorPointShape>(shapeCombo_->currentData().toInt());
    style.pointSize = pointSizeSpin_->value();
    style.interpolation = static_cast<MonitorInterpolation>(interpolationCombo_->currentData().toInt());
    style.fit.method = static_cast<MonitorFitMethod>(fitCombo_->currentData().toInt());
    style.fit.degree = fitDegreeSpin_->value();
    style.fit.window = fitWindowSpin_->value();
    style.fit.lineStyle = static_cast<Qt::PenStyle>(fitLineStyleCombo_->currentData().toInt());
    // Only the four content-driven shapes need it, but passing it unconditionally
    // keeps the style a faithful round trip of whatever the chart holds.
    style.pointContent = demoPointContent_;
    return style;
}

void MonitorChartDemoWindow::addShapeItem(const QString &text, MonitorPointShape shape)
{
    shapeCombo_->addItem(text, static_cast<int>(shape));
}

MonitorPointContent MonitorChartDemoWindow::buildDemoPointContent()
{
    MonitorPointContent content;

    // A five-pointed star outline, in a 20x20 box.
    QPainterPath path;
    const QPointF star[] = {QPointF(10, 0),  QPointF(13, 7),  QPointF(20, 7),  QPointF(14, 12),
                            QPointF(17, 20), QPointF(10, 15), QPointF(3, 20),  QPointF(6, 12),
                            QPointF(0, 7),   QPointF(7, 7)};
    path.moveTo(star[0]);
    for (int i = 1; i < 10; ++i) path.lineTo(star[i]);
    path.closeSubpath();
    content.path = path;

    // A filled circle with a dark rim, so a pixmap marker is distinguishable
    // from the plain Ellipse shape even at a glance.
    QImage image(16, 16, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(30, 30, 30), 1.5));
        painter.setBrush(QColor(60, 170, 90));
        painter.drawEllipse(QRectF(1.5, 1.5, 13.0, 13.0));
    }
    content.image = image;

    content.svg = QByteArray(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\">"
        "<path d=\"M5 0 L6 3.5 L10 3.5 L6.8 5.8 L8 10 L5 7.4 L2 10 L3.2 5.8 L0 3.5 L4 3.5 Z\""
        " fill=\"#c0392b\"/></svg>");
    return content;
}

int MonitorChartDemoWindow::visibleChartCount() const
{
    int visible = 0;
    for (const QString &chartKey : groupOrder_) {
        if (visibleGroups_.contains(chartKey)) ++visible;
    }
    return visible;
}

QString MonitorChartDemoWindow::report() const
{
    QStringList lines;
    lines << QStringLiteral("file=%1").arg(QFileInfo(sourcePath_).fileName());
    lines << QStringLiteral("rows=%1 columns=%2").arg(model_.rowCount()).arg(model_.seriesColumns().size());
    lines << QStringLiteral("curves=%1 charts=%2").arg(curves_.size()).arg(charts_.size());
    for (const QString &chartKey : groupOrder_) {
        MonitorChartView *chart = charts_.value(chartKey);
        if (!chart) continue;
        lines << QStringLiteral("  %1: axes=%2 marks=%3 joins=%4 status=%5")
                     .arg(chartKey)
                     .arg(chart->axisCount())
                     .arg(chart->pointSeriesCount())
                     .arg(chart->lineSeriesCount())
                     .arg(chart->statusText());
    }
    return lines.join(QLatin1Char('\n'));
}

// ---------------------------------------------------------------------------
// Self test
// ---------------------------------------------------------------------------

QString MonitorChartDemoWindow::selfTest(const QString &shotDirectory)
{
    QStringList log;
    if (!shotDirectory.isEmpty()) QDir().mkpath(shotDirectory);

    const auto shoot = [&](const QString &prefix) {
        if (shotDirectory.isEmpty()) return;
        int index = 1;
        for (const QString &chartKey : groupOrder_) {
            MonitorChartView *chart = charts_.value(chartKey);
            if (!chart) continue;
            const QString path = QStringLiteral("%1/%2_%3_%4.png")
                                     .arg(shotDirectory)
                                     .arg(prefix)
                                     .arg(index++)
                                     .arg(chartKey);
            chart->savePng(path);
        }
        if (!isVisible()) return;
        for (int i = 0; i < 6; ++i) QCoreApplication::processEvents();
        grab().save(QStringLiteral("%1/%2_window.png").arg(shotDirectory).arg(prefix));
    };

    log << QStringLiteral("STEP1_LOADED charts=%1 curves=%2").arg(charts_.size()).arg(curves_.size());
    for (const QString &chartKey : groupOrder_) {
        MonitorChartView *chart = charts_.value(chartKey);
        if (!chart) continue;
        log << QStringLiteral("  %1 axes=%2 marks=%3 points=%4 status=%5")
                   .arg(chartKey)
                   .arg(chart->axisCount())
                   .arg(chart->pointSeriesCount())
                   .arg(chart->drawnPointCount())
                   .arg(chart->statusText());
    }
    shoot(QStringLiteral("demo_01_loaded"));

    // Hide every coefficient so only the residual chart keeps content.
    for (const MonitorCurve &c : curves_) {
        if (model_.isResidual(c.key)) continue;
        setCurveVisible(idOf(c), false);
    }
    log << QStringLiteral("STEP2_RESIDUALS_ONLY charts=%1 visible=%2")
               .arg(charts_.size())
               .arg(visibleChartCount());
    shoot(QStringLiteral("demo_02_residuals"));

    for (const MonitorCurve &c : curves_) {
        if (model_.isResidual(c.key)) continue;
        setCurveVisible(idOf(c), true);
    }

    // Restyle CD: a thicker red square marker joined by a spline.
    const QString cdId = MonitorChartView::makeCurveId(model_.datasetId(), QStringLiteral("CD"));
    MonitorCurveStyle style;
    style.label = QStringLiteral("CD（已改样式）");
    style.color = QColor(QStringLiteral("#d62728"));
    style.lineStyle = Qt::SolidLine;
    style.pointSize = 9.0;
    style.pointShape = MonitorPointShape::Square;
    style.interpolation = MonitorInterpolation::Spline;
    const int buildsBefore = chartFor(cdId) ? chartFor(cdId)->chartBuildCount() : -1;
    applyStyle(cdId, style);
    selectCurve(cdId);
    const int buildsAfter = chartFor(cdId) ? chartFor(cdId)->chartBuildCount() : -1;
    // The panel must show what the curve actually holds. It once kept a stale
    // shape because the build linked an older object file, and the mismatch was
    // only visible by looking at a screenshot.
    const bool panelMatches = shapeCombo_->currentData().toInt() == static_cast<int>(MonitorPointShape::Square);
    log << QStringLiteral("STEP3_RESTYLED builds_before=%1 builds_after=%2 panel_matches=%3 label=%4")
               .arg(buildsBefore)
               .arg(buildsAfter)
               .arg(panelMatches ? 1 : 0)
               .arg(chartFor(cdId) ? chartFor(cdId)->legendLabels().join(QLatin1Char(',')) : QString());
    shoot(QStringLiteral("demo_03_restyled"));

    // Decimate every chart.
    setPointBudget(20);
    log << QStringLiteral("STEP4_BUDGET20 %1").arg(chartFor(cdId) ? chartFor(cdId)->statusText() : QString());
    shoot(QStringLiteral("demo_04_budget"));

    setPointBudget(0);

    // Overlay mode: two magnitudes at most, the rest reported as dropped.
    setOverlayMode(true);
    log << QStringLiteral("STEP5_OVERLAY charts=%1 selected=%2")
               .arg(charts_.size())
               .arg(charts_.contains(QStringLiteral("overlay")) ? charts_.value(QStringLiteral("overlay"))->selectedCurve() : QString());
    shoot(QStringLiteral("demo_05_overlay"));

    setOverlayMode(false);
    log << QStringLiteral("STEP6_SPLIT charts=%1 visible=%2")
               .arg(charts_.size())
               .arg(visibleChartCount());
    shoot(QStringLiteral("demo_06_split"));

    // Walk the four marker shapes that need caller-supplied artwork. Going
    // through applyStyle() is what the shape combo does, so this proves the
    // combo can reach every marker rather than only the geometric ones.
    {
        const QStringList shapeNames = {QStringLiteral("Path"), QStringLiteral("Pixmap"),
                                        QStringLiteral("Graphic"), QStringLiteral("Svg")};
        const MonitorPointShape shapeValues[] = {MonitorPointShape::Path, MonitorPointShape::Pixmap,
                                                 MonitorPointShape::Graphic, MonitorPointShape::Svg};
        QStringList drawn;
        for (int i = 0; i < shapeNames.size(); ++i) {
            MonitorCurveStyle style;
            style.label = QStringLiteral("CD（%1 标记）").arg(shapeNames.at(i));
            style.color = QColor(QStringLiteral("#d62728"));
            style.lineStyle = Qt::SolidLine;
            style.pointSize = 12.0;
            style.pointShape = shapeValues[i];
            style.pointContent = demoPointContent_;
            style.interpolation = MonitorInterpolation::None;
            applyStyle(cdId, style);
            const MonitorCurveStyle round = chartFor(cdId) ? chartFor(cdId)->curveStyle(cdId) : MonitorCurveStyle();
            if (round.pointShape == shapeValues[i]) drawn << shapeNames.at(i);
            shoot(QStringLiteral("demo_07_shape_%1").arg(shapeNames.at(i)));
        }
        log << QStringLiteral("STEP7_MARKER_CONTENT shapes=%1").arg(drawn.join(QLatin1Char(',')));
    }

    // Walk the four fit methods on a real residual column. Unlike the joining
    // stroke, a fit must smooth a noisy convergence trace, so the assertion is
    // that the chart really holds a trend and that R2 is reported for it.
    {
        const QStringList fitNames = {QStringLiteral("Linear"), QStringLiteral("Polynomial"),
                                      QStringLiteral("MovingAverage")};
        const MonitorFitMethod fitValues[] = {MonitorFitMethod::Linear, MonitorFitMethod::Polynomial,
                                              MonitorFitMethod::MovingAverage};
        QStringList fitted;
        for (int i = 0; i < fitNames.size(); ++i) {
            MonitorCurveStyle style;
            style.color = QColor(QStringLiteral("#2ca02c"));
            style.lineStyle = Qt::SolidLine;
            style.pointSize = 5.0;
            style.pointShape = MonitorPointShape::Circle;
            style.interpolation = MonitorInterpolation::None;
            style.fit.method = fitValues[i];
            style.fit.degree = 3;
            style.fit.window = 9;
            applyStyle(cdId, style);
            MonitorChartView *chart = chartFor(cdId);
            const MonitorFitResult result = chart ? chart->fitResult(cdId) : MonitorFitResult();
            const bool drawn = chart && chart->fitSeriesCount() == 1 && result.isValid();
            if (drawn) {
                fitted << QStringLiteral("%1(R2=%2)").arg(fitNames.at(i)).arg(result.rSquared, 0, 'f', 3);
            }
            shoot(QStringLiteral("demo_08_fit_%1").arg(fitNames.at(i)));
        }
        log << QStringLiteral("STEP8_FIT fits=%1").arg(fitted.join(QLatin1Char(',')));

        // Turning the fit off must remove the trend again rather than leaving a
        // stale stroke attached to the plot.
        MonitorCurveStyle plain;
        plain.color = QColor(QStringLiteral("#2ca02c"));
        plain.lineStyle = Qt::SolidLine;
        plain.pointSize = 5.0;
        plain.pointShape = MonitorPointShape::Circle;
        plain.interpolation = MonitorInterpolation::None;
        plain.fit.method = MonitorFitMethod::None;
        applyStyle(cdId, plain);
        MonitorChartView *chart = chartFor(cdId);
        const int left = chart ? chart->fitSeriesCount() : -1;
        log << QStringLiteral("STEP8_FIT_OFF series=%1").arg(left);
        shoot(QStringLiteral("demo_08_fit_off"));
    }

    // Walk the scope combo the way a user would: pick curves in the tree, then
    // change the interval. The assertion has to be on what actually reaches the
    // canvas (drawnSampleCount), not on the value the spin box now shows.
    {
        const QString clId = MonitorChartView::makeCurveId(model_.datasetId(), QStringLiteral("CL"));
        selectCurve(cdId);
        setSelectedCurves(QStringList() << cdId << clId);
        scopeCombo_->setCurrentIndex(scopeCombo_->findData(static_cast<int>(MonitorChartView::EditScope::Curves)));
        const int fullBefore = chartFor(cdId) ? chartFor(cdId)->drawnSampleCount(cdId) : -1;
        setSampleInterval(4);
        MonitorChartView *chart = chartFor(cdId);
        const int pickedAfter = chart ? chart->drawnSampleCount(cdId) : -1;
        // CL lives in a different chart, so ask that chart -- asking the CD chart
        // for a curve it does not hold would report 0 and look like a bug.
        MonitorChartView *clChart = chartFor(clId);
        const int untouched = clChart ? clChart->drawnSampleCount(clId) : -1;
        log << QStringLiteral("STEP9_SCOPE_CURVES before=%1 after=%2 second=%3")
                   .arg(fullBefore).arg(pickedAfter).arg(untouched);
        shoot(QStringLiteral("demo_09_scope_curves"));

        // Group scope: the residual group drops to every second sample while the
        // coefficient group keeps its own stride.
        selectCurve(QStringLiteral("su2_history_steady|rms[Rho]"));
        scopeCombo_->setCurrentIndex(scopeCombo_->findData(static_cast<int>(MonitorChartView::EditScope::Group)));
        setSampleInterval(2);
        MonitorChartView *residualChart = chartFor(QStringLiteral("su2_history_steady|rms[Rho]"));
        const int residual = residualChart ? residualChart->drawnSampleCount(QStringLiteral("su2_history_steady|rms[Rho]")) : -1;
        const int coefficient = chartFor(cdId) ? chartFor(cdId)->drawnSampleCount(cdId) : -1;
        log << QStringLiteral("STEP9_SCOPE_GROUP residual=%1 coefficient=%2").arg(residual).arg(coefficient);
        shoot(QStringLiteral("demo_09_scope_group"));

        // Everything back to every sample, so the closing screenshots are not
        // decimated by a test step.
        scopeCombo_->setCurrentIndex(scopeCombo_->findData(static_cast<int>(MonitorChartView::EditScope::All)));
        setSampleInterval(1);
        log << QStringLiteral("STEP9_SCOPE_ALL restored=%1")
                   .arg(chartFor(cdId) ? chartFor(cdId)->drawnSampleCount(cdId) : -1);
    }

    // A solver writes one row per iteration. The window is expected to grow the
    // curves in place for that, and only rebuild when the column set changes --
    // a rebuild would drop the zoom, the selection and the styles just edited.
    {
        QTemporaryDir temp;
        const QString livePath = QStringLiteral("%1/live_history.csv").arg(temp.path());

        QByteArray whole;
        {
            QFile in(sourcePath_);
            if (in.open(QIODevice::ReadOnly))
                whole = in.readAll();
        }
        QList<QByteArray> lines = whole.split('\n');
        while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
            lines.removeLast();

        const auto rewrite = [&](int dataRows) {
            QFile out(livePath);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
                return false;
            QByteArray bytes = lines.first();
            bytes += '\n';
            for (int i = 0; i < dataRows && i + 1 < lines.size(); ++i) {
                bytes += lines.at(i + 1);
                bytes += '\n';
            }
            out.write(bytes);
            out.close();
            return true;
        };

        rewrite(40);
        QString liveError;
        QString loadError;
        const bool loaded = loadFile(livePath, &loadError) && rewrite(40);
        const QString liveId = MonitorChartView::makeCurveId(model_.datasetId(), QStringLiteral("CD"));
        MonitorChartView *liveChart = chartFor(liveId);
        const int beforeRows = liveChart ? liveChart->drawnSampleCount(liveId) : -1;
        const int beforeBuilds = liveChart ? liveChart->chartBuildCount() : -1;

        int appendedFirst = 0;
        int appendedSecond = 0;
        bool followed = loaded && startFollowing(livePath, &liveError);
        // Nothing new yet: the first poll replays the rows already on screen and
        // must not double count them.
        int appendedIdle = 0;
        if (followed)
            followed = pollLiveOnce(&appendedIdle, &liveError);

        if (followed) {
            rewrite(60);
            followed = pollLiveOnce(&appendedFirst, &liveError);
        }
        liveChart = chartFor(liveId);
        const int afterFirst = liveChart ? liveChart->drawnSampleCount(liveId) : -1;
        const int buildsAfterFirst = liveChart ? liveChart->chartBuildCount() : -1;

        if (followed) {
            rewrite(79);
            followed = pollLiveOnce(&appendedSecond, &liveError);
        }
        liveChart = chartFor(liveId);
        const int afterSecond = liveChart ? liveChart->drawnSampleCount(liveId) : -1;
        const int buildsAfterSecond = liveChart ? liveChart->chartBuildCount() : -1;
        const QString liveStatus = liveChart ? liveChart->statusText() : QString();

        log << QStringLiteral("STEP10_LIVE idle_appended=%1 first=%2 second=%3 rows=%4->%5->%6 builds=%7->%8->%9 incremental=%10")
                   .arg(appendedIdle)
                   .arg(appendedFirst)
                   .arg(appendedSecond)
                   .arg(beforeRows)
                   .arg(afterFirst)
                   .arg(afterSecond)
                   .arg(beforeBuilds)
                   .arg(buildsAfterFirst)
                   .arg(buildsAfterSecond)
                   .arg(liveStatus.contains(QStringLiteral("实时追加")) ? 1 : 0);
        shoot(QStringLiteral("demo_10_live"));

        stopFollowing();
        followButton_->setChecked(false);
        followButton_->setText(QStringLiteral("跟随实时文件"));
        QString restoreError;
        loadFile(sourcePath_, &restoreError);
    }

    log << QStringLiteral("SELFTEST_OK");
    return log.join(QLatin1Char('\n'));
}

} // namespace demo
} // namespace monitor
} // namespace aerosim
