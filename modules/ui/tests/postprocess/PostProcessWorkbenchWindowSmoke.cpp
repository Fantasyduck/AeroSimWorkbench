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
#include <QImage>
#include <QListWidget>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
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
    CHECK(window.findChild<SARibbonCategory *>("postProcessToolsCategory"));
    CHECK(!action("postProcessCoefficientAction"));
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

    CHECK(window.windowTitle().contains(QString::fromUtf8("可视化分析台")));
    auto *sphere = window.findChild<QWidget *>("postProcessOrientationControl"); CHECK(sphere && sphere->isVisible());
    QString orientation;
    QObject::connect(&window, &aerosim::PostProcessWorkbenchWindow::viewRequested, &window, [&orientation](const QString &value) { orientation = value; });
    window.viewRequested(QStringLiteral("front")); flush();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(56,21), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(56,21), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(sphere, &press); QApplication::sendEvent(sphere, &release); CHECK(orientation=="top");
    flush(); const QImage beforeDrag = sphere->grab().toImage();
    QString dragCommand;
    QObject::connect(&window, &aerosim::PostProcessWorkbenchWindow::postProcessCommandRequested, &window, [&dragCommand](const QString &value) { dragCommand=value; });
    QMouseEvent dragPress(QEvent::MouseButtonPress, QPointF(56,56), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent dragMove(QEvent::MouseMove, QPointF(81,68), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent dragRelease(QEvent::MouseButtonRelease, QPointF(81,68), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(sphere,&dragPress); QApplication::sendEvent(sphere,&dragMove); QApplication::sendEvent(sphere,&dragRelease); flush();
    CHECK(dragCommand.startsWith("orientationDrag:") && sphere->grab().toImage()!=beforeDrag);
    CHECK(capture(sphere,"orientation-after-drag"));

    auto *ruler = action("postProcessRulerAction"); CHECK(ruler && ruler->isCheckable());
    ruler->trigger(); CHECK(window.findChild<QLabel *>("postProcessRulerPlaceholder")->isVisible());
    ruler->trigger(); CHECK(!window.findChild<QLabel *>("postProcessRulerPlaceholder")->isVisible());

    // Start empty; the example is explicitly requested.
    CHECK(tree->topLevelItemCount()==0 && layers->count()==0);
    CHECK(!action("postProcessContourAction")->isEnabled());
    CHECK(!window.findChild<QSlider *>("postProcessTimeSlider")->isEnabled());
    for (const char *name : {"postProcessFirstFrame", "postProcessPreviousFrame", "postProcessPlayFrame", "postProcessNextFrame", "postProcessLastFrame"})
        CHECK(!window.findChild<QToolButton *>(name)->isEnabled());
    action("postProcessImportAction")->trigger(); flush();
    auto *initialImport = window.findChild<QDialog *>("postProcessImportDataDialog"); CHECK(initialImport);
    CHECK(!initialImport->findChild<QComboBox *>("postProcessImportMode")->isVisible());
    auto *initialFiles = initialImport->findChild<QListWidget *>("postProcessImportFileList");
    auto *initialFile = new QListWidgetItem(QStringLiteral("initial.vti"), initialFiles);
    initialFile->setData(Qt::UserRole, QStringLiteral("/example/initial.vti"));
    initialImport->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(tree->topLevelItemCount()==1 && tree->topLevelItem(0)->data(0, Qt::UserRole+4).toString()=="initial");
    delete tree->takeTopLevelItem(0);
    action("postProcessMultiFileExampleAction")->trigger(); flush();
    CHECK(tree->topLevelItemCount()==2 && layers->count()==0);
    CHECK(tree->topLevelItem(0)->childCount()==1 && tree->topLevelItem(1)->childCount()==1);
    CHECK(tree->topLevelItem(0)->child(0)->child(0)->childCount()==3);
    CHECK(tree->topLevelItem(1)->child(0)->child(0)->childCount()==2);
    CHECK(tree->topLevelItem(0)->data(0, Qt::UserRole+3).toMap().value("sourceId") != tree->topLevelItem(1)->data(0, Qt::UserRole+3).toMap().value("sourceId"));
    CHECK(!window.findChild<QSlider *>("postProcessTimeSlider")->isEnabled());
    tree->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
    CHECK(tree->topLevelItem(1)->checkState(0)==Qt::Checked);
    CHECK(capture(&window,"multi-file-example"));
    // A file change must replace the available regions and fields together.
    auto *fileSourceAction = window.findChild<QAction *>("postProcessNew_contour"); CHECK(fileSourceAction);
    fileSourceAction->trigger(); flush();
    auto *fileSourceDialog = window.findChild<QDialog *>("postProcessFeatureSourceDialog"); CHECK(fileSourceDialog);
    auto *sourceFiles = fileSourceDialog->findChild<QComboBox *>("postProcessFeatureFile");
    auto *sourceRegions = fileSourceDialog->findChild<QComboBox *>("postProcessFeatureSource");
    auto *sourceFields = fileSourceDialog->findChild<QComboBox *>("postProcessFeatureField");
    CHECK(sourceFiles && sourceFiles->count()==2 && sourceRegions && sourceRegions->count()==1);
    CHECK(sourceFiles->currentText().contains("flowfield.cgns"));
    CHECK(sourceFields->findText(QString::fromUtf8("温度 [K]"))>=0);
    sourceFiles->setCurrentIndex(1); flush();
    CHECK(sourceRegions->count()==1 && sourceFiles->currentText().contains("surface.vtu"));
    CHECK(sourceFields->findText(QString::fromUtf8("温度 [K]"))<0);
    CHECK(sourceFields->findText(QString::fromUtf8("马赫数 [-]"))>=0);
    CHECK(capture(fileSourceDialog,"file-region-field-selection"));
    fileSourceDialog->reject(); flush();

    action("postProcessUnstructuredExampleAction")->trigger(); flush();
    CHECK(tree->topLevelItemCount()==2 && layers->count()==3);
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
        auto *item = tree->topLevelItem(0)->child(0)->child(0)->child(index);
        tree->setCurrentItem(item); tree->itemClicked(item,0); flush();
    };
    CHECK(!action("postProcessSequenceAction"));
    CHECK(action("postProcessRotationCenterAction"));

    // Panel commands must be direct clickable ribbon buttons.
    window.ribbonBar()->setCurrentIndex(2); flush();
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
        CHECK(action("postProcessStreamAction")->isEnabled());
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
    auto *rangeMode = createdDialog->findChild<QComboBox *>("rangeMode"); CHECK(rangeMode);
    auto *rangeMin = createdDialog->findChild<QDoubleSpinBox *>("rangeMin");
    auto *rangeMax = createdDialog->findChild<QDoubleSpinBox *>("rangeMax");
    CHECK(rangeMin && rangeMax && !rangeMin->isEnabled() && !rangeMax->isEnabled());
    rangeMode->setCurrentIndex(1); CHECK(rangeMin->isEnabled() && rangeMax->isEnabled());
    rangeMin->setValue(2); rangeMax->setValue(1);
    CHECK(!createdDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(!createdDialog->findChild<QPushButton *>("postProcessApplyDraft")->isEnabled());
    rangeMax->setValue(3);
    CHECK(createdDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
    CHECK(capture(createdDialog,"color-range-settings"));

    createdDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(action("postProcessSliceAction")->isEnabled() && action("postProcessIsoAction")->isEnabled());
    CHECK(!action("postProcessStreamAction")->isEnabled());
    chooseField(4); CHECK(action("postProcessStreamAction")->isEnabled());
    // Integration vector remains fixed when coloring a streamline by another scalar.
    action("postProcessStreamAction")->trigger(); flush();
    auto *streamSource = window.findChild<QDialog *>("postProcessFeatureSourceDialog"); CHECK(streamSource);
    auto *streamFields = streamSource->findChild<QComboBox *>("postProcessFeatureField"); CHECK(streamFields);
    CHECK(streamFields->findData(QStringLiteral("__components__")) >= 0);
    streamFields->setCurrentIndex(streamFields->findData(QStringLiteral("__components__")));
    auto *componentU = streamSource->findChild<QComboBox *>("postProcessVectorU");
    auto *componentV = streamSource->findChild<QComboBox *>("postProcessVectorV");
    auto *componentW = streamSource->findChild<QComboBox *>("postProcessVectorW");
    CHECK(componentU && componentV && componentW && componentU->isEnabled());
    auto *next = streamSource->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    componentV->setCurrentIndex(componentU->currentIndex()); CHECK(!next->isEnabled());
    componentV->setCurrentIndex(1); CHECK(next->isEnabled()); next->click(); flush();
    createdDialog = window.findChild<QDialog *>("postProcessParameterDialog"); CHECK(createdDialog);
    CHECK(layers->currentItem()->data(Qt::UserRole+3).toMap().value("vectorComponents").toStringList().size()==3);
    createdDialog->reject(); flush();
    chooseField(4); action("postProcessStreamAction")->trigger(); flush();
    streamSource = window.findChild<QDialog *>("postProcessFeatureSourceDialog"); CHECK(streamSource);
    streamSource->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
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
    auto *organization = window.findChild<QDialog *>("postProcessImportDataDialog"); CHECK(organization);
    auto *inputFiles = organization->findChild<QListWidget *>("postProcessImportFileList"); CHECK(inputFiles);
    auto *first = new QListWidgetItem(QStringLiteral("result.cgns"), inputFiles); first->setData(Qt::UserRole, QStringLiteral("/example/result.cgns"));
    auto *secondFile = new QListWidgetItem(QStringLiteral("boundary.vtu"), inputFiles); secondFile->setData(Qt::UserRole, QStringLiteral("/example/boundary.vtu"));
    auto *mode = organization->findChild<QComboBox *>("postProcessImportMode"); CHECK(mode && mode->isVisible());
    CHECK(mode->itemText(0)==QString::fromUtf8("追加显示") && mode->itemText(1)==QString::fromUtf8("替换现有数据"));
    organization->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(requestedPaths.size()==2 && tree->topLevelItemCount()==3);
    CHECK(tree->topLevelItem(2)->data(0, Qt::UserRole+4).toString()=="append");
    CHECK(!action("postProcessContourAction")->isEnabled());
    action("postProcessImportAction")->trigger(); flush();
    organization = window.findChild<QDialog *>("postProcessImportDataDialog"); CHECK(organization);
    mode = organization->findChild<QComboBox *>("postProcessImportMode"); mode->setCurrentIndex(1);
    inputFiles = organization->findChild<QListWidget *>("postProcessImportFileList");
    auto *replacement = new QListWidgetItem(QStringLiteral("new.vtk"), inputFiles); replacement->setData(Qt::UserRole, QStringLiteral("/example/new.vtk"));
    organization->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
    CHECK(requestedPaths.size()==1 && tree->topLevelItemCount()==4);
    CHECK(tree->topLevelItem(3)->data(0, Qt::UserRole+4).toString()=="replace");
    CHECK(tree->topLevelItem(0)->childCount()>0); // Existing data stays until reading succeeds.
    delete tree->takeTopLevelItem(3); delete tree->takeTopLevelItem(2);
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
        CHECK(fields->isVisible() == (QString::fromLatin1(kind) != QStringLiteral("slice")));
        CHECK(sourceDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
        sourceDialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
        auto *setup = window.findChild<QDialog *>("postProcessParameterDialog");
        CHECK(setup && setup->isVisible() && layers->count() == before + 1);
        const QString objectKind = QString::fromLatin1(kind);
        if (objectKind == "isoline") {
            auto *mode = setup->findChild<QComboBox *>("levelMode"); CHECK(mode);
            mode->setCurrentIndex(1); flush();
            auto *lo = setup->findChild<QDoubleSpinBox *>("levelMin");
            auto *hi = setup->findChild<QDoubleSpinBox *>("levelMax"); CHECK(lo && hi && lo->isVisible());
            hi->setValue(lo->value()); CHECK(!setup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
            hi->setValue(lo->value()+1); CHECK(setup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
        }
        if (objectKind == "trace") {
            auto *type = setup->findChild<QComboBox *>("traceType"); CHECK(type);
            auto *count = setup->findChild<QDoubleSpinBox *>("releaseCount"); CHECK(count && !count->isVisible());
            type->setCurrentIndex(1); flush(); CHECK(count->isVisible());
            CHECK(!setup->findChild<QComboBox *>("direction"));
        }
        if (objectKind == "stream") {
            auto *mode = setup->findChild<QComboBox *>("seedMode"); CHECK(mode);
            mode->setCurrentIndex(1); flush();
            CHECK(!setup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
            setup->findChild<QDoubleSpinBox *>("seedEndX")->setValue(1);
            CHECK(setup->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
        }
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
    auto *normalMode = dialog->findChild<QComboBox *>("normal"); CHECK(normalMode);
    CHECK(dialog->findChild<QDoubleSpinBox *>("offset")->isVisible());
    CHECK(!dialog->findChild<QDoubleSpinBox *>("normalX")->isVisible());
    normalMode->setCurrentIndex(3); flush();
    CHECK(!dialog->findChild<QDoubleSpinBox *>("offset")->isVisible());
    dialog->findChild<QDoubleSpinBox *>("normalX")->setValue(0);
    CHECK(!dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
    dialog->findChild<QDoubleSpinBox *>("normalX")->setValue(1);
    CHECK(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
    normalMode->setCurrentIndex(0);
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
        if (i==1) {
            auto *extra = dialog->findChild<QLineEdit *>("additionalValues"); CHECK(extra);
            extra->setText("1,2,3"); CHECK(!dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
            extra->setText("1,2"); CHECK(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
        } else {
            auto *seedMode = dialog->findChild<QComboBox *>("seedMode"); CHECK(seedMode);
            CHECK(!dialog->findChild<QDoubleSpinBox *>("seedEndX")->isVisible());
            seedMode->setCurrentIndex(1); flush(); CHECK(dialog->findChild<QDoubleSpinBox *>("seedEndX")->isVisible());
        }
        CHECK(capture(dialog, i==1 ? "iso-parameters" : "stream-parameters"));
        dialog->reject(); flush();
    }
    nav->setCurrentIndex(0);
    for (int group : {1}) {
        auto *item = tree->topLevelItem(0)->child(0)->child(0)->child(0); tree->setCurrentItem(item); tree->itemClicked(item,0);
        parameters->trigger(); flush(); dialog = window.findChild<QDialog *>("postProcessParameterDialog");
        CHECK(dialog && capture(dialog,group==1 ? "field-settings" : "curve-settings"));
        dialog->reject(); flush();
    }
    tree->setCurrentItem(tree->topLevelItem(1)->child(2));
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
        for (int category=0; category<3; ++category) {
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
