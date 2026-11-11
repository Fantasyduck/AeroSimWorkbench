#include "aerosim/ui/MainWindow.h"
#include "aerosim/ui/monitor/ComputeMonitorPage.h"
#include "aerosim/contracts/Telemetry.h"
#include <QApplication>
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

// Receives the contract type directly; the page hands over MonitorSampleV2 and
// the test must not need a narrower copy to observe what was delivered.
class CurveReceiver final : public MonitorCurveWidget {
public:
    QString task;
    QString attempt;
    QVector<MonitorSampleV2> samples;
    void replaceTask(const QString &id, const QString &run, const QVector<MonitorSampleV2> &history) override {
        task = id; attempt = run; samples = history;
    }
    void appendSample(const MonitorSampleV2 &sample) override { samples.append(sample); }
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
    MonitorSampleV2 sample(const QString &id, double value) {
        MonitorSampleV2 s; s.task.taskId = id; s.attemptId = "attempt-1";
        s.metricKey = "rms[Rho]"; s.quantity = MonitorQuantity::Residual;
        s.step = 1; s.sequence = 1; s.value = value; return s;
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
    // The window owns one live page. This covers the three things only an
    // end-to-end test can: the navigation entry reaches it, task events reach it
    // without displacing the placeholder monitor page, and the curve slot takes
    // exactly one widget.
    void computeMonitorIsReachableAndFedByEvents() {
        MainWindow window; window.show();
        auto *page = window.monitorPage();
        QVERIFY(page);
        auto *tree = window.findChild<QTreeWidget *>("workspaceTree");
        auto *workspace = window.findChild<QStackedWidget *>("centralWorkspace");
        QVERIFY(node(tree, "compute-monitor"));
        click(tree, node(tree, "compute-monitor"));
        QCOMPARE(workspace->currentWidget(), static_cast<QWidget *>(page));

        page->upsertTask(record("Run_001"), {{"Mach", 0.6}});
        auto *curve = new CurveReceiver;
        QVERIFY(page->insertCurveWidget(curve));
        // A second curve widget must be refused rather than stacked: the page
        // owns one slot and a test that injects a double would be locked out by
        // a silently accepted second one.
        auto *second = new CurveReceiver;
        QVERIFY(!page->insertCurveWidget(second));
        delete second;

        QVERIFY(page->selectTask("Run_001"));
        QCOMPARE(curve->task, QString("Run_001"));
        QCOMPARE(curve->attempt, QString("attempt-1"));
        auto *params = page->findChild<QTableWidget *>("monitorParameters");
        QCOMPARE(params->item(0, 1)->text(), QString("0.6"));

        auto *log = page->findChild<QPlainTextEdit *>("monitorTaskLog");
        window.displayTaskEvent(event("Run_001", 1, "single-log"));
        QApplication::processEvents();
        QVERIFY(log->toPlainText().contains("single-log"));
        QCOMPARE(page->taskRecord("Run_001").status, TaskStatus::Running);

        page->appendMonitorSample(sample("Run_001", 0.1), "attempt-1");
        QCOMPARE(curve->samples.size(), 1);
        QCOMPARE(curve->samples.first().metricKey, QString("rms[Rho]"));

        // Re-entering must be idempotent and must not disturb the selection.
        window.showComputeMonitor();
        window.showComputeMonitor();
        QCOMPARE(workspace->currentWidget(), static_cast<QWidget *>(page));
        QCOMPARE(page->selectedTaskId(), QString("Run_001"));
        QCOMPARE(curve->samples.size(), 1);
    }
    // The placeholder page carries its own object names and mock log behaviour
    // and is pinned by the window-level tests; the live page must not take it
    // over. Both are separate entries in the same workspace stack.
    void livePageDoesNotDisplacePlaceholder() {
        MainWindow window; window.show();
        auto *page = window.monitorPage();
        auto *workspace = window.findChild<QStackedWidget *>("centralWorkspace");
        auto *placeholder = window.findChild<QWidget *>("taskMonitorPage");
        QVERIFY(placeholder);
        QVERIFY(page != placeholder);
        window.showComputeMonitor();
        QCOMPARE(workspace->currentWidget(), static_cast<QWidget *>(page));
        auto *tree = window.findChild<QTreeWidget *>("workspaceTree");
        click(tree, node(tree, "Task_001"));
        QCOMPARE(workspace->currentWidget(), placeholder);
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
        QVERIFY(page->selectTask("A"));
        auto *log = page->findChild<QPlainTextEdit *>("monitorTaskLog");
        emit service.taskEventOccurred(event("A", 1, "duplicate"));
        QApplication::processEvents();
        // The window log table records every event it is handed; sequence-based
        // de-duplication is the live page's contract, not the window's. Both
        // sides are asserted separately so a change to either stays visible.
        QCOMPARE(output->rowCount(), before + 2);
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
        // Unbinding drops the connections; the page keeps what it already has.
        window.bindServices({});
        QApplication::processEvents();
        QVERIFY(log->toPlainText().contains("retry"));
    }
    void nativeLayoutEvidence() {
        MainWindow window; window.show();
        auto *page = window.monitorPage();
        page->upsertTask(record("Run_001"), {{"Mach", 0.6}});
        page->upsertTask(record("Task_001", "BatchRun_001"), {{"Mach", 0.8}});
        page->upsertTask(record("Task_002", "BatchRun_001"), {{"Mach", 1.2}});
        QVERIFY(page->selectTask("Task_001"));
        window.displayTaskEvent(event("Task_001", 25, "UI test: task log, no solver executed."));
        window.showComputeMonitor();
        const QString capture = qEnvironmentVariable("AEROSIM_UI_CAPTURE_DIR");
        auto *split = page->findChild<QSplitter *>("monitorCurveLogSplit");
        auto *log = page->findChild<QPlainTextEdit *>("monitorTaskLog");
        QVERIFY(split); QVERIFY(log);
        for (const QSize size : {QSize(1000, 720), QSize(1440, 960)}) {
            window.resize(size); QApplication::processEvents();
            QVERIFY(log->width() >= 150); QVERIFY(log->height() >= 100);
            QVERIFY(split->widget(0)->geometry().bottom() < split->widget(1)->geometry().top());
            if (!capture.isEmpty()) {
                QDir().mkpath(capture);
                QVERIFY(window.grab().save(capture + QString("/main-monitor-%1.png").arg(size.width())));
            }
        }
        QVERIFY(page->selectBatch("BatchRun_001")); QApplication::processEvents();
        if (!capture.isEmpty()) QVERIFY(window.grab().save(capture + "/main-monitor-batch.png"));
    }
};

QTEST_MAIN(MainMonitorTest)
#include "MainWindowMonitorTest.moc"
