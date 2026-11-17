#include "aerosim/ui/intelligent/ResultSummaryTablePage.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <memory>

class ResultForecastTest : public QObject {
    Q_OBJECT
private slots:
    void fixtureForecastRunsFromTheSummaryPage();
    void invalidDataAndReloadInvalidateResults();
    void predictionCanBeCancelledAndItsPageClosed();
    void samplingStepMismatchCanBeCorrectedAutomatically();
};

void ResultForecastTest::fixtureForecastRunsFromTheSummaryPage()
{
    aerosim::ResultSummaryTablePage page;
    page.resize(1280, 800);
    page.show();
    auto *run = page.findChild<QPushButton *>(QStringLiteral("forecastRunButton"));
    QVERIFY(run && !run->isEnabled());
    QString error;
    QVERIFY2(page.loadFile(QStringLiteral(AEROSIM_FORECAST_FIXTURE_DIR "/su2_history_steady.csv"), &error), qPrintable(error));
    auto *panel = page.findChild<QWidget *>(QStringLiteral("resultForecastPanel"));
    auto *coordinate = page.findChild<QComboBox *>(QStringLiteral("forecastTimeColumn"));
    auto *target = page.findChild<QComboBox *>(QStringLiteral("forecastTargetColumn"));
    auto *inputs = page.findChild<QListWidget *>(QStringLiteral("forecastInputs"));
    auto *future = page.findChild<QTableWidget *>(QStringLiteral("forecastResults"));
    auto *validation = page.findChild<QTableWidget *>(QStringLiteral("forecastBacktest"));
    QVERIFY(panel && coordinate && target && inputs && future && validation);
    QCOMPARE(coordinate->currentText(), QStringLiteral("Inner_Iter"));
    QCOMPARE(target->currentText(), QStringLiteral("CD"));
    QCOMPARE(page.findChild<QSplitter *>(QStringLiteral("resultSummarySplit"))->count(), 2);
    for (int index = 0; index < inputs->count(); ++index) {
        auto *item = inputs->item(index);
        if (item->text() == QStringLiteral("CL") || item->text() == QStringLiteral("rms[Rho]"))
            item->setCheckState(Qt::Checked);
    }
    QSignalSpy finished(panel, SIGNAL(forecastFinished(bool)));
    QVERIFY(finished.isValid() && run->isEnabled());
    QTest::mouseClick(run, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(future->rowCount(), 5);
    QCOMPARE(validation->rowCount(), 5);
    QCOMPARE(future->item(0, 0)->text().toDouble(), 326.0);
    QCOMPARE(future->item(4, 0)->text().toDouble(), 330.0);
    QVERIFY(std::abs(future->item(0, 1)->text().toDouble() - 0.01100871616079867) < 2e-12);
    for (int row = 0; row < 5; ++row) {
        QVERIFY(std::isfinite(future->item(row, 1)->text().toDouble()));
        QVERIFY(validation->item(row, 3)->text().toDouble() < 1e-9);
    }
    QVERIFY(page.findChild<QLabel *>(QStringLiteral("forecastStatus"))->text().contains(QStringLiteral("39")));
    QVERIFY(future->isVisibleTo(&page));
    const QString screenshot = qEnvironmentVariable("AEROSIM_FORECAST_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        QCoreApplication::processEvents();
        QVERIFY(page.grab().save(screenshot));
    }

    auto *gamma = page.findChild<QLineEdit *>(QStringLiteral("forecastGamma"));
    QVERIFY(gamma);
    gamma->setText(QStringLiteral("0"));
    QVERIFY(!run->isEnabled());
    QCOMPARE(future->rowCount(), 0);
    gamma->setText(QStringLiteral("1e-4"));
    QVERIFY(run->isEnabled());
}

void ResultForecastTest::invalidDataAndReloadInvalidateResults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("physical_quantities.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("step,Pressure,Temperature\n");
    for (int row = 0; row < 6; ++row)
        file.write(QByteArray::number(row) + ',' + QByteArray::number(100000 + row) + ",300\n");
    file.close();
    aerosim::ResultSummaryTablePage page;
    page.resize(1200, 750);
    page.show();
    QString error;
    QVERIFY2(page.loadFile(path, &error), qPrintable(error));
    auto *run = page.findChild<QPushButton *>(QStringLiteral("forecastRunButton"));
    auto *panel = page.findChild<QWidget *>(QStringLiteral("resultForecastPanel"));
    auto *future = page.findChild<QTableWidget *>(QStringLiteral("forecastResults"));
    auto *status = page.findChild<QLabel *>(QStringLiteral("forecastStatus"));
    auto *horizon = page.findChild<QSpinBox *>(QStringLiteral("forecastHorizon"));
    auto *target = page.findChild<QComboBox *>(QStringLiteral("forecastTargetColumn"));
    QVERIFY(run && panel && future && status && horizon && target);
    QCOMPARE(target->currentText(), QStringLiteral("Pressure"));
    QSignalSpy finished(panel, SIGNAL(forecastFinished(bool)));
    QTest::mouseClick(run, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
    QCOMPARE(finished.first().first().toBool(), false);
    QVERIFY(status->text().contains(QStringLiteral("7")));
    QCOMPARE(future->rowCount(), 0);

    horizon->setValue(2);
    QTest::mouseClick(run, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 15000);
    QCOMPARE(finished.last().first().toBool(), true);
    QCOMPARE(future->rowCount(), 2);
    QCOMPARE(future->item(0, 0)->text().toDouble(), 6.0);

    // Reloading or clearing during work must ignore the older completion.
    QTest::mouseClick(run, Qt::LeftButton);
    page.clearData();
    QVERIFY(!run->isEnabled());
    QTest::qWait(100);
    QCOMPARE(future->rowCount(), 0);
    QCOMPARE(finished.count(), 2);
    QVERIFY(status->text().contains(QStringLiteral(u"导入")));
    QVERIFY2(page.loadFile(path, &error), qPrintable(error));
    QVERIFY(run->isEnabled());
    QTest::mouseClick(run, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 3, 15000);
    QCOMPARE(finished.last().first().toBool(), true);
}

void ResultForecastTest::predictionCanBeCancelledAndItsPageClosed()
{
    std::unique_ptr<aerosim::ResultSummaryTablePage> page(new aerosim::ResultSummaryTablePage);
    page->resize(1200, 750);
    page->show();
    QString error;
    QVERIFY2(page->loadFile(QStringLiteral(AEROSIM_FORECAST_FIXTURE_DIR "/su2_history_steady.csv"), &error), qPrintable(error));
    auto *run = page->findChild<QPushButton *>(QStringLiteral("forecastRunButton"));
    auto *cancel = page->findChild<QPushButton *>(QStringLiteral("forecastCancelButton"));
    auto *panel = page->findChild<QWidget *>(QStringLiteral("resultForecastPanel"));
    auto *status = page->findChild<QLabel *>(QStringLiteral("forecastStatus"));
    QVERIFY(run && cancel && panel && status);
    QSignalSpy finished(panel, SIGNAL(forecastFinished(bool)));
    QTest::mouseClick(run, Qt::LeftButton);
    QVERIFY(cancel->isVisibleTo(page.get()));
    QTest::mouseClick(cancel, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
    QCOMPARE(finished.first().first().toBool(), false);
    QVERIFY(status->text().contains(QStringLiteral(u"取消")));
    QVERIFY(run->isEnabled());
    QTest::mouseClick(run, Qt::LeftButton);
    page.reset();
    QTest::qWait(100);
}

void ResultForecastTest::samplingStepMismatchCanBeCorrectedAutomatically()
{
    aerosim::ResultSummaryTablePage page;
    page.resize(1200, 750);
    page.show();
    QString error;
    QVERIFY2(page.loadFile(QStringLiteral(AEROSIM_FORECAST_FIXTURE_DIR "/su2_history_steady.csv"), &error), qPrintable(error));
    auto *coordinate = page.findChild<QComboBox *>(QStringLiteral("forecastTimeColumn"));
    auto *target = page.findChild<QComboBox *>(QStringLiteral("forecastTargetColumn"));
    auto *inputs = page.findChild<QListWidget *>(QStringLiteral("forecastInputs"));
    auto *step = page.findChild<QDoubleSpinBox *>(QStringLiteral("forecastStep"));
    auto *automatic = page.findChild<QPushButton *>(QStringLiteral("forecastAutomaticStep"));
    auto *run = page.findChild<QPushButton *>(QStringLiteral("forecastRunButton"));
    auto *status = page.findChild<QLabel *>(QStringLiteral("forecastStatus"));
    auto *panel = page.findChild<QWidget *>(QStringLiteral("resultForecastPanel"));
    auto *future = page.findChild<QTableWidget *>(QStringLiteral("forecastResults"));
    QVERIFY(coordinate && target && inputs && step && automatic && run && status && panel && future);
    target->setCurrentIndex(target->findText(QStringLiteral("CEff")));
    for (int index = 0; index < inputs->count(); ++index) {
        auto *item = inputs->item(index);
        item->setCheckState(item->text() == QStringLiteral("CEff") ? Qt::Checked : Qt::Unchecked);
    }
    step->setValue(7);
    QVERIFY(!run->isEnabled());
    QVERIFY(status->text().contains(QStringLiteral(u"末尾间隔为 1")));
    QVERIFY(status->text().contains(QStringLiteral(u"采样步长为 7")));
    QTest::mouseClick(automatic, Qt::LeftButton);
    QCOMPARE(step->value(), 1.0);
    QVERIFY(run->isEnabled());
    QSignalSpy finished(panel, SIGNAL(forecastFinished(bool)));
    QTest::mouseClick(run, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(future->rowCount(), 5);
    QCOMPARE(future->item(0, 0)->text().toDouble(), 326.0);
    QCOMPARE(future->item(4, 0)->text().toDouble(), 330.0);

    coordinate->setCurrentIndex(coordinate->findText(QStringLiteral("Time_Iter")));
    QVERIFY(!run->isEnabled());
    QVERIFY(status->text().contains(QStringLiteral(u"没有有效的递增间隔")));
    coordinate->setCurrentIndex(coordinate->findText(QStringLiteral("Inner_Iter")));
    QVERIFY(run->isEnabled());
}

QTEST_MAIN(ResultForecastTest)
#include "ResultForecastTest.moc"
