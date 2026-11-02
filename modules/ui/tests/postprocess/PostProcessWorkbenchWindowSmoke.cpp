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
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
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

    // Empty startup and explicit UI fixtures: never treat sample metadata as a reader.
    CHECK(tree->topLevelItemCount()==0 && layers->count()==0);
    for (const char *name : {"postProcessContourAction","postProcessSliceAction","postProcessIsoAction","postProcessStreamAction","postProcessExportAction"})
        CHECK(!action(name)->isEnabled());
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

    auto *fields = window.findChild<QComboBox *>("postProcessFieldSelector"); CHECK(fields);
    // Same UI capabilities for structured and unstructured topology.
    for (const char *example : {"postProcessStructuredExampleAction","postProcessUnstructuredExampleAction"}) {
        action(example)->trigger(); flush(); chooseZone(0);
        CHECK(action("postProcessSliceAction")->isEnabled() && action("postProcessIsoAction")->isEnabled());
        CHECK(!action("postProcessStreamAction")->isEnabled());
        fields->setCurrentIndex(4); // Three-component velocity, not speed magnitude.
        CHECK(action("postProcessStreamAction")->isEnabled() && !action("postProcessIsoAction")->isEnabled());
        CHECK(action("postProcessSliceAction")->isEnabled());
        fields->setCurrentIndex(1); // Speed magnitude is scalar.
        CHECK(!action("postProcessStreamAction")->isEnabled() && action("postProcessIsoAction")->isEnabled());
        chooseZone(1);
        CHECK(action("postProcessIsoLineAction")->isEnabled() && !action("postProcessIsoAction")->isEnabled());
        CHECK(!action("postProcessSliceAction")->isEnabled());
        chooseObject(0); // A slice retains its source fields but has surface dimension.
        CHECK(action("postProcessIsoLineAction")->isEnabled() && !action("postProcessIsoAction")->isEnabled());
        CHECK(!action("postProcessGeometryExportAction")->isEnabled()); // UI drafts are not geometry.
    }
    chooseZone(0); action("postProcessContourAction")->trigger(); flush();
    CHECK(nav->currentIndex()==2 && parameters->isEnabled());
    CHECK(action("postProcessSliceAction")->isEnabled() && action("postProcessIsoAction")->isEnabled());
    CHECK(!action("postProcessStreamAction")->isEnabled());
    fields->setCurrentIndex(4); CHECK(action("postProcessStreamAction")->isEnabled());
    // Integration vector remains fixed when coloring a streamline by another scalar.
    action("postProcessStreamAction")->trigger(); flush();
    CHECK(layers->currentItem()->data(Qt::UserRole+3).toMap().value("integrationField").toString().contains(QString::fromUtf8("速度矢量")));
    fields->setCurrentIndex(0);
    CHECK(layers->currentItem()->data(Qt::UserRole+3).toMap().value("integrationField").toString().contains(QString::fromUtf8("速度矢量")));
    action("postProcessUnstructuredExampleAction")->trigger(); flush();

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
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click(); flush();
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
    CHECK(window.findChildren<QDialog *>().isEmpty());

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
