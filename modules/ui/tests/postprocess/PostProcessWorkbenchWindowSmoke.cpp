#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include <SARibbonBar.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QTimer>
#include <QTreeWidget>

#define CHECK(condition) do { if (!(condition)) { qCritical() << "Failed at line" << __LINE__ << #condition; return 1; } } while (false)

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc,argv);
    aerosim::PostProcessWorkbenchWindow window;
    const auto action = [&window](const char *name) { return window.findChild<QAction *>(QString::fromLatin1(name)); };
    auto *tree = window.findChild<QTreeWidget *>("postProcessDatasetTree");
    auto *layers = window.findChild<QListWidget *>("sceneLayerList");
    auto *nav = window.findChild<QTabWidget *>("postProcessNavigator");
    auto *content = window.findChild<QSplitter *>("postProcessContent");
    auto *style = action("postProcessObjectStyleAction");
    auto *parameters = action("postProcessParametersAction");
    auto *probe = action("postProcessProbeAction");
    auto *results = action("postProcessProbeResultsAction");
    auto *select = action("postProcessSelectAction");
    auto *pan = action("postProcessPanAction");
    auto *tasks = action("postProcessTasksAction");
    CHECK(tree && layers && nav && content && style && parameters && probe && results && select && pan && tasks);
    CHECK(content->count() == 2);
    CHECK(!window.findChild<QWidget *>("postProcessInspectorTabs"));
    CHECK(!window.findChild<QDockWidget *>("postProcessProbeDock"));
    CHECK(QFile::exists(":/SARibbonTheme/resource/palettes/office2013-default.json"));
    const QString captureDir = qEnvironmentVariable("AEROSIM_UI_CAPTURE_DIR");
    if (!captureDir.isEmpty()) QDir().mkpath(captureDir);
    const auto flush = [&app] { app.processEvents(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); app.processEvents(); };
    const auto capture = [&captureDir, &flush](QWidget *widget, const QString &name) {
        flush();
        return captureDir.isEmpty() || widget->grab().save(captureDir + "/" + name + ".png");
    };
    const auto chooseZone = [&] (int index) {
        nav->setCurrentIndex(1);
        auto *item = tree->topLevelItem(0)->child(index);
        tree->setCurrentItem(item); tree->itemClicked(item,0); flush();
    };
    const auto chooseObject = [&] (int index) {
        nav->setCurrentIndex(2); layers->setCurrentRow(index); layers->itemClicked(layers->currentItem()); flush();
    };
    window.show(); flush();
    for (auto *dock : window.findChildren<QDockWidget *>()) CHECK(!dock->isVisible());
    for (auto *toolbar : window.findChildren<QToolBar *>()) CHECK(window.toolBarArea(toolbar) == Qt::NoToolBarArea);

    // The startup sample is explicitly illustrative, not a reader result.
    CHECK(tree->topLevelItemCount()==4 && layers->count()==3);
    auto *play = window.findChild<QToolButton *>("postProcessPlayFrame");
    auto *playTimer = window.findChild<QTimer *>("postProcessPlayTimer");
    CHECK(play && playTimer && !playTimer->isActive());
    play->click(); CHECK(playTimer->isActive());
    play->click(); CHECK(!playTimer->isActive());
    CHECK(action("postProcessContourAction")->isEnabled());
    CHECK(action("postProcessExportAction")->isEnabled());
    CHECK(!action("postProcessConvertAction") && !action("postProcessExcelAction"));
    auto *host = window.findChild<QWidget *>("postProcessViewportHost");
    CHECK(host->property("backgroundBottom").value<QColor>()==QColor(98,93,90));
    CHECK(capture(&window,"empty-session"));
    const auto backgroundDialog = [&] {
        action("postProcessBackgroundAction")->trigger(); flush();
        return window.findChild<QDialog *>("postProcessBackgroundDialog");
    };
    auto *bg = backgroundDialog(); CHECK(bg);
    bg->findChild<QComboBox *>("postProcessBackgroundPreset")->setCurrentIndex(4);
    bg->reject(); flush();
    CHECK(host->property("backgroundBottom").value<QColor>()==QColor(98,93,90));
    bg=backgroundDialog();
    bg->findChild<QComboBox *>("postProcessBackgroundPreset")->setCurrentIndex(3);
    bg->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(host->property("gradientBackground").toBool());
    CHECK(host->property("backgroundBottom").value<QColor>()==QColor("#52576e"));
    bg=backgroundDialog();
    bg->findChild<QLineEdit *>("postProcessBackgroundBottom")->setText("invalid-color");
    bg->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(bg->isVisible());
    bg->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::RestoreDefaults)->click();
    CHECK(capture(bg,"background-settings"));
    bg->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(!host->property("gradientBackground").toBool());
    CHECK(host->property("backgroundBottom").value<QColor>()==QColor(98,93,90));

    CHECK(!window.findChild<QComboBox *>("postProcessFieldSelector"));
    const auto chooseField = [&] (int index) {
        nav->setCurrentIndex(1);
        auto *item = tree->topLevelItem(1)->child(index);
        tree->setCurrentItem(item); tree->itemClicked(item,0); flush();
    };
    // Close independently; menu actions reopen each panel in its original order.
    emit nav->tabCloseRequested(1); CHECK(nav->count()==2);
    action("postProcessShowDataPanel")->trigger(); CHECK(nav->count()==3 && nav->currentIndex()==1);
    emit nav->tabCloseRequested(2); emit nav->tabCloseRequested(1); emit nav->tabCloseRequested(0);
    CHECK(nav->count()==0 && !nav->isVisible());
    action("postProcessShowObjectPanel")->trigger(); CHECK(nav->count()==1 && nav->isVisible());
    action("postProcessShowViewPanel")->trigger(); action("postProcessShowDataPanel")->trigger();
    CHECK(nav->count()==3 && nav->currentIndex()==1);
    // Same UI capabilities for structured and unstructured topology.
    for (const char *example : {"postProcessStructuredExampleAction","postProcessUnstructuredExampleAction"}) {
        action(example)->trigger(); flush(); chooseZone(0);
        CHECK(action("postProcessSliceAction")->isEnabled() && action("postProcessIsoAction")->isEnabled());
        CHECK(!action("postProcessStreamAction")->isEnabled());
        chooseField(4); // Three-component velocity, not speed magnitude.
        CHECK(action("postProcessStreamAction")->isEnabled() && !action("postProcessIsoAction")->isEnabled());
        CHECK(action("postProcessSliceAction")->isEnabled());
        chooseField(1); // Speed magnitude is scalar.
        CHECK(!action("postProcessStreamAction")->isEnabled() && action("postProcessIsoAction")->isEnabled());
        chooseZone(1);
        CHECK(action("postProcessIsoLineAction")->isEnabled() && !action("postProcessIsoAction")->isEnabled());
        CHECK(!action("postProcessSliceAction")->isEnabled());
        chooseObject(0); // A slice retains its source fields but has surface dimension.
        CHECK(action("postProcessIsoLineAction")->isEnabled() && !action("postProcessIsoAction")->isEnabled());
        CHECK(action("postProcessGeometryExportAction")->isEnabled()); // Request only; no file is written.
    }
    chooseZone(0); action("postProcessContourAction")->trigger(); flush();
    CHECK(nav->currentIndex()==2 && parameters->isEnabled());
    auto *createdDialog = window.findChild<QDialog *>("postProcessParameterDialog"); CHECK(createdDialog);
    createdDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(action("postProcessSliceAction")->isEnabled() && action("postProcessIsoAction")->isEnabled());
    CHECK(!action("postProcessStreamAction")->isEnabled());
    chooseField(4); CHECK(action("postProcessStreamAction")->isEnabled());
    // Integration vector remains fixed when coloring a streamline by another scalar.
    action("postProcessStreamAction")->trigger(); flush();
    createdDialog = window.findChild<QDialog *>("postProcessParameterDialog"); CHECK(createdDialog);
    createdDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(layers->currentItem()->data(Qt::UserRole+3).toMap().value("integrationField").toString().contains(QString::fromUtf8("速度矢量")));
    CHECK(layers->currentItem()->data(Qt::UserRole+3).toMap().value("integrationField").toString().contains(QString::fromUtf8("速度矢量")));
    action("postProcessUnstructuredExampleAction")->trigger(); flush();

    // The main entry points submit UI requests without claiming a reader/exporter exists.
    QStringList requestedPaths;
    QString exportKind, exportPath, appliedCommand;
    QObject::connect(&window, &aerosim::PostProcessWorkbenchWindow::importFilesRequested, &window,
                     [&requestedPaths](const QStringList &paths) { requestedPaths = paths; });
    QObject::connect(&window, &aerosim::PostProcessWorkbenchWindow::exportFileRequested, &window,
                     [&exportKind, &exportPath](const QString &kind, const QString &path) { exportKind = kind; exportPath = path; });
    QObject::connect(&window, &aerosim::PostProcessWorkbenchWindow::postProcessCommandRequested, &window,
                     [&appliedCommand](const QString &command) { if (command.startsWith("apply:")) appliedCommand = command; });
    action("postProcessImportAction")->trigger(); flush();
    auto *fileDialog = window.findChild<QFileDialog *>("postProcessImportDialog");
    CHECK(fileDialog); fileDialog->reject(); flush(); CHECK(requestedPaths.isEmpty());
    action("postProcessImportAction")->trigger(); flush();
    fileDialog = window.findChild<QFileDialog *>("postProcessImportDialog"); CHECK(fileDialog);
    fileDialog->filesSelected(QStringList{QStringLiteral("/example/result.vtk")});
    CHECK(requestedPaths.size()==1 && tree->topLevelItemCount()==5);
    CHECK(!action("postProcessContourAction")->isEnabled()); // pending file is not a dataset
    fileDialog->reject(); flush();
    action("postProcessImportAction")->trigger(); flush();
    fileDialog = window.findChild<QFileDialog *>("postProcessImportDialog"); CHECK(fileDialog);
    fileDialog->filesSelected(QStringList{QStringLiteral("/example/a.vtk"), QStringLiteral("/example/b.vtk")});
    fileDialog->reject(); flush();
    auto *organization = window.findChild<QDialog *>("postProcessImportOrganizationDialog"); CHECK(organization);
    CHECK(capture(organization,"import-organization"));
    auto *importMode = organization->findChild<QComboBox *>("postProcessImportMode"); CHECK(importMode);
    importMode->setCurrentIndex(2);
    auto *fileTable = organization->findChild<QTableWidget *>("postProcessImportFiles"); CHECK(fileTable && fileTable->rowCount()==2);
    CHECK(fileTable->item(0,1)->text().isEmpty()); // File order is not a physical time.
    fileTable->item(0,1)->setText(QStringLiteral("1.5"));
    organization->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(requestedPaths.size()==2 && tree->topLevelItemCount()==6);
    chooseZone(0); action("postProcessExportAction")->trigger(); flush();
    auto *exportSetup = window.findChild<QDialog *>("postProcessExportSetupDialog"); CHECK(exportSetup);
    CHECK(capture(exportSetup,"data-export-setup"));
    exportSetup->findChild<QComboBox *>("postProcessExportFormat")->setCurrentIndex(1);
    exportSetup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    fileDialog = window.findChild<QFileDialog *>("postProcessExportDialog"); CHECK(fileDialog);
    fileDialog->fileSelected(QStringLiteral("/example/output.cgns"));
    CHECK(exportKind=="data" && exportPath=="/example/output.cgns");
    flush();
    chooseObject(0); parameters->trigger(); flush();
    auto *applyDialog = window.findChild<QDialog *>("postProcessParameterDialog"); CHECK(applyDialog);
    auto *applyButton = applyDialog->findChild<QPushButton *>("postProcessApplyDraft");
    CHECK(applyButton); applyButton->click(); CHECK(appliedCommand=="apply:slice");
    applyDialog->reject(); flush();
    action("postProcessUnstructuredExampleAction")->trigger(); flush();

    // Each analysis entry opens its own form and an explicitly empty result panel.
    for (int i=0; i<5; ++i) {
        auto *analysisAction = action(QStringLiteral("postProcessAnalysisAction%1").arg(i).toLatin1().constData());
        CHECK(analysisAction); analysisAction->trigger(); flush();
        auto *analysisDialog = window.findChild<QDialog *>("postProcessAnalysisDialog");
        CHECK(analysisDialog && analysisDialog->findChild<QLineEdit *>("analysisDataPath"));
        CHECK(capture(analysisDialog,QString("analysis-%1").arg(i)));
        analysisDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
        auto *resultDock = window.findChild<QDockWidget *>("postProcessAnalysisResultDock");
        CHECK(resultDock && resultDock->isVisible());
        CHECK(!resultDock->findChild<QTextEdit *>("postProcessAnalysisResult")->toPlainText().isEmpty());
        resultDock->hide();
    }

    // New-object commands choose a source independently of the current object.
    chooseObject(2); // A one-dimensional streamline cannot itself supply these operations.
    auto *newButton = window.findChild<QPushButton *>("postProcessNewObjectButton");
    CHECK(newButton && newButton->menu());
    CHECK(newButton->menu()->minimumWidth() >= newButton->menu()->fontMetrics().horizontalAdvance(QString::fromUtf8("等值面")) + 80);
    for (const char *kind : {"contour", "slice", "isoline", "iso", "stream", "trace"}) {
        auto *newAction = newButton->menu()->findChild<QAction *>(QStringLiteral("postProcessNew_%1").arg(QString::fromLatin1(kind)));
        CHECK(newAction && newAction->isEnabled());
        const int before = layers->count();
        newAction->trigger(); flush();
        auto *sourceDialog = window.findChild<QDialog *>("postProcessFeatureSourceDialog");
        CHECK(sourceDialog && sourceDialog->isVisible());
        auto *sources = sourceDialog->findChild<QComboBox *>("postProcessFeatureSource");
        auto *fields = sourceDialog->findChild<QComboBox *>("postProcessFeatureField");
        CHECK(sources && sources->count() > 0 && fields);
        CHECK(sourceDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
        sourceDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
        auto *setup = window.findChild<QDialog *>("postProcessParameterDialog");
        CHECK(setup && setup->isVisible() && layers->count() == before + 1);
        setup->reject(); flush(); CHECK(layers->count() == before);
    }

    // Merely selecting a zone or object must never open an editor or sidebar.
    chooseObject(0); chooseZone(0);
    CHECK(window.findChildren<QDialog *>().isEmpty());
    CHECK(style->isEnabled() && !parameters->isEnabled());

    // Editing is transactional and keyed by object identity, not the selected row.
    style->trigger(); flush();
    auto *dialog = window.findChild<QDialog *>("postProcessObjectStyleDialog");
    CHECK(dialog && dialog->isVisible());
    CHECK(capture(dialog,"object-style"));
    auto *opacity = dialog->findChild<QSpinBox *>("objectStyleOpacity");
    CHECK(opacity && opacity->value() == 100);
    opacity->setValue(25); dialog->reject(); flush();
    style->trigger(); flush(); dialog = window.findChild<QDialog *>("postProcessObjectStyleDialog");
    CHECK(dialog->findChild<QSpinBox *>("objectStyleOpacity")->value() == 100);
    dialog->findChild<QSpinBox *>("objectStyleOpacity")->setValue(60);
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    chooseZone(1); style->trigger(); flush(); dialog = window.findChild<QDialog *>("postProcessObjectStyleDialog");
    CHECK(dialog->findChild<QSpinBox *>("objectStyleOpacity")->value() == 100);
    dialog->reject(); flush(); chooseZone(0); style->trigger(); flush();
    dialog = window.findChild<QDialog *>("postProcessObjectStyleDialog");
    CHECK(dialog->findChild<QSpinBox *>("objectStyleOpacity")->value() == 60);
    dialog->reject(); flush();
    tree->itemDoubleClicked(tree->currentItem(),0); flush();
    CHECK(window.findChild<QDialog *>("postProcessObjectStyleDialog"));
    window.findChild<QDialog *>("postProcessObjectStyleDialog")->reject(); flush();

    chooseObject(0);
    layers->itemDoubleClicked(layers->currentItem()); flush();
    dialog = window.findChild<QDialog *>("postProcessParameterDialog");
    CHECK(dialog && dialog->findChild<QDoubleSpinBox *>("offset"));
    CHECK(!dialog->findChild<QSpinBox *>("objectStyleOpacity"));
    CHECK(capture(dialog,"slice-parameters"));
    dialog->findChild<QDoubleSpinBox *>("offset")->setValue(1.25);
    auto *sliceField = dialog->findChild<QComboBox *>("displayField");
    CHECK(sliceField && sliceField->count() > 3); sliceField->setCurrentIndex(3);
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(layers->currentItem()->data(Qt::UserRole+3).toMap().value("field").toString().contains(QString::fromUtf8("温度")));
    parameters->trigger(); flush(); dialog = window.findChild<QDialog *>("postProcessParameterDialog");
    CHECK(dialog->findChild<QDoubleSpinBox *>("offset")->value() == 1.25);
    dialog->reject(); flush();
    for (int i : {1,2}) {
        chooseObject(i); parameters->trigger(); flush();
        dialog = window.findChild<QDialog *>("postProcessParameterDialog");
        CHECK(dialog && dialog->findChild<QDoubleSpinBox *>(i==1 ? "isoValue" : "seedX"));
        CHECK(capture(dialog, i==1 ? "iso-parameters" : "stream-parameters"));
        dialog->reject(); flush();
    }
    nav->setCurrentIndex(1);
    for (int group : {1,3}) {
        auto *item = tree->topLevelItem(group)->child(0); tree->setCurrentItem(item); tree->itemClicked(item,0);
        parameters->trigger(); flush(); dialog = window.findChild<QDialog *>("postProcessParameterDialog");
        CHECK(dialog && capture(dialog,group==1 ? "field-settings" : "curve-settings"));
        dialog->reject(); flush();
    }
    tree->setCurrentItem(tree->topLevelItem(2)->child(2));
    CHECK(window.findChild<QSlider *>("postProcessTimeSlider")->value() == 2);
    CHECK(!style->isEnabled() && !parameters->isEnabled());
    chooseZone(0);
    const int count = layers->count(); action("postProcessContourAction")->trigger();
    CHECK(layers->count() == count+1);
    createdDialog = window.findChild<QDialog *>("postProcessParameterDialog"); CHECK(createdDialog);
    createdDialog->reject(); flush(); CHECK(layers->count() == count);
    action("postProcessContourAction")->trigger(); flush();
    createdDialog = window.findChild<QDialog *>("postProcessParameterDialog"); CHECK(createdDialog);
    createdDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(layers->count() == count+1 && window.findChildren<QDialog *>().isEmpty());

    // Probe is an exclusive mouse mode; results visibility is a separate action.
    pan->trigger(); CHECK(pan->isChecked());
    probe->trigger(); CHECK(probe->isChecked() && !pan->isChecked());
    CHECK(!window.findChild<QDockWidget *>("postProcessProbeDock"));
    CHECK(window.findChild<QWidget *>("postProcessViewportHost")->cursor().shape() == Qt::CrossCursor);
    results->trigger(); flush();
    auto *probeDock = window.findChild<QDockWidget *>("postProcessProbeDock");
    CHECK(probeDock && probeDock->isVisible() && probe->isChecked());
    CHECK(capture(&window,"probe-results-empty"));
    probeDock->hide(); CHECK(probe->isChecked());
    select->trigger(); CHECK(select->isChecked() && !probe->isChecked());
    CHECK(window.findChild<QWidget *>("postProcessViewportHost")->cursor().shape() == Qt::ArrowCursor);
    results->trigger(); CHECK(select->isChecked() && !probe->isChecked());
    probeDock->hide();
    // Presentation fixture only: no simulated picking or numerical computation.
    window.displayProbeResult(QStringLiteral("UI presentation test fixture: node 7, P = 42"));
    CHECK(probeDock->isVisible() && select->isChecked());
    CHECK(window.findChild<QTextEdit *>("postProcessProbeResult")->toPlainText().contains("node 7"));
    probeDock->hide(); results->trigger();
    CHECK(window.findChild<QTextEdit *>("postProcessProbeResult")->toPlainText().contains("node 7"));
    probeDock->hide();
    tasks->trigger(); flush();
    auto *taskDock = window.findChild<QDockWidget *>("postProcessMessageDock");
    CHECK(taskDock && taskDock->isVisible() && capture(&window,"tasks")); taskDock->hide();

    for (const QSize size : {QSize(1120,680), QSize(1440,960)}) {
        window.resize(size);
        for (int category=0; category<5; ++category) {
            window.ribbonBar()->setCurrentIndex(category); nav->setCurrentIndex(category%3); flush();
            CHECK(window.size() == size);
            for (auto *button : window.findChildren<QToolButton *>()) {
                if (button->isVisible() && button->objectName().startsWith("postProcess"))
                    CHECK(button->width() >= button->iconSize().width()+8 && button->height() >= button->iconSize().height()+8);
            }
            CHECK(capture(&window,QString("workspace-%1x%2-%3").arg(size.width()).arg(size.height()).arg(category)));
        }
    }
    action("postProcessHarnessAction")->trigger(); flush();
    CHECK(capture(&window,"harness"));
    for (auto *caption : window.findChildren<QLabel *>("postProcessDockCaption")) {
        if (caption->isVisible()) CHECK(caption->height() >= caption->sizeHint().height() && caption->width() >= caption->sizeHint().width());
    }
    for (auto *command : window.findChildren<QAction *>()) {
        if (command->objectName().startsWith("postProcess")) CHECK(!command->icon().isNull());
    }
    auto *viewer = new QLabel("Viewer"); CHECK(window.insertWidget(aerosim::WidgetSlot::Viewport,viewer));
    auto *second = new QLabel("Second viewer"); CHECK(!window.insertWidget(aerosim::WidgetSlot::Viewport,second)); delete second;
    return 0;
}
