#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/monitor/ComputeMonitorPage.h"
#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

using namespace aerosim;

class CurveReceiver final : public MonitorCurveWidget {
public:
    QString task;
    QString attempt;
    QVector<MonitorSample> samples;
    void replaceTask(const QString &id, const QString &run, const QVector<MonitorSample> &history) override {
        task = id; attempt = run; samples = history;
    }
    void appendSample(const MonitorSample &sample) override { samples.append(sample); }
};

class MonitorTaskService final : public ITaskService {
public:
    QHash<QString, TaskRecord> records;
    TaskRecord record(const QString &id) const override { return records.value(id); }
    void handleTaskCommand(const TaskCommand &) override {}
};

class MainMonitorTest final : public QObject {
    Q_OBJECT
    QTemporaryDir preferences_;
    TaskRecord record(const QString &id, const QString &batch = {}) {
        TaskRecord r; r.taskId = id; r.batchId = batch; r.caseId = "Case_" + id;
        r.attemptId = "attempt-1"; r.status = TaskStatus::Preparing; return r;
    }
    TaskEvent event(const QString &id, quint64 sequence, const QString &message) {
        TaskEvent e; e.taskId = id; e.attemptId = "attempt-1"; e.sequence = sequence;
        e.status = TaskStatus::Running; e.type = TaskEventType::ProgressUpdated;
        e.progress.currentStep = sequence; e.progress.totalSteps = 100;
        e.message = message; return e;
    }
    QTreeWidgetItem *node(QTreeWidget *tree, const QString &id) {
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole + 1).toString() == id) return *it;
        return nullptr;
    }
    void click(QTreeWidget *tree, QTreeWidgetItem *item) {
        QVERIFY(item);
        for (auto *p = item->parent(); p; p = p->parent()) p->setExpanded(true);
        QApplication::processEvents();
        tree->scrollToItem(item);
        QApplication::processEvents();
        QVERIFY(tree->viewport()->rect().contains(tree->visualItemRect(item).center()));
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(item).center());
    }
