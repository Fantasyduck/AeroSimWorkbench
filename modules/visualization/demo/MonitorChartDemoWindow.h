#pragma once

#include "MonitorChartView.h"
#include "MonitorLogAccumulator.h"
#include "MonitorSeriesModel.h"

#include <memory>

#include <QColor>
#include <QHash>
#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QWidget;

namespace aerosim {
namespace monitor {
namespace demo {

// A small runnable front end for the monitor chart stack: pick a solver history
// CSV and the curves appear; the tree on the left controls which curves are
// drawn, and the panel under the charts edits the selected curve's appearance.
//
// It exists so the module can be seen and driven without wiring it into the
// post-processing workbench, and it deliberately uses only the public API of
// MonitorLogReader, MonitorSeriesModel and MonitorChartView -- the same calls a
// workbench would make. No Qwt type appears in this file or in the demo.
class MonitorChartDemoWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MonitorChartDemoWindow(QWidget *parent = nullptr);

    // Reads a solver history file and draws every numeric column in it.
    bool loadFile(const QString &path, QString *error);

    // ---- Programmatic equivalents of the widgets ---------------------------
    // A scripted run (see selfTest) drives exactly the code a click drives, so
    // "works when clicked" and "works in the test" cannot diverge.
    bool setCurveVisible(const QString &curveId, bool visible);
    bool applyStyle(const QString &curveId, const MonitorCurveStyle &style);
    bool applyLabel(const QString &curveId, const QString &label);
    void selectCurve(const QString &curveId);
    bool setAxis(MonitorAxis axis);
    bool setResidualLogarithmic(bool on);
    // Global safety cap on samples per curve; 0 draws every sample. Kept as the
    // blunt instrument it is -- the control the UI exposes is the sampling
    // interval below, which is scoped.
    void setPointBudget(int budget);
    // Sampling interval for the current scope: 1 draws every sample, N draws
    // every Nth. The scope is the one chosen in the panel (selected curves,
    // their group, or everything).
    void setSampleInterval(int interval);
    void setOverlayMode(bool on);
    // Curves the tree currently has selected, so a scripted run can drive the
    // same multi-selection a user makes with ctrl-click.
    void setSelectedCurves(const QStringList &curveIds);
    QStringList selectedCurveIds() const;

    // ---- Live follow -------------------------------------------------------
    // Follows a history file while a solver appends to it. A poll appends rows
    // to the curves in place (MonitorChartView::appendPoints), so the zoom, the
    // selection and the styles survive; only a structural change -- a column
    // that appears late, or a solver restart -- falls back to a full rebuild.
    bool startFollowing(const QString &path, QString *error);
    void stopFollowing();
    bool isFollowing() const;
    // Advances the follower by hand. selfTest drives this instead of waiting on
    // the timer, so its assertions do not depend on wall-clock timing.
    bool pollLiveOnce(int *appendedRows, QString *error);
    QString liveStatusText() const;

    int curveCount() const { return curves_.size(); }
    // Number of chart widgets currently laid out (one per magnitude group, or a
    // single overlaid chart).
    int chartCount() const { return charts_.size(); }
    // Charts currently shown. A magnitude group whose every curve is unchecked
    // hides its whole block, so this is what "only the residuals are drawn"
    // means in numbers.
    int visibleChartCount() const;
    QString report() const;

    // Loads the fixture, walks every control path once and writes one PNG per
    // step so the result can be looked at instead of trusted.
    QString selfTest(const QString &shotDirectory);

private:
    void buildCurves();
    // Creates one chart per magnitude group (or one overlaid chart) and installs
    // them. Any style already stored in curves_ is carried into the new charts.
    void rebuildCharts();
    void applyCurves();
    void populateTree();
    void refreshAxisCombo();
    void loadStylePanel(const QString &curveId);
    void refreshStatus();
    void syncTreeCheckState(const QString &curveId, bool visible);
    QTreeWidgetItem *treeItemFor(const QString &curveId) const;
    int indexOfCurve(const QString &curveId) const;
    // The chart that currently owns a curve; null when it is not drawn.
    MonitorChartView *chartFor(const QString &curveId) const;
    MonitorCurveStyle styleFromPanel() const;
    // Only the fields the user actually changed, measured against the curve the
    // panel is showing. A group-wide edit must not stamp one marker size's worth
    // of colour onto curves that are told apart by colour.
    MonitorChartView::StylePatch patchFromPanel(const MonitorCurveStyle &base) const;
    // Applies a patch to whatever the scope combo selects, across every chart,
    // then mirrors the result back into curves_ so a later rebuild keeps it.
    void applyPatch(const MonitorChartView::StylePatch &patch);
    void syncStyleFromCharts(const QStringList &curveIds);
    // Folds one delivery into the accumulated log and pushes it onto the charts.
    // Returns false only when the delivery itself is malformed.
    bool applyLiveUpdate(const MonitorUpdate &update, int *appendedRows, QString *error);
    void refreshLiveLabel();
    void addShapeItem(const QString &text, MonitorPointShape shape);
    // The demo has no artwork of its own, so the four shapes that need external
    // content get a small built-in sample: a pentagon path, a circular pixmap,
    // the same image drawn as a graphic, and a five-point star in SVG.
    static MonitorPointContent buildDemoPointContent();

private slots:
    void onImportClicked();
    void onClearClicked();
    void onExportClicked();
    void onAxisChanged(int index);
    void onLogToggled(bool on);
    void onOverlayToggled(bool on);
    void onIntervalChanged(int value);
    void onItemChanged(QTreeWidgetItem *item, int column);
    void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onStyleEdited();
    void onColorClicked();
    void onSelectionFromChart(const QString &curveId);
    void onFollowClicked();
    void onFollowTicked();

private:
    MonitorSeriesModel model_;
    // The authoritative curve list. MonitorChartView keeps its own copy and
    // clears any run-time captions whenever setCurves() is called, so the demo
    // has to hold the edited styles itself and hand them back on every rebuild.
    QVector<MonitorCurve> curves_;
    QString sourcePath_;
    MonitorAxis axis_ = MonitorAxis::IterationStep;
    bool residualLogarithmic_ = true;
    bool overlayMode_ = false;

