#pragma once

#include <QColor>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSlider;
class QSplitter;
class QTabWidget;
class QTableWidget;
class QTableWidgetItem;

namespace aerosim {

// Front-end-only mesh workspace. It owns no mesh data or VTK objects; the
// actual viewer remains supplied through MainWindow's Viewport widget slot.
// The page mirrors the reference mesh manager: a "网格信息" settings panel
// (coordinate system, mesh table, import/delete) sits left of the tabbed
// workspace that carries the persistent VTK viewport.
class MeshManagementPage final : public QWidget {
    Q_OBJECT
public:
    explicit MeshManagementPage(QWidget *viewportHost, QWidget *parent = nullptr);
    QVariantMap layoutState() const;
    void restoreLayoutState(const QVariantMap &state);
    void restoreDefaultLayout();
    void enableBoundaryMode();
    void beginBoundary(const QString &id, const QString &path);
    void finishBoundary(const QString &id, const QString &summary);
    void failBoundary(const QString &id, const QString &message);
    void setBoundaryBusy(bool busy);
signals:
    void boundaryImportRequested(const QString &path);
    void boundaryRemoveRequested(const QString &id);
    void boundaryStyleChanged(const QString &id, const QVariantMap &style);
    void boundaryViewRequested(const QString &command);
    void boundaryBackgroundRequested(int mode);
    void postProcessRequested();

private:
    struct MeshUiState {
        QString id;
        QString summary;
        bool ready = true;
        QString name;
        bool visible = true;
        QString displayMode;
        QColor color;
        int opacity = 100;
        double scale = 1.0;
        QString unit;
    };

    void addMesh();
    void removeCurrentMesh();
    void selectMesh(int row);
    void refreshTableRow(int row);
    void refreshEditor();
    void updateCurrentMesh();
    void updateCount();
    void appendHistory(const QString &message);
    void setHistoryCollapsed(bool collapsed);
    void emitBoundaryStyle(int row);
    int selectedRow() const;
    bool boundaryMode_ = false;
    bool boundaryBusy_ = false;

    QVector<MeshUiState> meshes_;
    QTableWidget *meshTable_ = nullptr;
    QLabel *meshCount_ = nullptr;
    QPushButton *importMeshButton_ = nullptr;
    QPushButton *deleteMeshButton_ = nullptr;
    QRadioButton *solidModelRadio_ = nullptr;
    QRadioButton *planarModelRadio_ = nullptr;
    QLineEdit *nameEditor_ = nullptr;
    QCheckBox *visibleEditor_ = nullptr;
    QComboBox *displayModeEditor_ = nullptr;
    QPushButton *colorButton_ = nullptr;
    QSlider *opacityEditor_ = nullptr;
    QLabel *opacityValue_ = nullptr;
    QDoubleSpinBox *scaleEditor_ = nullptr;
    QComboBox *unitEditor_ = nullptr;
    QLabel *viewerStatus_ = nullptr;
    // Custom colour axis triad; declared as QWidget so the header stays free
    // of the paint-only helper type.
    QWidget *axisIndicator_ = nullptr;
    QSplitter *mainSplitter_ = nullptr;
    QSplitter *settingsSplitter_ = nullptr;
    QTabWidget *workspaceTabs_ = nullptr;
    QTabWidget *historyTabs_ = nullptr;
    QListWidget *historyList_ = nullptr;
    QWidget *historyHeader_ = nullptr;
    QPushButton *historyCollapseButton_ = nullptr;
    int expandedHistoryHeight_ = 145;
    bool historyCollapsed_ = false;
    bool updatingEditor_ = false;
};

} // namespace aerosim
