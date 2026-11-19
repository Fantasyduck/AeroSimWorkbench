#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/GoverningEquationsPanel.h"
#include "aerosim/ui/CalculationControlPanel.h"
#include "aerosim/ui/OutputControlPanel.h"
#include "aerosim/ui/SolutionMethodPanel.h"
#include <QTableWidget>
#include "aerosim/ui/RadioChoice.h"
#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    QString error;
    const auto input = dir.filePath("input.xcf"), output = dir.filePath("output.xcf");
    aerosim::XcfProjectDocument source;
    if (!source.save(input, &error)) return 1;
    aerosim::MainWindow window;
    if (!window.openXcfProject(input, &error)) { QTextStream(stderr) << error; return 2; }
    auto *panel = window.findChild<aerosim::GoverningEquationsPanel *>();
    if (!panel) return 3;
    auto *laminar = panel->findChild<QRadioButton *>("laminarFlow");
    if (!laminar) return 4;
    laminar->setChecked(true);
    panel->findChild<QDoubleSpinBox *>("knudsenNumber")->setValue(0.123456789012345);
    if (!window.xcfProject().isModified() || !window.saveXcfProject(output, &error)) return 5;
    aerosim::XcfProjectDocument saved;
    if (!saved.load(output, &error)
        || saved.governingEquations().find("flowRegime")->value.textValue != "laminar"
        || saved.governingEquations().find("knudsenNumber")->value.realValue != 0.123456789012345) return 6;
    if (!window.openXcfProject(input, &error)) return 7;
    panel = window.findChild<aerosim::GoverningEquationsPanel *>();
    if (!panel->findChild<QRadioButton *>("turbulentFlow")->isChecked()) return 8;
    // Locate the footer within the parameter editor, independent of scroll nesting.
    auto *editor = window.findChild<QWidget *>("parameterEditor_control-equations");
    auto *reset = editor->findChild<QPushButton *>("resetParameterDraft");
    panel->findChild<QRadioButton *>("eulerEquation")->setChecked(true);
    reset->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (window.xcfProject().governingEquations().find("equation")->value.textValue != "navier-stokes") return 9;
    if (!window.saveXcfProject(output, &error)) return 10;
    QFile bad(dir.filePath("bad.xcf")); bad.open(QIODevice::WriteOnly); bad.write("invalid"); bad.close();
    if (window.openXcfProject(bad.fileName(), &error) || window.xcfProject().filePath() != output) return 11;
    if (window.findChild<QAction *>("open-projectAction")
        || window.findChild<QAction *>("save-projectAction")) return 12;
    auto *tree = window.findChild<QTreeWidget *>("workspaceTree");
    const auto showCalculation = [&] {
        tree->setCurrentItem(nullptr);
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole + 1).toString() == "calculation-control") {
                tree->setCurrentItem(*it);
                break;
            }
    };
    showCalculation();
    auto *calc = window.findChild<aerosim::CalculationControlPanel *>();
    if (!calc) return 15;
    calc->findChild<QSpinBox *>("maximumIterations")->setValue(54321);
    calc->findChild<QDoubleSpinBox *>("convergenceResidual")->setValue(-13.1234567890123);
    auto *gpu = calc->findChild<aerosim::RadioChoice *>("useGpu");
    auto *settings = calc->findChild<QWidget *>("gpuSettingsGroup");
    if (!settings->isHidden()) return 16;
    gpu->setCurrentIndex(1);
    if (settings->isHidden()) return 17;
    calc->findChild<QSpinBox *>("gpuCount")->setValue(2);
    calc->findChild<QComboBox *>("gpuIndex")->setCurrentText("2,3");
    if (!window.xcfProject().isModified() || !window.saveXcfProject(output, &error)) return 18;
    if (!saved.load(output, &error)) return 19;
    const auto &entries = saved.calculationControl().entries;
    for (const auto &entry : entries) {
        if (entry.key == "maximumIterations" && entry.value.integerValue != 54321) return 20;
        if (entry.key == "gpuCount" && entry.value.integerValue != 2) return 21;
        if (entry.key == "convergenceResidual" && entry.value.realValue != -13.1234567890123) return 22;
    }
    if (!window.openXcfProject(output, &error)) return 23;
    showCalculation();
    calc = window.findChild<aerosim::CalculationControlPanel *>();
    if (!calc || calc->findChild<QSpinBox *>("maximumIterations")->value() != 54321
        || calc->findChild<QDoubleSpinBox *>("convergenceResidual")->value() != -13.1234567890123
        || calc->findChild<aerosim::RadioChoice *>("useGpu")->currentIndex() != 1
        || calc->findChild<QSpinBox *>("gpuCount")->value() != 2
        || !calc->findChild<QComboBox *>("gpuIndex")->currentText().isEmpty()) return 24;
    const auto showOutput = [&] {
        tree->setCurrentItem(nullptr);
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole + 1).toString() == "output-control") {
                tree->setCurrentItem(*it); break;
            }
    };
    // Seed a project-specific boundary; it must survive the UI conversion.
    auto outConfig = window.xcfProject().outputControl();
    aerosim::ParameterEntry boundaryEntry; boundaryEntry.key = "boundaryHistory/custom_boundary";
    boundaryEntry.value = aerosim::ParameterValue::boolean(true); outConfig.entries.append(boundaryEntry);
    aerosim::XcfProjectDocument seeded = window.xcfProject();
    if (!seeded.setOutputControl(outConfig, &error) || !seeded.save(output, &error)
        || !window.openXcfProject(output, &error)) return 25;
    showOutput();
    auto *outPanel = window.findChild<aerosim::OutputControlPanel *>();
    if (!outPanel || outPanel->findChildren<QCheckBox *>().size() != 35) return 26;
    outPanel->findChild<QSpinBox *>("restartInterval")->setValue(111);
    outPanel->findChild<QSpinBox *>("wallInterval")->setValue(-11);
    outPanel->findChild<QSpinBox *>("flowInterval")->setValue(-12);
    outPanel->findChild<QPushButton *>("flowVariablesSelectAll")->click();
    outPanel->findChild<QCheckBox *>("flowVariables_vorticity-x")->setChecked(false);
    outPanel->findChild<QCheckBox *>("wallVariables_total-pressure")->setChecked(true);
    outPanel->findChild<QCheckBox *>("boundaryHistory_custom_boundary")->setChecked(false);
    if (!window.xcfProject().isModified() || !window.saveXcfProject(output, &error)
        || !window.openXcfProject(output, &error)) return 27;
    showOutput();
    outPanel = window.findChild<aerosim::OutputControlPanel *>();
    if (!outPanel || outPanel->findChild<QSpinBox *>("wallInterval")->value() != -11
        || outPanel->findChild<QSpinBox *>("flowInterval")->value() != -12
        || outPanel->findChild<QCheckBox *>("flowVariables_vorticity-x")->isChecked()
        || !outPanel->findChild<QCheckBox *>("flowVariables_q-criterion")->isChecked()
        || !outPanel->findChild<QCheckBox *>("wallVariables_total-pressure")->isChecked()) return 28;
    auto *outEditor = window.findChild<QWidget *>("parameterEditor_output-control");
    outEditor->findChild<QPushButton *>("resetParameterDraft")->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (window.xcfProject().outputControl().find("wallInterval")->value.integerValue != -1
        || !window.saveXcfProject(output, &error)) return 29;
    const auto showSolution = [&] {
        tree->setCurrentItem(nullptr);
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole + 1).toString() == "solution-method") {
                tree->setCurrentItem(*it); break;
            }
    };
    showSolution();
    auto *solution = window.findChild<aerosim::SolutionMethodPanel *>();
    if (!solution) return 30;
    solution->findChild<QRadioButton *>("spatialScheme_roe")->click();
    solution->findChild<QCheckBox *>("entropyFix")->setChecked(true);
    solution->findChild<QCheckBox *>("strongStability")->setChecked(true);
    solution->findChild<QRadioButton *>("limiter_hvanalbada")->click();
    solution->findChild<QRadioButton *>("timeScheme_bdf3")->setChecked(true);
    solution->findChild<QDoubleSpinBox *>("timeStep")->setValue(0.123456789012345);
    solution->findChild<QSpinBox *>("subIterations")->setValue(42);
    solution->findChild<QSpinBox *>("cflPointCount")->setValue(3);
    auto *cflTable = solution->findChild<QTableWidget *>("cflTable");
    qobject_cast<QSpinBox *>(cflTable->cellWidget(2, 0))->setValue(4000);
    qobject_cast<QDoubleSpinBox *>(cflTable->cellWidget(2, 1))->setValue(0.123456789012345);
    if (!window.xcfProject().isModified() || !window.saveXcfProject(output, &error)
        || !window.openXcfProject(output, &error)) return 31;
    showSolution();
    solution = window.findChild<aerosim::SolutionMethodPanel *>();
    cflTable = solution->findChild<QTableWidget *>("cflTable");
    if (!solution->findChild<QRadioButton *>("spatialScheme_roe")->isChecked()
        || solution->findChild<QCheckBox *>("entropyFix")->isHidden()
        || !solution->findChild<QCheckBox *>("entropyFix")->isChecked()
        || !solution->findChild<QCheckBox *>("strongStability")->isChecked()
        || !solution->findChild<QRadioButton *>("limiter_hvanalbada")->isChecked()
        || !solution->findChild<QRadioButton *>("timeScheme_bdf3")->isChecked()
        || solution->findChild<QDoubleSpinBox *>("timeStep")->value() != 0.123456789012345
        || solution->findChild<QSpinBox *>("subIterations")->value() != 42
        || cflTable->rowCount() != 3
        || qobject_cast<QSpinBox *>(cflTable->cellWidget(2, 0))->value() != 4000
        || qobject_cast<QDoubleSpinBox *>(cflTable->cellWidget(2, 1))->value() != 0.123456789012345) return 32;
    // Hidden values remain persisted for steady / non-Roe configurations.
    solution->findChild<QRadioButton *>("spatialScheme_hllc")->click();
    solution->findChild<QSpinBox *>("cflPointCount")->setValue(1);
    if (!window.saveXcfProject(output, &error) || !saved.load(output, &error)
        || !saved.solutionMethod().find("entropyFix")->value.booleanValue
        || saved.solutionMethod().find("timeScheme")->value.textValue != "bdf3"
        || saved.solutionMethod().find("cflPoints/1/cfl")) return 33;
    window.findChild<QWidget *>("parameterEditor_solution-method")->findChild<QPushButton *>("resetParameterDraft")->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (window.xcfProject().solutionMethod().find("entropyFix")->value.booleanValue
        || window.xcfProject().solutionMethod().find("cflPointCount")->value.integerValue != 2
        || !window.saveXcfProject(output, &error)) return 34;
    window.show();
    app.processEvents();
    panel = window.findChild<aerosim::GoverningEquationsPanel *>();
    panel->findChild<QRadioButton *>("eulerEquation")->setChecked(true);
    QTimer::singleShot(100, [] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->button(QMessageBox::Cancel)->click();
    });
    if (window.close() || !window.xcfProject().isModified()) return 13;
    QTimer::singleShot(100, [] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->button(QMessageBox::Save)->click();
    });
    if (!window.close() || window.xcfProject().isModified() || !saved.load(output, &error)
        || saved.governingEquations().find("flowRegime")->value.textValue != "inviscid") return 14;
    QTextStream(stdout) << "XCF -> typed model -> panel -> edited model -> saved XCF passed.\n";
    return 0;
}
