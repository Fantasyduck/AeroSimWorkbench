#include "aerosim/ui/RadioChoice.h"
#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/ReferenceValuesPanel.h"
#include "aerosim/ui/InitializationPanel.h"
#include <QDir>
#include <QTreeWidgetItemIterator>
#include "aerosim/ui/GoverningEquationsPanel.h"
#include "aerosim/ui/SolutionMethodPanel.h"
#include "aerosim/ui/MeshMotionPanel.h"
#include "aerosim/ui/BoundaryConditionsPanel.h"
#include "aerosim/ui/ForcedMotionDialog.h"
#include "aerosim/ui/FixedAxisMotionDialog.h"
#include "aerosim/ui/SixDofMotionDialog.h"
#include <QDialogButtonBox>
#include <QScreen>
#include <QScrollArea>
#include <QTabWidget>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSet>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const auto check = [](bool ok, const char *message) {
        if (!ok) QTextStream(stderr) << message << '\n';
        return ok;
    };
    aerosim::GoverningEquationsPanel equations(QVariantMap{});
    aerosim::SolutionMethodPanel solution(QVariantMap{});
    QObject::connect(&equations, &aerosim::GoverningEquationsPanel::draftChanged,
                     &solution, [&solution](const QVariantMap &draft) {
        solution.setUnsteady(draft.value("calculationType") == "unsteady");
    });
    auto *step = solution.findChild<QDoubleSpinBox *>("timeStep");
    equations.findChild<QRadioButton *>("calculationType_unsteady")->click();
    step->setValue(0.01234567);
    equations.findChild<QRadioButton *>("calculationType_steady")->click();
    if (!check(step->isHidden(), "Steady mode did not hide time settings")) return 1;
    equations.findChild<QRadioButton *>("calculationType_unsteady")->click();
    if (!check(!step->isHidden() && qAbs(step->value() - 0.01234567) < 1e-9,
               "Unsteady settings lost their value")) return 2;
    auto *count = solution.findChild<QSpinBox *>("cflPointCount");
    auto *cfl = solution.findChild<QTableWidget *>("cflTable");
    count->setValue(3);
    qobject_cast<QDoubleSpinBox *>(cfl->cellWidget(2, 1))->setValue(7.25);
    count->setValue(1);
    count->setValue(3);
    if (!check(qobject_cast<QDoubleSpinBox *>(cfl->cellWidget(2, 1))->value() == 7.25,
               "CFL points were lost after resizing")) return 3;

    aerosim::MeshMotionPanel motion(QVariantMap{});
    motion.findChild<QCheckBox *>("meshMotionEnabled")->setChecked(true);
    auto *bodies = motion.findChild<QSpinBox *>("motionBodyCount");
    bodies->setValue(1);
    motion.findChild<QLineEdit *>("motionBody0_name")->setText("retained-body");
    bodies->setValue(0);
    bodies->setValue(1);
    if (!check(motion.findChild<QLineEdit *>("motionBody0_name")->text() == "retained-body",
               "Motion body data was lost")) return 4;
    aerosim::BoundaryConditionsPanel boundary(QVariantMap{});
    auto *table = boundary.findChild<QTreeWidget *>("boundaryTable");
    for (int index = 0; index < table->topLevelItemCount(); ++index)
        if (table->topLevelItem(index)->text(0) == "farfield") table->setCurrentItem(table->topLevelItem(index));
    auto *type = boundary.findChild<aerosim::RadioChoice *>("farfieldParameterType");
    auto *pressure = boundary.findChild<QDoubleSpinBox *>("farfield_staticPressure");
    pressure->setValue(1234.5);
    type->setCurrentIndex(type->findData("altitude"));
    type->setCurrentIndex(type->findData("static-temperature-pressure"));
    if (!check(!pressure->isHidden() && pressure->value() == 1234.5,
               "Farfield parameter switching lost input")) return 6;
    auto *apply = boundary.findChild<QPushButton *>("applyBoundary");
    if (!check(apply->isEnabled(), "Edited boundary cannot be applied")) return 7;
    QVariantMap saved;
    QObject::connect(&boundary, &aerosim::BoundaryConditionsPanel::draftChanged,
                     &boundary, [&saved](const QVariantMap &draft) { saved = draft; });
    apply->click();
    bool retained = false;
    for (const auto &value : saved.value("boundaries").toList()) {
        const auto row = value.toMap();
        if (row.value("id") == "farfield")
            retained = row.value("farfield").toMap().value("staticPressure").toDouble() == 1234.5;
    }
    if (!check(retained, "Apply did not preserve farfield input")) return 8;
    // Construct dialogs directly: no nested modal event loop is needed here.
    aerosim::ForcedMotionDialog forced(QVariantMap{{"rotationType", "sinusoidal"}});
    forced.findChild<QDoubleSpinBox *>("rotationAmplitude")->setValue(18.5);
    auto *rotation = forced.findChild<aerosim::RadioChoice *>("rotationType");
    rotation->setCurrentIndex(rotation->findData("none"));
    rotation->setCurrentIndex(rotation->findData("sinusoidal"));
    forced.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    if (!check(forced.result() == QDialog::Accepted && forced.parameters().value("rotationAmplitude").toDouble() == 18.5,
               "Rotation model switching lost amplitude or acceptance")) return 9;
    for (bool release : {false, true}) {
        aerosim::SixDofMotionDialog dialog(QVariantMap{}, nullptr, release
            ? aerosim::SixDofMotionDialog::Mode::RotationRelease : aerosim::SixDofMotionDialog::Mode::SixDof);
        auto *forceCount = dialog.findChild<QSpinBox *>("externalForceCount");
        forceCount->setValue(2);
        auto *current = dialog.findChild<QComboBox *>("externalForceCurrent");
        current->setCurrentIndex(1);
        auto *forces = dialog.findChild<QTableWidget *>("externalForceTable");
        qobject_cast<QDoubleSpinBox *>(forces->cellWidget(0, 0))->setValue(42.5);
        forceCount->setValue(0);
        forceCount->setValue(2);
        current->setCurrentIndex(1);
        if (!check(qobject_cast<QDoubleSpinBox *>(forces->cellWidget(0, 0))->value() == 42.5,
                   "Load data lost after shrinking count")) return 10;
        auto *tabs = dialog.findChild<QTabWidget *>("motionLoadTabs");
        tabs->setCurrentIndex(tabs->count() - 1);
        dialog.findChild<QDoubleSpinBox *>("inertiaxy")->setValue(-0.25);
        dialog.show();
        app.processEvents();
        const QSize available = app.primaryScreen()->availableGeometry().size();
        if (!check(dialog.width() <= available.width() && dialog.height() <= available.height(),
                   "Motion dialog exceeds screen")) return 11;
        dialog.resize(760, 520);
        app.processEvents();
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        if (!check(dialog.rect().contains(buttons->geometry()), "Dialog footer is outside window")) return 12;
        buttons->button(QDialogButtonBox::Ok)->click();
        if (!check(dialog.result() == QDialog::Accepted && dialog.parameters().value("inertiaxy").toDouble() == -0.25,
                   "Motion parameters were not accepted")) return 13;
    }
    aerosim::FixedAxisMotionDialog fixed(QVariantMap{});
    auto *definition = fixed.findChild<aerosim::RadioChoice *>("axisDefinition");
    definition->setCurrentIndex(1);
    fixed.findChild<QDoubleSpinBox *>("axisPoint20")->setValue(0);
    auto *fixedButtons = fixed.findChild<QDialogButtonBox *>();
    fixedButtons->button(QDialogButtonBox::Ok)->click();
    if (!check(fixed.result() != QDialog::Accepted, "Degenerate axis was accepted")) return 14;
    fixed.findChild<QDoubleSpinBox *>("axisPoint20")->setValue(2);
    fixedButtons->button(QDialogButtonBox::Ok)->click();
    if (!check(fixed.result() == QDialog::Accepted, "Valid axis was rejected")) return 15;

    // Use the visible radio buttons to exercise exclusivity and draft wiring.
    QVariantMap solutionDraft;
    QObject::connect(&solution, &aerosim::SolutionMethodPanel::draftChanged,
                     &solution, [&solutionDraft](const QVariantMap &v) { solutionDraft = v; });
    auto *roe = solution.findChild<QRadioButton *>("spatialScheme_roe");
    auto *entropy = solution.findChild<QCheckBox *>("entropyFix");
    if (!check(entropy && entropy->isHidden() && !entropy->isChecked(),
               "Entropy fix should default off and hidden outside Roe")) return 34;
    roe->click();
    if (!check(!entropy->isHidden(), "Roe does not expose entropy fix")) return 35;
    entropy->click();
    solution.findChild<QRadioButton *>("spatialScheme_hllc")->click();
    if (!check(entropy->isHidden() && solutionDraft.value("entropyFix").toBool(),
               "Scheme switching lost entropy fix")) return 36;
    roe->click();
    solution.setUnsteady(true);
    solution.setUnsteady(false);
    aerosim::SolutionMethodPanel restoredSolution(solutionDraft);
    auto *restoredEntropy = restoredSolution.findChild<QCheckBox *>("entropyFix");
    if (!check(!entropy->isHidden() && entropy->isChecked()
               && !restoredEntropy->isHidden() && restoredEntropy->isChecked(),
               "Entropy fix does not survive mode switching or panel reconstruction")) return 37;
    solution.findChild<QRadioButton *>("limiter_none")->click();
    if (!check(solutionDraft.value("spatialScheme") == "roe" && solutionDraft.value("limiter") == "none"
               && roe->isChecked() && !solution.findChild<QRadioButton *>("spatialScheme_hllc")->isChecked(),
               "Radio groups interfere or fail to publish draft IDs")) return 16;

    aerosim::ReferenceValuesPanel reference(QVariantMap{});
    reference.findChild<QRadioButton *>("referenceInputMode_custom")->click();
    auto *referencePressure = reference.findChild<QDoubleSpinBox *>("staticPressure");
    reference.findChild<QRadioButton *>("referenceParameterType_static-temperature-pressure")->click();
    referencePressure->setValue(2468.5);
    reference.findChild<QRadioButton *>("referenceParameterType_altitude")->click();
    reference.findChild<QRadioButton *>("referenceParameterType_static-temperature-pressure")->click();
    if (!check(!referencePressure->isHidden() && referencePressure->value() == 2468.5,
               "Reference radio switching loses values")) return 17;

    aerosim::InitializationPanel initialization(QVariantMap{});
    initialization.findChild<QDoubleSpinBox *>("machNumber")->setValue(7.25);
    initialization.findChild<QRadioButton *>("initializationMode_reference")->click();
    initialization.findChild<QRadioButton *>("initializationMode_custom")->click();
    if (!check(initialization.findChild<QDoubleSpinBox *>("machNumber")->value() == 7.25,
               "Initialization mode switching loses custom state")) return 18;
    auto *regionCount = initialization.findChild<QSpinBox *>("initializationRegionCount");
    regionCount->setValue(1);
    initialization.findChild<aerosim::RadioChoice *>("initializationRegion0Shape")->setCurrentIndex(2);
    QVariantMap localDraft;
    QObject::connect(&initialization, &aerosim::InitializationPanel::draftChanged,
                     &initialization, [&localDraft](const QVariantMap &v) { localDraft = v; });
    initialization.findChild<QRadioButton *>("initializationRegion0ParameterType_altitude")->click();
    const auto region = localDraft.value("regions").toList().at(0).toMap();
    if (!check(region.value("parameterType") == "altitude"
               && region.value("state").toMap().value("parameterType") == "altitude"
               && initialization.findChild<aerosim::RadioChoice *>("initializationRegion0Shape")->currentData() == "cylinder",
               "Local parameter radio did not synchronize state or interfered with shape")) return 32;
    regionCount->setValue(0);
    regionCount->setValue(1);
    if (!check(initialization.findChild<aerosim::RadioChoice *>("initializationRegion0Shape")->currentData() == "cylinder"
               && initialization.findChild<aerosim::RadioChoice *>("initializationRegion0ParameterType")->currentData() == "altitude",
               "Local initialization choices lost after resizing")) return 23;

    // Exercise all eight embedded pages at desktop and minimum window widths.
    aerosim::MainWindow window;
    window.setLayoutSettingsScope("AeroSimRadioTest", "Layout");
    window.show();
    auto *navigation = window.findChild<QTreeWidget *>("workspaceTree");
    const QString captureDir = qEnvironmentVariable("AEROSIM_PARAMETER_CAPTURE_DIR");
    if (!captureDir.isEmpty()) QDir().mkpath(captureDir);
    const QStringList pages{"control-equations", "boundary-conditions", "mesh-motion", "solution-method",
                            "reference-values", "output-control", "initialization", "calculation-control"};
    for (int width : {1440, 1000, 1440}) {
        window.resize(width, 960);
        for (const auto &pageId : pages) {
            QTreeWidgetItem *node = nullptr;
            for (QTreeWidgetItemIterator it(navigation); *it; ++it)
                if ((*it)->data(0, Qt::UserRole + 1).toString() == pageId) node = *it;
            if (!check(node != nullptr, "Parameter navigation missing")) return 19;
            navigation->setCurrentItem(node);
            app.processEvents();
            auto *page = window.findChild<QWidget *>("parameterEditor_" + pageId);
            if (!check(page && page->isVisible(), "Parameter page did not open")) return 20;
            if (pageId == "control-equations")
                page->findChild<QRadioButton *>("calculationType_unsteady")->click();
            if (pageId == "reference-values")
                page->findChild<QRadioButton *>("referenceInputMode_custom")->click();
            if (pageId == "mesh-motion") {
                page->findChild<QCheckBox *>("meshMotionEnabled")->setChecked(true);
                page->findChild<QSpinBox *>("motionBodyCount")->setValue(2);
            }
            if (pageId == "initialization")
                page->findChild<QSpinBox *>("initializationRegionCount")->setValue(1);
            if (pageId == "boundary-conditions") {
                auto *boundaries = page->findChild<QTreeWidget *>("boundaryTable");
                for (int row = 0; row < boundaries->topLevelItemCount(); ++row)
                    if (boundaries->topLevelItem(row)->text(0) == "farfield")
                        boundaries->setCurrentItem(boundaries->topLevelItem(row));
            }
            app.processEvents();
            app.processEvents(); // Responsive group directions also update the scroll area's minimum size.
            for (auto *choices : page->findChildren<aerosim::RadioChoice *>()) {
                if (!choices->isVisible()) continue;
                const auto buttons = choices->findChildren<QRadioButton *>();
                int selected = 0;
                for (auto *button : buttons) {
                    selected += button->isChecked() ? 1 : 0;
                    if (!check(choices->rect().contains(button->geometry()), "Radio option clipped by its group")) {
                        QTextStream(stderr) << pageId << ' ' << width << ' ' << choices->objectName() << '\n';
                        return 21;
                    }
                }
                if (!check(selected == 1, "Radio choice must have one selection")) return 22;
            }
            if (pageId == "mesh-motion" || pageId == "initialization") {
                auto *editors = page->findChild<QTableWidget *>(pageId == "mesh-motion"
                    ? "motionBodyTable" : "initializationRegions");
                if (!check(editors->horizontalScrollBar()->maximum() == 0,
                           "Parameter table requires horizontal scrolling")) {
                    QTextStream(stderr) << pageId << ' ' << width << " viewport=" << editors->viewport()->width();
                    for (int column = 0; column < editors->columnCount(); ++column)
                        QTextStream(stderr) << " column=" << editors->columnWidth(column);
                    QTextStream(stderr) << '\n';
                    return 24;
                }
                for (int row = 0; row < editors->rowCount(); ++row) {
                    if (!check(editors->rowHeight(row) <= 100, "Choice row is unnecessarily tall")) {
                        QTextStream(stderr) << pageId << ' ' << width << ' ' << row << ' ' << editors->rowHeight(row) << '\n';
                        if (!captureDir.isEmpty()) window.grab().save(QDir(captureDir).filePath(pageId + QString::number(width) + ".png"));
                        return 25;
                    }
                    for (int column = 0; column < editors->columnCount(); ++column) {
                        auto *cell = editors->cellWidget(row, column);
                        auto *editor = cell->findChild<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
                        if (!check(editor && cell->rect().adjusted(7, 7, -7, -7).contains(editor->geometry()),
                                   "Table editor is clipped or lacks cell padding")) return 26;
                        if (!check(qAbs(editor->geometry().center().y() - cell->rect().center().y()) <= 1,
                                   "Table editor is not vertically centered")) return 27;
                        if (auto *choice = qobject_cast<aerosim::RadioChoice *>(editor)) {
                            QSet<int> rows;
                            for (auto *radio : choice->findChildren<QRadioButton *>()) rows.insert(radio->y());
                            if (!check(rows.size() == 2, "Table choices should form two compact rows")) return 28;
                        } else if (!check(editor->height() <= 36, "Table control stretched to fill row")) return 29;
                    }
                }
                if (pageId == "initialization") {
                    auto *global = page->findChild<QGroupBox *>("globalInitializationGroup");
                    auto *local = page->findChild<QGroupBox *>("localInitializationGroup");
                    if (!check(width == 1000 ? local->y() >= global->geometry().bottom()
                                            : local->x() >= global->geometry().right(),
                               "Initialization groups do not adapt to window width")) return 31;
                }
            }
            if (!captureDir.isEmpty()) window.grab().save(QDir(captureDir).filePath(pageId + QString::number(width) + ".png"));
            if (!captureDir.isEmpty() && pageId == "initialization" && width == 1000) {
                auto *scroll = page->findChild<QScrollArea *>("parameterScroll");
                const int position = scroll->verticalScrollBar()->value();
                scroll->ensureWidgetVisible(page->findChild<QGroupBox *>("localInitializationGroup"));
                app.processEvents();
                window.grab().save(QDir(captureDir).filePath("initialization1000-local.png"));
                scroll->verticalScrollBar()->setValue(position);
            }
        }
    }
    return 0;
}
