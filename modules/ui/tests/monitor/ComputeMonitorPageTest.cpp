#include "aerosim/ui/monitor/ComputeMonitorPage.h"
#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QSplitter>
#include <QTableWidget>
#include <QTest>
#include <QTreeWidget>

using namespace aerosim;

class CurveSpy final : public MonitorCurveWidget {
public:
    QString task;
    QString attempt;
    QVector<MonitorSampleV2> samples;
    int resets = 0;
    void replaceTask(const QString &id, const QString &run, const QVector<MonitorSampleV2> &history) override {
        task = id; attempt = run; samples = history; ++resets;
    }
    void appendSample(const MonitorSampleV2 &sample) override { samples.append(sample); }
};

class MonitorTest final : public QObject {
    Q_OBJECT
    TaskRecord record(const QString &id, const QString &batch = {}) {
        TaskRecord r; r.taskId = id; r.batchId = batch; r.attemptId = "attempt-1";
        r.caseId = "case-" + id; r.status = TaskStatus::Preparing; return r;
    }
    TaskEvent event(const QString &id, quint64 sequence, const QString &message) {
        TaskEvent e; e.taskId = id; e.attemptId = "attempt-1"; e.sequence = sequence;
        e.message = message; e.status = TaskStatus::Running; return e;
    }
    MonitorSampleV2 sample(const QString &id, quint64 sequence) {
        MonitorSampleV2 s; s.task.taskId = id; s.sequence = sequence; s.metricKey = "residual";
        s.value = 1.0 / sequence; return s;
    }
private slots:
    void selectionAndLiveIsolation() {
        ComputeMonitorPage page;
        page.resize(1100, 680); page.show();
        page.upsertTask(record("Run_001"), {{"Mach", 0.6}});
        page.upsertTask(record("Task_001", "BatchRun_001"), {{"Mach", 1.2}});
        page.upsertTask(record("Task_002", "BatchRun_001"), {{"Mach", 2.4}});
        page.displayTaskEvent(event("Run_001", 1, "single-only"));
        page.displayTaskEvent(event("Task_001", 1, "batch-child-one"));
        page.appendMonitorSample(sample("Run_001", 1), "attempt-1");
        page.appendMonitorSample(sample("Task_001", 1), "attempt-1");
        auto *curve = new CurveSpy;
        QVERIFY(page.insertCurveWidget(curve));
        QVERIFY(page.isAncestorOf(curve));
        auto *rejected = new CurveSpy;
        QVERIFY(!page.insertCurveWidget(rejected)); QVERIFY(!rejected->parent()); delete rejected;
        auto *tree = page.findChild<QTreeWidget *>("monitorTaskTree");
        auto *log = page.findChild<QPlainTextEdit *>("monitorTaskLog");
        auto *params = page.findChild<QTableWidget *>("monitorParameters");
        QSignalSpy selection(&page, &ComputeMonitorPage::taskSelected);
        QApplication::processEvents();
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tree->visualItemRect(tree->topLevelItem(0)->child(0)).center());
        QCOMPARE(page.selectedTaskId(), QString("Run_001"));
        QCOMPARE(curve->task, QString("Run_001"));
        QCOMPARE(curve->samples.size(), 1);
        QVERIFY(log->toPlainText().contains("single-only"));
        auto *child = tree->topLevelItem(1)->child(0)->child(0);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(child).center());
        QCOMPARE(page.selectedTaskId(), QString("Task_001"));
        QCOMPARE(curve->task, QString("Task_001"));
        QVERIFY(log->toPlainText().contains("batch-child-one"));
        QVERIFY(!log->toPlainText().contains("single-only"));
        QCOMPARE(params->item(0, 1)->text(), QString("1.2"));
        const int resets = curve->resets;
        page.displayTaskEvent(event("Run_001", 2, "background-single"));
        page.appendMonitorSample(sample("Run_001", 2), "attempt-1");
        QCOMPARE(curve->resets, resets);
        QCOMPARE(curve->samples.size(), 1);
        QVERIFY(!log->toPlainText().contains("background-single"));
        page.appendMonitorSample(sample("Task_001", 2), "attempt-1");
        QCOMPARE(curve->samples.size(), 2);
        page.selectTask("Task_002");
        QVERIFY(log->toPlainText().isEmpty()); QVERIFY(curve->samples.isEmpty());
        page.selectTask("Run_001");
        QVERIFY(log->toPlainText().contains("background-single"));
        QCOMPARE(curve->samples.size(), 2);
        QVERIFY(selection.count() >= 4);
        const auto curveRect = curve->geometry();
        QVERIFY(curveRect.height() > 20);
        const auto *splitter = page.findChild<QSplitter *>("monitorCurveLogSplit");
        QVERIFY(splitter->widget(0)->geometry().bottom() < splitter->widget(1)->geometry().top());
    }

    void batchAndAttempts() {
        ComputeMonitorPage page; page.resize(1100, 680); page.show();
        page.upsertTask(record("A", "Batch")); page.upsertTask(record("B", "Batch"));
        auto *curve = new CurveSpy; page.insertCurveWidget(curve);
        page.selectTask("A");
        auto e = event("A", 10, "newest"); e.status = TaskStatus::Finished;
        page.displayTaskEvent(e);
        page.displayTaskEvent(event("A", 9, "stale"));
        page.displayTaskEvent(e);
        auto *log = page.findChild<QPlainTextEdit *>("monitorTaskLog");
        QVERIFY(!log->toPlainText().contains("stale"));
        QCOMPARE(log->toPlainText().count("newest"), 1);
        auto *tree = page.findChild<QTreeWidget *>("monitorTaskTree");
        tree->setCurrentItem(tree->topLevelItem(1)->child(0));
        QVERIFY(page.selectedTaskId().isEmpty()); QVERIFY(curve->task.isEmpty());
        QVERIFY(log->toPlainText().isEmpty());
        auto *children = page.findChild<QTableWidget *>("monitorBatchChildren");
        QCOMPARE(children->rowCount(), 2);
        QApplication::processEvents();
        QTest::mouseClick(children->viewport(), Qt::LeftButton, Qt::NoModifier,
                          children->visualItemRect(children->item(1, 0)).center());
        QCOMPARE(page.selectedTaskId(), QString("B"));
        page.selectTask("A");
        auto next = record("A", "Batch"); next.attemptId = "attempt-2";
        page.upsertTask(next, {{"Mach", 3.0}});
        QVERIFY(log->toPlainText().isEmpty());
        QCOMPARE(curve->attempt, QString("attempt-2"));
        page.displayTaskEvent(event("A", 11, "old-attempt"));
        page.upsertTask(record("A", "Batch"));
        page.appendMonitorSample(sample("A", 12), "attempt-1");
        QVERIFY(log->toPlainText().isEmpty()); QVERIFY(curve->samples.isEmpty());
        e.attemptId = "attempt-2"; e.sequence = 1; e.message = "fresh-attempt";
        page.displayTaskEvent(e);
        QVERIFY(log->toPlainText().contains("fresh-attempt"));
        page.clearTasks();
        QVERIFY(curve->task.isEmpty()); QVERIFY(log->toPlainText().isEmpty());
        QCOMPARE(tree->topLevelItem(1)->childCount(), 0);
        delete curve;
        QVERIFY(page.insertCurveWidget(new CurveSpy));
    }

    void boundedHistoryAndLayout() {
        ComputeMonitorPage page; page.resize(1100, 680); page.show();
        page.upsertTask(record("Task_001", "BatchRun_001"), {{"Mach", 0.8}});
        for (int i = 1; i <= 2100; ++i) page.displayTaskEvent(event("Task_001", i, QString("line-%1").arg(i)));
        for (int i = 1; i <= 5100; ++i) page.appendMonitorSample(sample("Task_001", i), "attempt-1");
        page.selectTask("Task_001");
        auto *log = page.findChild<QPlainTextEdit *>("monitorTaskLog");
        QCOMPARE(log->document()->blockCount(), 2000);
        QVERIFY(!log->toPlainText().contains("line-1\n"));
        auto *curve = new CurveSpy; page.insertCurveWidget(curve);
        QCOMPARE(curve->samples.size(), 5000);
        QCOMPARE(curve->samples.first().sequence, quint64(101));
        delete curve; QApplication::processEvents();
        const QString capture = qEnvironmentVariable("AEROSIM_UI_CAPTURE_DIR");
        for (const QSize size : {QSize(1000, 600), QSize(1440, 900)}) {
            page.resize(size); QApplication::processEvents();
            QVERIFY(log->width() > 100); QVERIFY(log->height() >= 100);
            if (!capture.isEmpty()) {
                QDir().mkpath(capture);
                QVERIFY(page.grab().save(capture + QString("/monitor-%1.png").arg(size.width())));
            }
        }
    }
};
QTEST_MAIN(MonitorTest)
#include "ComputeMonitorPageTest.moc"