    // Group key -> chart. One chart per magnitude group is what keeps every
    // curve readable: Qwt puts exactly one scale on each side of a plot, so an
    // overlay of three magnitudes has to drop one of them.
    QHash<QString, MonitorChartView *> charts_;
    QHash<QString, QLabel *> chartStatus_;
    QVector<QString> groupOrder_;
    // Groups with at least one visible curve. Tracked here rather than read back
    // with isVisible(), which is false for every child of a window that was
    // never shown -- exactly the case in a headless self test.
    QSet<QString> visibleGroups_;
    // Curve identity -> the group it belongs to, so an edit can find its chart.
    QHash<QString, QString> curveGroup_;

    QScrollArea *chartScroll_ = nullptr;
    QWidget *chartHost_ = nullptr;
    QVBoxLayout *chartHostLayout_ = nullptr;
    QTreeWidget *curveTree_ = nullptr;
    QComboBox *axisCombo_ = nullptr;
    QCheckBox *logCheck_ = nullptr;
    QCheckBox *overlayCheck_ = nullptr;
    // Every Nth sample, per curve. A point budget says "this file is too big"; an
    // interval says "show me the shape, not every sample" -- the second is what a
    // chart panel offers.
    QSpinBox *intervalSpin_ = nullptr;
    // What an edit reaches: the selected curves, the group they belong to, or
    // every curve. A user thinks in these units, not in "one curve at a time".
    QComboBox *scopeCombo_ = nullptr;
    QLabel *fileLabel_ = nullptr;

    QLabel *selectionLabel_ = nullptr;
    QLineEdit *labelEdit_ = nullptr;
    QPushButton *colorButton_ = nullptr;
    QComboBox *lineStyleCombo_ = nullptr;
    QComboBox *shapeCombo_ = nullptr;
    QDoubleSpinBox *pointSizeSpin_ = nullptr;
    QComboBox *interpolationCombo_ = nullptr;
    // Trend fitting is a separate question from joining: the joining combo
    // connects neighbouring samples, the fit approximates the whole series.
    QComboBox *fitCombo_ = nullptr;
    QSpinBox *fitDegreeSpin_ = nullptr;
    QSpinBox *fitWindowSpin_ = nullptr;
    QComboBox *fitLineStyleCombo_ = nullptr;
    // Handed to every curve whose shape is Path, Pixmap, Graphic or Svg.
    MonitorPointContent demoPointContent_;

    // Live following state. The accumulator speaks the same MonitorLog the
    // offline reader produces, so nothing below this line knows the data arrived
    // in pieces.
    MonitorLogAccumulator liveAccumulator_;
    std::unique_ptr<MonitorLiveSource> liveSource_;
    QTimer *followTimer_ = nullptr;
    QPushButton *followButton_ = nullptr;
    QLabel *liveLabel_ = nullptr;
    qint64 livePolls_ = 0;
    int liveAppendedRows_ = 0;
    // Column keys the currently drawn curves were built from. A delivery that
    // changes them needs a rebuild; one that does not is appended in place.
    QStringList curveKeys_;

    QString currentId_;
    // Kept so a rebuild can re-apply the cap; the chart is recreated by
    // setCurves() below, so nothing survives in the widget itself.
    int pointBudget_ = 0;
    QColor currentColor_ = QColor(Qt::blue);
    // Guards the feedback loops between widgets and the chart: selecting in the
    // chart selects in the tree, which reloads the panel, which would edit the
    // style right back.
    bool updating_ = false;
};

} // namespace demo
} // namespace monitor
} // namespace aerosim
