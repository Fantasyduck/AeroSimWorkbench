#include "aerosim/ui/postprocess/PostProcessWorkbenchWindow.h"
#include <SARibbonBar.h>
#include <SARibbonCategory.h>
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
#include <QStandardPaths>
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
    CHECK(tree && layers && nav && content && style && parameters && probe && results && select && pan);
    CHECK(!action("postProcessPanelsAction"));
    CHECK(content->count() == 2);
    CHECK(window.findChild<SARibbonCategory *>("postProcessOutputCategory"));
    CHECK(!window.findChild<SARibbonCategory *>("postProcessAnalysisCategory"));
    CHECK(!action("postProcessMeshAction"));
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
        nav->setCurrentIndex(0);
        auto *item = tree->topLevelItem(0)->child(index);
        tree->setCurrentItem(item); tree->itemClicked(item,0); flush();
    };
    const auto chooseObject = [&] (int index) {
        nav->setCurrentIndex(1); layers->setCurrentRow(index); layers->itemClicked(layers->currentItem()); flush();
    };
    window.show(); flush();
    for (auto *dock : window.findChildren<QDockWidget *>()) CHECK(!dock->isVisible());
    for (auto *toolbar : window.findChildren<QToolBar *>()) CHECK(window.toolBarArea(toolbar) == Qt::NoToolBarArea);

    // Start empty; the example is explicitly requested.
    CHECK(tree->topLevelItemCount()==0 && layers->count()==0);
    CHECK(!action("postProcessContourAction")->isEnabled());
    CHECK(!window.findChild<QSlider *>("postProcessTimeSlider")->isEnabled());
    for (const char *name : {"postProcessFirstFrame", "postProcessPreviousFrame", "postProcessPlayFrame", "postProcessNextFrame", "postProcessLastFrame"})
        CHECK(!window.findChild<QToolButton *>(name)->isEnabled());
    action("postProcessUnstructuredExampleAction")->trigger(); flush();
    CHECK(tree->topLevelItemCount()==3 && layers->count()==3);
    auto *play = window.findChild<QToolButton *>("postProcessPlayFrame");
    auto *playTimer = window.findChild<QTimer *>("postProcessPlayTimer");
    CHECK(play && playTimer && !playTimer->isActive());
    play->click(); CHECK(playTimer->isActive());
    play->click(); CHECK(!playTimer->isActive());
    CHECK(action("postProcessContourAction")->isEnabled());
    CHECK(!action("postProcessExportAction") && !action("postProcessGeometryExportAction"));
    CHECK(!action("postProcessConvertAction") && !action("postProcessExcelAction"));
    auto *host = window.findChild<QWidget *>("postProcessViewportHost");
    CHECK(host->property("backgroundBottom").value<QColor>()==QColor(255,255,255));
    CHECK(capture(&window,"empty-session"));
    const auto backgroundDialog = [&] {
        action("postProcessBackgroundAction")->trigger(); flush();
        return window.findChild<QDialog *>("postProcessBackgroundDialog");
    };
    auto *bg = backgroundDialog(); CHECK(bg);
    bg->findChild<QComboBox *>("postProcessBackgroundPreset")->setCurrentIndex(4);
    bg->reject(); flush();
    CHECK(host->property("backgroundBottom").value<QColor>()==QColor(255,255,255));
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
    CHECK(host->property("gradientBackground").toBool());
    CHECK(host->property("backgroundBottom").value<QColor>()==QColor(255,255,255));

    CHECK(!window.findChild<QComboBox *>("postProcessFieldSelector"));
    const auto chooseField = [&] (int index) {
        nav->setCurrentIndex(0);
        auto *item = tree->topLevelItem(1)->child(index);
        tree->setCurrentItem(item); tree->itemClicked(item,0); flush();
    };
    CHECK(!action("postProcessSequenceAction"));
    CHECK(action("postProcessRotationCenterAction"));

    // Panel commands must be direct clickable ribbon buttons.
    window.ribbonBar()->setCurrentIndex(4); flush();
    for (const char *name : {"postProcessShowDataPanel", "postProcessShowObjectPanel", "postProcessProbeResultsAction"}) {
        QToolButton *button = nullptr;
        for (auto *candidate : window.findChildren<QToolButton *>()) {
            if (candidate->defaultAction() == action(name) && candidate->isVisible()) { button = candidate; break; }
        }
        CHECK(button && button->isEnabled());
        button->click(); flush();
    }
    auto *directProbe = window.findChild<QDockWidget *>("postProcessProbeDock");
    CHECK(directProbe && directProbe->isVisible()); directProbe->hide();
    CHECK(!action("postProcessTasksAction"));
    CHECK(!window.findChild<QDockWidget *>("postProcessMessageDock"));

    // Data and object panels can be closed and reopened independently.
    CHECK(nav->count()==2 && !action("postProcessShowViewPanel"));
    emit nav->tabCloseRequested(0); CHECK(nav->count()==1);
    action("postProcessShowDataPanel")->trigger(); CHECK(nav->count()==2 && nav->currentIndex()==0);
    emit nav->tabCloseRequested(1); emit nav->tabCloseRequested(0);
    CHECK(nav->count()==0 && !nav->isVisible());
    action("postProcessShowObjectPanel")->trigger(); CHECK(nav->count()==1 && nav->isVisible());
    action("postProcessShowDataPanel")->trigger();
    CHECK(nav->count()==2 && nav->currentIndex()==0);
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
    }
    chooseZone(0); action("postProcessContourAction")->trigger(); flush();
    CHECK(nav->currentIndex()==1 && parameters->isEnabled());
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
    auto *organization = window.findChild<QDialog *>("postProcessResultSetDialog"); CHECK(organization);
    organization->findChild<QLineEdit *>("resultSetName")->setText(QStringLiteral("case"));
    organization->findChild<QLineEdit *>("resultSetPath0")->setText(QStringLiteral("/example/result.cgns"));
    organization->findChild<QLineEdit *>("resultSetPath1")->setText(QStringLiteral("/example/boundary.tec"));
    organization->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(requestedPaths.size()==2 && tree->topLevelItemCount()==4);
    CHECK(!action("postProcessContourAction")->isEnabled());
    action("postProcessImportAction")->trigger(); flush();
    organization = window.findChild<QDialog *>("postProcessResultSetDialog"); CHECK(organization);
    organization->findChild<QComboBox *>("resultSetTarget")->setCurrentIndex(1);
    organization->findChild<QLineEdit *>("resultSetPath2")->setText(QStringLiteral("/example/force.csv"));
    organization->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(requestedPaths.size()==1 && tree->topLevelItem(3)->childCount()==3);
    action("postProcessCoefficientAction")->trigger(); flush();
    auto *coefficient = window.findChild<QDialog *>("postProcessCoefficientDialog"); CHECK(coefficient);
    CHECK(coefficient->findChild<QTableWidget *>("postProcessCoefficientTable")->rowCount()==0);
    coefficient->accept(); flush();
    delete tree->takeTopLevelItem(3);
    QFileDialog *fileDialog = nullptr;
    // Each output command opens its own settings dialog, without creating a file.
    chooseZone(0);
    for (const char *name : {"postProcessImageExportAction", "postProcessAnimationExportAction"}) {
        CHECK(action(name)->isEnabled()); action(name)->trigger(); flush();
        auto *output = window.findChild<QDialog *>("postProcessExportSetupDialog"); CHECK(output);
        CHECK(output->findChild<QComboBox *>("postProcessExportFormat")); output->reject(); flush();
    }
    chooseZone(0); action("postProcessImageExportAction")->trigger(); flush();
    auto *exportSetup = window.findChild<QDialog *>("postProcessExportSetupDialog"); CHECK(exportSetup);
    CHECK(capture(exportSetup,"image-export-setup"));
    exportSetup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    fileDialog = window.findChild<QFileDialog *>("postProcessExportDialog"); CHECK(fileDialog);
    const QString outputDir = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(QString::fromUtf8("AeroSimWorkbench/后处理"));
    CHECK(QDir::cleanPath(fileDialog->directory().absolutePath()) == QDir::cleanPath(outputDir));
    fileDialog->fileSelected(QStringLiteral("/example/output.png"));
    CHECK(exportKind=="image" && exportPath=="/example/output.png");
    flush();
    chooseObject(0); parameters->trigger(); flush();
    auto *applyDialog = window.findChild<QDialog *>("postProcessParameterDialog"); CHECK(applyDialog);
    auto *applyButton = applyDialog->findChild<QPushButton *>("postProcessApplyDraft");
    CHECK(applyButton); applyButton->click(); CHECK(appliedCommand=="apply:slice");
    applyDialog->reject(); flush();
    action("postProcessUnstructuredExampleAction")->trigger(); flush();

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
    CHECK(dialog->findChild<QComboBox *>("objectStyleRepresentation"));
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
    nav->setCurrentIndex(0);
    for (int group : {1}) {
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
    CHECK(!window.findChild<QDockWidget *>("postProcessProbeDock")->isVisible());
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

    for (const QSize size : {QSize(1120,680), QSize(1440,960)}) {
        window.resize(size);
        for (int category=0; category<5; ++category) {
            window.ribbonBar()->setCurrentIndex(category); nav->setCurrentIndex(category%2); flush();
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
