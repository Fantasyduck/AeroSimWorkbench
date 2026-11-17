// Framework-level regression tests for the shared UI layout contract.
//
// These are NOT business-page tests: they exercise the public framework only
// (UiLayoutConfig numbers, the MainWindow chrome, the central page container
// semantics and the layout persistence rules) using simple synthetic widgets,
// so they never depend on a solver, mesh data, VTK rendering or a real user
// configuration file.
//
// The layout save/restore tests redirect MainWindow to an isolated QSettings
// scope (org/app pair) that is cleared before each run, so the real user's
// "AeroSimWorkbench/UiLayout" record is never read or overwritten.
//
// Run directly (cocoa) on macOS, or via ctest with QT_QPA_PLATFORM=offscreen.
#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/UiLayoutConfig.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QSize>
#include <QSettings>
#include <QStackedWidget>
#include <QSplitter>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace {
int failures = 0;
void check(bool ok, const char *message)
{
    if (!ok) {
        QTextStream(stderr) << "FAIL: " << message << '\n';
        ++failures;
    }
}

// Does any item under a tree (text-wise) match the given text?
bool treeContainsText(const QTreeWidget *tree, const QString &text)
{
    if (!tree->topLevelItemCount()) return false;
    QList<QTreeWidgetItem *> pending;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) pending.append(tree->topLevelItem(i));
    while (!pending.isEmpty()) {
        QTreeWidgetItem *item = pending.takeLast();
        for (int col = 0; col < item->columnCount(); ++col) {
            if (item->text(col).contains(text)) return true;
        }
        for (int i = 0; i < item->childCount(); ++i) pending.append(item->child(i));
    }
    return false;
}

// 1) Pure config assertions (no windowing dependence except screen helpers).
void testConfig()
{
    using namespace aerosim::uimetrics;
    check(defaultWindowSize() == QSize(1440, 960), "default window size mismatch");
    check(minimumWindowSize() == QSize(1000, 720), "minimum window size mismatch");
    check(designReferenceSize() == QSize(1920, 1080), "design reference mismatch");
    check(projectDockWidth() == 260, "project dock default width");
    check(projectDockMinimumWidth() == 220, "project dock min width");
    check(propertyDockMinimumWidth() == 180, "property dock min width");
    check(controlHeight() == 28, "control height");
    check(sectionTitleHeight() == 28, "section title height");
    check(pageMargins() == QMargins(24, 20, 24, 20), "page margins");
    QWidget workbench;
    auto *workbenchLayout = createWorkbenchRootLayout(&workbench);
    check(workbenchLayout->contentsMargins() == QMargins(0, 0, 0, 0),
          "workbench root has no outer margins");
    check(workbenchLayout->spacing() == 0, "workbench root has no outer spacing");
    // clampRatio behaviour.
    check(clampRatio(-1.0) == 0.10, "clampRatio lower bound");
    check(clampRatio(2.0) == 0.45, "clampRatio upper bound");
    check(clampRatio(0.25) == 0.25, "clampRatio passes through mid value");
    check(clampProjectDockRatio(0.01) == kProjectDockRatioMin, "project ratio lower clamp");
    check(clampProjectDockRatio(0.99) == kProjectDockRatioMax, "project ratio upper clamp");
    // Screen helpers must be null-safe: no screen -> safe fallback, finite DPI.
    const qreal dpi = logicalDpi(nullptr);
    check(qIsFinite(dpi) && dpi > 0.0, "logicalDpi(null) finite positive");
    const qreal scale = scaleFactor(nullptr);
    check(qIsFinite(scale) && scale > 0.0, "scaleFactor(null) finite positive");
    const QRect rect = availableRect(nullptr);
    check(rect.isValid(), "availableRect(null) valid");
}