private slots:
    void initTestCase() {
        QVERIFY(preferences_.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences_.path());
    }
    void unifiedNavigationAndTelemetry() {
        MainWindow window; window.show();
        auto *page = window.monitorPage();
        auto *tree = window.findChild<QTreeWidget *>("workspaceTree");
        auto *workspace = window.findChild<QStackedWidget *>("centralWorkspace");
        QCOMPARE(node(tree, "single-runs")->childCount(), 0);
        QCOMPARE(node(tree, "batch-runs")->childCount(), 0);
        for (const QString id : {"Run_001", "Task_001", "Task_002"})
            page->upsertTask(record(id, id.startsWith("Task") ? "BatchRun_001" : ""),
                             {{"Mach", id == "Run_001" ? 0.6 : 1.2}});
        auto *curve = new CurveReceiver;
        QVERIFY(page->insertCurveWidget(curve));
        auto *log = page->findChild<QPlainTextEdit *>("monitorTaskLog");
        auto *params = page->findChild<QTableWidget *>("monitorParameters");
        auto *propertyDock = window.findChild<QDockWidget *>("propertyDock");
        QVERIFY(!propertyDock->isHidden());
        click(tree, node(tree, "Run_001"));
        auto *sharedWorkspace = workspace->currentWidget();
        QCOMPARE(sharedWorkspace->objectName(), QString("monitorWorkspace"));
        QVERIFY(page->findChild<QTreeWidget *>("monitorTaskTree")->isHidden());
        QVERIFY(propertyDock->isHidden());
        QCOMPARE(page->selectedTaskId(), QString("Run_001"));
        QCOMPARE(curve->task, QString("Run_001"));
        QCOMPARE(params->item(0, 1)->text(), QString("0.6"));
        window.displayTaskEvent(event("Run_001", 1, "single-log"));
        QVERIFY(log->toPlainText().contains("single-log"));
        click(tree, node(tree, "Task_001"));
        QCOMPARE(workspace->currentWidget(), sharedWorkspace);
        QCOMPARE(curve->task, QString("Task_001"));
        QCOMPARE(params->item(0, 1)->text(), QString("1.2"));
        QVERIFY(window.findChild<QLabel *>("propertyTitle")->text().contains("Task_001"));
        QVERIFY(window.findChild<QLabel *>("outputTitle")->text().contains("Task_001"));
        window.displayTaskEvent(event("Run_001", 2, "background-single"));
        QVERIFY(log->toPlainText().isEmpty());
        QCOMPARE(page->selectedTaskId(), QString("Task_001"));
        QCOMPARE(tree->currentItem(), node(tree, "Task_001"));
        window.displayTaskEvent(event("Task_001", 1, "child-log"));
        MonitorSample sample; sample.taskId = "Task_001"; sample.metric = "residual";
        sample.sequence = 1; sample.value = 0.1;
        page->appendMonitorSample(sample, "attempt-1");
        QCOMPARE(curve->samples.size(), 1);
        click(tree, node(tree, "BatchRun_001"));
        QCOMPARE(workspace->currentWidget(), sharedWorkspace);
        QVERIFY(curve->task.isEmpty()); QVERIFY(log->toPlainText().isEmpty());
        auto *children = page->findChild<QTableWidget *>("monitorBatchChildren");
        QCOMPARE(children->rowCount(), 2);
        QVERIFY(params->isHidden());
        QApplication::processEvents();
        QTest::mouseClick(children->viewport(), Qt::LeftButton, Qt::NoModifier,
                          children->visualItemRect(children->item(0, 0)).center());
        QCOMPARE(tree->currentItem(), node(tree, "Task_001"));
        QCOMPARE(curve->samples.size(), 1);
        QVERIFY(log->toPlainText().contains("child-log"));
        QVERIFY(!params->isHidden());
        auto finished = event("Task_001", 100, "done");
        finished.type = TaskEventType::Finished;
        finished.status = TaskStatus::Finished;
        window.displayTaskEvent(finished);
        QCOMPARE(page->findChild<QProgressBar *>("monitorTaskProgress")->value(), 100);
        click(tree, node(tree, "Run_001"));
        QVERIFY(log->toPlainText().contains("background-single"));
        click(tree, node(tree, "single-runs"));
        QVERIFY(page->selectedTaskId().isEmpty()); QVERIFY(curve->task.isEmpty());
        click(tree, node(tree, "mesh-transform"));
        QVERIFY(!propertyDock->isHidden());
        QCOMPARE(workspace->currentWidget()->objectName(), QString("mesh-transformPage"));
        window.findChild<QAction *>("monitorAction")->trigger();
        QCOMPARE(workspace->currentWidget(), sharedWorkspace);
        QVERIFY(propertyDock->isHidden());
        page->clearTasks();
        QCOMPARE(node(tree, "single-runs")->childCount(), 0);
        QCOMPARE(node(tree, "batch-runs")->childCount(), 0);
    }
    void serviceRebindingAndRetries() {
        MainWindow window;
        MonitorTaskService service;
        service.records.insert("A", record("A", "Batch"));
        ServiceBindings bindings; bindings.tasks = &service;
        window.bindServices(bindings); window.bindServices(bindings);
        auto *page = window.monitorPage();
        auto *tree = window.findChild<QTreeWidget *>("workspaceTree");
        auto *output = window.findChild<QTableWidget *>("outputLogTable");
        const int before = output->rowCount();
        emit service.taskEventOccurred(event("A", 1, "first"));
        QTRY_VERIFY(node(tree, "A"));
        QCOMPARE(node(tree, "A")->parent(), node(tree, "Batch"));
        QCOMPARE(output->rowCount(), before + 1);
        page->selectTask("A");
        auto *log = page->findChild<QPlainTextEdit *>("monitorTaskLog");
        emit service.taskEventOccurred(event("A", 1, "duplicate"));
        QApplication::processEvents();
        QCOMPARE(output->rowCount(), before + 1);
        QVERIFY(!log->toPlainText().contains("duplicate"));
        service.records["A"].attemptId = "attempt-2";
        auto retry = event("A", 1, "retry"); retry.attemptId = "attempt-2";
        emit service.taskEventOccurred(retry);
        QTRY_VERIFY(log->toPlainText().contains("retry"));
        QVERIFY(!log->toPlainText().contains("first"));
        emit service.taskEventOccurred(event("A", 2, "retired"));
        QApplication::processEvents();
        QVERIFY(!log->toPlainText().contains("retired"));
        emit service.taskEventOccurred(retry);
        window.bindServices({});
        QApplication::processEvents();
        QVERIFY(!node(tree, "A"));
        QVERIFY(log->toPlainText().isEmpty());
        QCOMPARE(node(tree, "batch-runs")->childCount(), 0);
    }
    void nativeLayoutEvidence() {
        MainWindow window; window.show();
        auto *page = window.monitorPage();
        page->upsertTask(record("Run_001"), {{"Mach", 0.6}});
        page->upsertTask(record("Task_001", "BatchRun_001"), {{"Mach", 0.8}});
        page->upsertTask(record("Task_002", "BatchRun_001"), {{"Mach", 1.2}});
        page->selectTask("Task_001");
        window.displayTaskEvent(event("Task_001", 25, "UI test: task log, no solver executed."));
        window.showComputeMonitor();
        const QString capture = qEnvironmentVariable("AEROSIM_UI_CAPTURE_DIR");
        auto *split = page->findChild<QSplitter *>("monitorCurveLogSplit");
        auto *log = page->findChild<QPlainTextEdit *>("monitorTaskLog");
        for (const QSize size : {QSize(1000, 720), QSize(1440, 960)}) {
            window.resize(size); QApplication::processEvents();
            QVERIFY(log->width() >= 150); QVERIFY(log->height() >= 100);
            QVERIFY(split->widget(0)->geometry().bottom() < split->widget(1)->geometry().top());
            if (!capture.isEmpty()) {
                QDir().mkpath(capture);
                QVERIFY(window.grab().save(capture + QString("/main-monitor-%1.png").arg(size.width())));
            }
        }
        page->selectBatch("BatchRun_001"); QApplication::processEvents();
        if (!capture.isEmpty()) QVERIFY(window.grab().save(capture + "/main-monitor-batch.png"));
    }
};

QTEST_MAIN(MainMonitorTest)
#include "MainWindowMonitorTest.moc"
