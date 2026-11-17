#pragma once

#include "aerosim/ui/IntegrationPorts.h"
#include <SARibbonMainWindow.h>

#include <QList>
#include <QHash>
#include <QPointer>
#include <QVariantMap>
#include <QColor>
#include <QStringList>

class QAction;
class QDialog;
class QComboBox;
class QFileDialog;
class QLabel;
class QListWidget;
class QDockWidget;
class QSlider;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QWidget;

namespace aerosim {

// The post-processing workspace owns Qt controls only. A visualization scene
// service supplies the actual VTK widget through WidgetSlot::Viewport.
class PostProcessWorkbenchWindow final : public SARibbonMainWindow, public IWidgetHost {
    Q_OBJECT
public:
    explicit PostProcessWorkbenchWindow(QWidget *parent = nullptr);

    QWidget *windowWidget() override { return this; }
    bool insertWidget(WidgetSlot slot, QWidget *widget) override;
    void bindServices(const ServiceBindings &services) override;

public slots:
    // GUI-thread presentation endpoint; no picking or numerical work is done here.
    void displayProbeResult(const QString &text);
    void setDataServiceAvailable(bool available);
    void presentImportedData(const QVariantList &datasets, bool replace);
    void presentImportError(const QString &message);

signals:
    void viewRequested(const QString &orientation);
    void postProcessCommandRequested(const QString &command);
    // UI requests only; no reader or exporter is currently connected.
    void importFilesRequested(const QStringList &paths);
    void importDataRequested(const QStringList &paths, const QString &mode);
    void datasetVisibilityChanged(const QString &dataset, bool visible);
    void datasetRemovalRequested(const QString &dataset);
    void regionVisibilityChanged(const QString &dataset, const QString &region, bool visible);
    void dataSelectionChanged(const QString &dataset, const QString &region,
                              const QString &field, const QString &association);
    void exportFileRequested(const QString &kind, const QString &path);
    // GUI-thread view preference; a future viewer adapter must apply it to VTK.
    void backgroundChanged(const QColor &bottom, const QColor &top, bool gradient);

private:
    QWidget *createDataPanel();
    QWidget *createObjectPanel();
    QWidget *createViewportPanel();
    QWidget *createTimeline();
    void createActions();
    void openDeepSeekHarness();
    void showProbePanel();
    void openObjectStyle();
    void openObjectParameters(bool colorOnly = false);
    void openSelectedSettings();
    void updateSelectionActions();
    void openImportDialog();
    void openExportDialog(const QString &kind);
    void loadUiExample(bool structured);
    void loadMultiFileUiExample();
    void openBackgroundSettings();
    void applyBackground(const QColor &bottom, const QColor &top, bool gradient);
    void showNavigationPanel(int panel);
    void updateDataSelection(QTreeWidgetItem *item, int column);
    void addDerivedLayer(const QString &kind, const QString &name, const QString &message);
    void openNewFeatureDialog(const QString &kind);
    void appendMessage(const QString &message);
    void updateSelectionDetails();

    QVBoxLayout *viewportLayout_ = nullptr;
    QWidget *emptyViewport_ = nullptr;
    QTreeWidget *datasetTree_ = nullptr;
    QListWidget *sceneList_ = nullptr;
    QAction *styleAction_ = nullptr;
    QAction *parameterAction_ = nullptr;
    QPointer<QDialog> styleDialog_;
    QPointer<QDialog> parameterDialog_;
    QPointer<QDialog> sourceDialog_;
    QPointer<QDialog> importOrganizationDialog_;
    QPointer<QDialog> exportSetupDialog_;
    QPointer<QFileDialog> exportDialog_;
    QString selectedId_;
    QString selectedKind_;
    QString selectedName_;
    QVariantMap selectedContext_;
    QWidget *navigationPanels_[3] = {nullptr, nullptr, nullptr};
    QColor backgroundBottom_ = QColor(255, 255, 255);
    QColor backgroundTop_ = QColor(82, 87, 110);
    bool gradientBackground_ = true;
    QHash<QString, QVariantMap> styleDrafts_;
    QHash<QString, QVariantMap> parameterDrafts_;
    int nextObjectId_ = 0;
    QSlider *timeSlider_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QLabel *sessionContext_ = nullptr;
    QLabel *viewportTitle_ = nullptr;
    QLabel *viewportHint_ = nullptr;
    QLabel *selectionSummary_ = nullptr;
    QTextEdit *probeResult_ = nullptr;
    QDockWidget *probeDock_ = nullptr;
    QDockWidget *deepSeekHarnessDock_ = nullptr;
    ServiceBindings services_;
    bool bound_ = false;
    bool dataServiceAvailable_ = false;
};

} // namespace aerosim