// 2) Independent central-page-container semantics (synthetic widgets only).
void testPageContainer(QApplication &app)
{
    QMainWindow host;
    auto *stack = new QStackedWidget(&host);
    host.setCentralWidget(stack);

    auto *pageA = new QWidget(stack);
    pageA->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *layoutA = new QVBoxLayout(pageA);
    layoutA->addWidget(new QWidget(pageA));
    auto *pageB = new QWidget(stack);
    pageB->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    stack->addWidget(pageA);   // index 0
    stack->addWidget(pageB);   // index 1
    const QWidget *const pageAPtr = pageA;
    const QWidget *const pageBPtr = pageB;

    host.resize(800, 600);
    host.show();
    app.processEvents();

    check(stack->count() == 2, "two pages registered");
    check(stack->currentWidget() == pageA, "first page active by default");
    // The parent container fills the central area and any child page gets its size.
    check(pageA->width() == stack->width(), "pageA fills container width");
    check(pageA->height() == stack->height(), "pageA fills container height");

    // Switching reuses the same instances (never recreated).
    stack->setCurrentWidget(pageB);
    app.processEvents();
    check(stack->currentWidget() == pageB, "pageB active after switch");
    check(stack->widget(1) == pageBPtr, "pageB instance stable across switch");
    check(stack->widget(0) == pageAPtr, "pageA instance stable across switch");

    // The host window size is not affected by which page is shown.
    const QSize hostSize = host.size();
    stack->setCurrentWidget(pageA);
    app.processEvents();
    check(host.size() == hostSize, "page switch does not resize host");

    // A larger host propagates to the (Expanding) page.
    host.resize(1100, 760);
    app.processEvents();
    check(pageA->width() == stack->width() && pageA->height() == stack->height(),
          "page follows container resize");
}

