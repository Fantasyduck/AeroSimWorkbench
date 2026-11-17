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
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
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
    auto *type = boundary.findChild<QComboBox *>("farfieldParameterType");
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
    auto *rotation = forced.findChild<QComboBox *>("rotationType");
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
    auto *definition = fixed.findChild<QComboBox *>("axisDefinition");
    definition->setCurrentIndex(1);
    fixed.findChild<QDoubleSpinBox *>("axisPoint20")->setValue(0);
    auto *fixedButtons = fixed.findChild<QDialogButtonBox *>();
    fixedButtons->button(QDialogButtonBox::Ok)->click();
    if (!check(fixed.result() != QDialog::Accepted, "Degenerate axis was accepted")) return 14;
    fixed.findChild<QDoubleSpinBox *>("axisPoint20")->setValue(2);
    fixedButtons->button(QDialogButtonBox::Ok)->click();
    if (!check(fixed.result() == QDialog::Accepted, "Valid axis was rejected")) return 15;
    return 0;
}
