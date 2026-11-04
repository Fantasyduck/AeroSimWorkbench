#include "aerosim/ui/StartupLandingPage.h"

#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextStream>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    aerosim::StartupLandingPage landing;
    const auto check = [](bool passed, const char *message) {
        if (!passed) QTextStream(stderr) << message << '\n';
        return passed;
    };
    auto *stack = landing.findChild<QStackedWidget *>("landingContentStack");
    auto *recent = landing.findChild<QTableWidget *>("recentWorkspaceTable");
    auto *search = landing.findChild<QLineEdit *>("landingSearch");
    auto *returnButton = landing.findChild<QPushButton *>("landingReturnButton");
    if (!check(stack && stack->count() == 7 && stack->currentIndex() == 0,
               "Landing page must expose seven navigation pages")) return 1;
    if (!check(recent && recent->rowCount() == 1 && search && returnButton && !returnButton->isEnabled(),
               "Landing start page is incomplete")) return 2;
    search->setText(QStringLiteral("not-found"));
    app.processEvents();
    if (!check(recent->isRowHidden(0), "Recent workspace Mock filter failed")) return 3;
    search->clear();
    landing.findChild<QPushButton *>("landingNewNav")->click();
    if (!check(stack->currentWidget()->objectName() == "landingNewPage"
               && landing.findChild<QPushButton *>("dynamicTemplateCard")
               && landing.findChild<QPushButton *>("thermalTemplateCard"),
               "New workspace and template page is incomplete")) return 4;
    bool entered = false;
    QString source;
    QObject::connect(&landing, &aerosim::StartupLandingPage::enterWorkbenchRequested,
                     [&entered, &source](const QString &value) { entered = true; source = value; });
    landing.findChild<QPushButton *>("createWorkspaceCard")->click();
    if (!check(entered && source == "new-workspace-mock", "New workspace did not request workbench entry")) return 5;
    entered = false;
    source.clear();
    landing.findChild<QPushButton *>("landingStartNav")->click();
    QMetaObject::invokeMethod(recent, "cellDoubleClicked", Qt::DirectConnection, Q_ARG(int, 0), Q_ARG(int, 0));
    if (!check(entered && source == "recent-mock", "Recent workspace did not request workbench entry")) return 6;
    landing.findChild<QPushButton *>("landingOpenNav")->click();
    if (!check(stack->currentWidget()->objectName() == "landingOpenPage"
               && landing.findChild<QPushButton *>("openWorkspaceMockButton"),
               "Open workspace placeholder is incomplete")) return 7;
    landing.setReturnAvailable(true);
    if (!check(returnButton->isEnabled(), "Return button state was not updated")) return 8;
    QTextStream(stdout) << "Startup landing navigation and Mock entry interactions passed\n";
    return 0;
}
