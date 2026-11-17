#include "aerosim/ui/MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QComboBox>
#include <QDir>
#include <QDialogButtonBox>
#include <QDialog>
#include <QGroupBox>
#include <QLineEdit>
#include <QProcessEnvironment>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace {
QString text(const char16_t *value)
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value));
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    aerosim::MainWindow window;
    window.show();
    app.processEvents();
    const auto check = [](bool condition, const char *message) {
        if (!condition) QTextStream(stderr) << message << '\n';
        return condition;
    };

    auto *tree = window.findChild<QTreeWidget *>("workspaceTree");
    auto *workspace = window.findChild<QStackedWidget *>("centralWorkspace");
    auto *property = window.findChild<QDockWidget *>("propertyDock");
    if (!check(tree && workspace && property, "Main calculation workbench controls are missing")) return 1;
    if (!check(tree->topLevelItemCount() == 1, "Workspace navigation must have one root")) return 2;
    auto *root = tree->topLevelItem(0);
    auto *calculation = root->child(2);
    if (!check(calculation && calculation->text(0) == text(u"\u8ba1\u7b97\u4efb\u52a1")
               && calculation->childCount() == 2,
               "Calculation navigation must expose exactly two task pages")) return 3;
    if (!check(calculation->child(0)->text(0) == text(u"\u5355\u4efb\u52a1")
               && calculation->child(1)->text(0) == text(u"\u591a\u4efb\u52a1")
               && calculation->child(0)->childCount() == 0
               && calculation->child(1)->childCount() == 0,
               "Single and multiple task pages must be leaf nodes")) return 4;

    tree->setCurrentItem(calculation->child(0));
    app.processEvents();
    auto *single = window.findChild<QWidget *>("singleTaskPage");
    if (!check(workspace->currentWidget()->objectName() == "singleTaskPage"
               && single && property->isHidden()
               && single->findChild<QCheckBox *>("singleTaskResume")
               && single->findChild<QPushButton *>("submitCalculation")
               && single->findChild<QComboBox *>("submissionModeCombo")
               && single->findChildren<QGroupBox *>("calculationObservables").size() == 1
               && single->findChild<QPushButton *>("calculationConditionTab")
               && single->findChild<QPlainTextEdit *>("calculationRunHistory")
               && single->findChild<QPlainTextEdit *>("calculationSolverOutput")
               && single->findChild<QCheckBox *>("singleTaskResume"),
               "Single task reference layout is incomplete")) return 5;
    if (!check(single->findChild<QPushButton *>("calculationConditionTab")->text() == text(u"\u5355\u5de5\u51b5")
               && !single->findChild<QPushButton *>("calculationMonitorTab"),
               "Single task must expose only the single-condition view button")) return 6;
    const QString captureDir = qEnvironmentVariable("AEROSIM_CALCULATION_CAPTURE_DIR");
    if (!captureDir.isEmpty()) window.grab().save(QDir(captureDir).filePath(QStringLiteral("single-task.png")));
    if (!check(!window.findChild<QDockWidget *>("outputDock")->isVisible(),
               "Calculation pages must keep the log inside the right-hand page")) return 7;
    single->findChild<QPushButton *>("submitCalculation")->click();
    app.processEvents();
    if (!check(single->findChild<QPlainTextEdit *>("calculationRunHistory")->toPlainText().contains(text(u"已提交计算")),
               "Submitting a single task must record a submitted message")) return 8;

    tree->setCurrentItem(calculation->child(1));
    app.processEvents();
    auto *multi = window.findChild<QWidget *>("multipleTaskPage");
    auto *table = window.findChild<QTableWidget *>("multipleTaskTable");
    if (!check(workspace->currentWidget()->objectName() == "multipleTaskPage"
               && multi && table && table->columnCount() == 8
               && property->isHidden()
               && multi->findChild<QPushButton *>("calculationConditionTab")->text() == text(u"\u591a\u5de5\u51b5")
               && !multi->findChild<QPushButton *>("calculationMonitorTab"),
               "Multiple task reference layout is incomplete")) return 9;
    int categoryButtons = 0;
    for (auto *button : multi->findChildren<QPushButton *>())
        categoryButtons += button->property("taskCategory").toBool();
    if (!check(categoryButtons == 5
               && multi->findChild<QGroupBox *>("taskCategoryGroup")
               && window.findChild<QPushButton *>("removeSelectedTasks")
               && !window.findChild<QPushButton *>("addTask")
               && !window.findChild<QPushButton *>("clearTasks"),
               "Multiple task category controls are incomplete")) return 10;
    if (!captureDir.isEmpty()) window.grab().save(QDir(captureDir).filePath(QStringLiteral("multiple-task.png")));
    if (!check(!window.findChild<QComboBox *>("multipleTaskCategory")
               && multi->findChild<QPlainTextEdit *>("calculationRunHistory")
               && multi->findChild<QPlainTextEdit *>("calculationSolverOutput")
               && multi->findChild<QPushButton *>("submitCalculation")
               && multi->findChild<QComboBox *>("submissionModeCombo")
               && multi->findChild<QComboBox *>("submissionModeCombo")->count() == 3,
               "Task category input must use buttons and retain the right-hand log")) return 11;

    QTimer::singleShot(0, &window, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog) return;
        const auto edits = dialog->findChildren<QLineEdit *>();
        const QStringList values = {QStringLiteral("100000"), QStringLiteral("300"),
                                    QStringLiteral("0.8"), QStringLiteral("2"), QStringLiteral("0")};
        for (int i = 0; i < edits.size() && i < values.size(); ++i) edits[i]->setText(values[i]);
        if (auto *buttons = dialog->findChild<QDialogButtonBox *>())
            buttons->button(QDialogButtonBox::Ok)->click();
    });
    multi->findChild<QPushButton *>("addTaskCategory_0")->click();
    app.processEvents();
    if (!check(table->rowCount() == 1, "A category button must add one configured task row")) return 12;
    if (!check(multi->findChild<QPlainTextEdit *>("calculationRunHistory")->toPlainText().contains(text(u"\u5df2\u6dfb\u52a0")),
               "Adding a task must be recorded in run history")) return 13;
    auto *selectAll = multi->findChild<QCheckBox *>("selectAllTasks");
    auto *remove = multi->findChild<QPushButton *>("removeSelectedTasks");
    table->selectRow(0);
    app.processEvents();
    if (!check(!selectAll->isChecked() && !remove->isEnabled(),
               "Highlighting a row must not select it for deletion")) return 14;
    remove->click();
    if (!check(table->rowCount() == 1, "Delete must ignore a highlighted but unchecked row")) return 15;
    table->item(0, 0)->setCheckState(Qt::Checked);
    app.processEvents();
    if (!check(selectAll->isChecked() && remove->isEnabled(),
               "The select-all indicator must reflect every sequence checkbox")) return 16;
    multi->findChild<QPushButton *>("submitCalculation")->click();
    if (!check(multi->findChild<QPlainTextEdit *>("calculationRunHistory")->toPlainText().contains(text(u"已提交计算")),
               "Submitting multiple tasks must record a submitted message")) return 17;
    remove->click();
    if (!check(table->rowCount() == 0, "Removing the checked task must remove its row")) return 18;

    QTextStream(stdout) << "Calculation task reference UI checks passed\n";
    return 0;
}
