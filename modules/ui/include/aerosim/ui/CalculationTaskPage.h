#pragma once

#include "aerosim/ui/CalculationTaskInputDialog.h"
#include <QWidget>
#include <QVariantMap>
#include <QVector>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QTableWidget;
class QVBoxLayout;

namespace aerosim {

// Reference-layout UI only. Input rows remain window-local; submission does
// not manufacture a validated snapshot or invoke a solver.
class CalculationTaskPage final : public QWidget {
    Q_OBJECT
public:
    explicit CalculationTaskPage(bool multiple, QWidget *parent = nullptr);
    void attachMonitor(QWidget *widget);
    void appendLog(const QString &message);
    void appendSolverOutput(const QString &message);
    QVariantMap layoutState() const;
    void restoreLayoutState(const QVariantMap &state);
    void restoreDefaultLayout();

private:
    struct InputRow {
        CalculationInputKind kind;
        QVector<double> values;
        quint64 id = 0;
    };
    QWidget *createSingleSettings();
    QWidget *createMultipleSettings();
    QWidget *createSubmissionControls();
    QWidget *createObservables();
    void addTasks(CalculationInputKind kind);
    void editTask(int row);
    void fillRow(int row);
    void updateSelection();
    void removeSelected();
    bool multiple_;
    quint64 nextId_ = 1;
    QVector<InputRow> rows_;
    QTableWidget *table_ = nullptr;
    QCheckBox *selectAll_ = nullptr;
    QPushButton *remove_ = nullptr;
    QLabel *count_ = nullptr;
    QCheckBox *resume_ = nullptr;
    QPlainTextEdit *history_ = nullptr;
    QPlainTextEdit *solverOutput_ = nullptr;
    QSplitter *verticalSplitter_ = nullptr;
    QSplitter *mainSplitter_ = nullptr;
    QSplitter *outputSplitter_ = nullptr;
    QSplitter *monitorSplitter_ = nullptr;
    QVBoxLayout *curveLayout_ = nullptr;
    QWidget *emptyChart_ = nullptr;
};

} // namespace aerosim