// 3) MainWindow chrome + layout persistence, using an isolated QSettings scope.
void testMainWindow()
{
    using namespace aerosim::uimetrics;
    aerosim::MainWindow window;
    // Isolated scope so the real user layout is never read or written.
    window.setLayoutSettingsScope(QStringLiteral("AeroSimWorkbenchFrameworkTest"),
                                  QStringLiteral("UiLayoutFrameworkTest"));

    // Clean any previous test record.
    QSettings clean(QStringLiteral("AeroSimWorkbenchFrameworkTest"),
                    QStringLiteral("UiLayoutFrameworkTest"));
    clean.clear();
    clean.sync();

    window.show();
    QApplication::processEvents();

    // Minimum size constraint comes from the shared config.
    check(window.minimumSize() == minimumWindowSize(), "main window min size from config");
    // Default size only asserted when the screen can hold it (offscreen/CI may clamp).
    if (availableRect(&window).width() >= defaultWindowSize().width()
        && availableRect(&window).height() >= defaultWindowSize().height()) {
        check(window.size() == defaultWindowSize(), "main window default size from config");
    } else {
        QTextStream(stderr) << "INFO: screen smaller than default window; skip exact size check\n";
    }

    // Central container is a QStackedWidget set as the central widget.
    auto *central = window.findChild<QStackedWidget *>(QStringLiteral("centralWorkspace"));
    check(central != nullptr, "central stacked container present");
    if (central) check(window.centralWidget() == central, "central stack is the central widget");
    // A real immersive page keeps its chrome edge-to-edge while its splitter
    // remains inside a separately padded content wrapper.
    auto *meshPage = central && central->count() ? central->widget(0) : nullptr;
    auto *meshHeader = meshPage ? meshPage->findChild<QWidget *>(QStringLiteral("meshManagementHeader")) : nullptr;
    auto *meshContent = meshPage ? meshPage->findChild<QWidget *>(QStringLiteral("meshWorkspaceContent")) : nullptr;
    auto *meshSplitter = meshPage ? meshPage->findChild<QSplitter *>(QStringLiteral("meshWorkspaceMainSplitter")) : nullptr;
    auto *viewportHost = meshPage ? meshPage->findChild<QWidget *>(QStringLiteral("viewportSlot")) : nullptr;
    auto *viewportEmptyState = viewportHost
        ? viewportHost->findChild<QLabel *>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    if (meshPage && meshHeader && meshContent && meshSplitter) {
        check(meshHeader->pos().y() == 0, "mesh header is flush with workbench top");
        check(meshHeader->width() == meshPage->width(), "mesh header spans workbench width");
        check(meshSplitter->pos().x() == pageMargins().left(), "mesh splitter has content left padding");
        check(meshSplitter->pos().y() == pageMargins().top(), "mesh splitter has content top padding");
    } else {
        check(false, "mesh workbench chrome and content exist");
    }
    check(viewportEmptyState && !viewportEmptyState->isVisible(),
          "mesh viewport empty state is hidden behind the workbench");
    // Switching the central page must not resize the main window itself.
    if (central && central->count() > 1) {
        const QSize sizeBefore = window.size();
        const int originalIndex = central->currentIndex();
        for (int i = 0; i < central->count(); ++i) {
            if (i != originalIndex) { central->setCurrentWidget(central->widget(i)); break; }
        }
        QApplication::processEvents();
        check(window.size() == sizeBefore, "central page switch does not resize main window");
        central->setCurrentIndex(originalIndex);
        QApplication::processEvents();
    }

    // Dock minimums come from the shared config; three docks only, no duplicates.
    auto *project = window.findChild<QDockWidget *>(QStringLiteral("projectDock"));
    auto *property = window.findChild<QDockWidget *>(QStringLiteral("propertyDock"));
    auto *output = window.findChild<QDockWidget *>(QStringLiteral("outputDock"));
    if (project) check(project->minimumWidth() == projectDockMinimumWidth(), "project dock min from config");
    if (property) check(property->minimumWidth() == propertyDockMinimumWidth(), "property dock min from config");
    check(window.findChildren<QDockWidget *>(QStringLiteral("projectDock")).size() == 1,
          "single project dock");
    check(window.findChildren<QDockWidget *>(QStringLiteral("outputDock")).size() == 1,
          "single output dock");

    // The previously-removed standalone compute-monitoring entry never returns.
    auto *tree = window.findChild<QTreeWidget *>(QStringLiteral("workspaceTree"));
    if (tree) {
        check(!treeContainsText(tree, QStringLiteral("计算监控")), "no compute-monitor module node");
        check(!treeContainsText(tree, QStringLiteral("实时监控")), "no live-monitor node");
    }

    // ---- Layout persistence priorities ----
    // Find layout actions.
    auto *restoreAction = window.findChild<QAction *>(QStringLiteral("restoreUserLayoutAction"));
    auto *defaultAction = window.findChild<QAction *>(QStringLiteral("layoutAction"));
    // Save is routed through the import-config id, which sets "saveUserLayoutAction".
    auto *saveAction = window.findChild<QAction *>(QStringLiteral("saveUserLayoutAction"));

    // (a) No record yet -> restore is a safe no-op ("尚无已保存").
    if (restoreAction) { restoreAction->setEnabled(true); restoreAction->trigger(); }
    QApplication::processEvents();
    check(QSettings(QStringLiteral("AeroSimWorkbenchFrameworkTest"),
                    QStringLiteral("UiLayoutFrameworkTest"))
              .value(QStringLiteral("UserLayout/version")).toInt() != 1,
          "no layout record persists before any save");

    // (b) Save a user layout -> a single record exists.
    if (saveAction) saveAction->trigger();
    QApplication::processEvents();
    check(QSettings(QStringLiteral("AeroSimWorkbenchFrameworkTest"),
                    QStringLiteral("UiLayoutFrameworkTest"))
              .value(QStringLiteral("UserLayout/version")).toInt() == 1,
          "save creates a layout record");

    // (c) Overwrite: save again keeps exactly one versioned record.
    if (saveAction) saveAction->trigger();
    QApplication::processEvents();
    check(QSettings(QStringLiteral("AeroSimWorkbenchFrameworkTest"),
                    QStringLiteral("UiLayoutFrameworkTest"))
              .value(QStringLiteral("UserLayout/version")).toInt() == 1,
          "save overwrites the single record");

    // (d) Restore last saved layout is safe.
    if (restoreAction) { restoreAction->setEnabled(true); restoreAction->trigger(); }
    QApplication::processEvents();

    // (e) Restore default must not delete the saved personal layout.
    if (defaultAction) defaultAction->trigger();
    QApplication::processEvents();
    check(QSettings(QStringLiteral("AeroSimWorkbenchFrameworkTest"),
                    QStringLiteral("UiLayoutFrameworkTest"))
              .value(QStringLiteral("UserLayout/version")).toInt() == 1,
          "restore-default keeps the saved user layout");

    // (f) A corrupt/corrupted-version record falls back safely (no crash).
    QSettings corrupt(QStringLiteral("AeroSimWorkbenchFrameworkTest"),
                      QStringLiteral("UiLayoutFrameworkTest"));
    corrupt.beginGroup(QStringLiteral("UserLayout"));
    corrupt.setValue(QStringLiteral("version"), 0);
    corrupt.setValue(QStringLiteral("geometry"), QByteArray("not-a-geometry"));
    corrupt.endGroup();
    corrupt.sync();
    if (restoreAction) { restoreAction->setEnabled(true); restoreAction->trigger(); }
    QApplication::processEvents();
    check(true, "corrupt record handled without crash");
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testConfig();
    testPageContainer(app);
    testMainWindow();

    QTextStream(stderr) << (failures == 0 ? "UiLayout framework tests passed"
                                          : "UiLayout framework tests FAILED")
                        << " (" << failures << " failure(s))\n";
    return failures == 0 ? 0 : 1;
}
