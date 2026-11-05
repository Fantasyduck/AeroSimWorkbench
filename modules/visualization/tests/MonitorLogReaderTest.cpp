#include "MonitorCurveFit.h"
#include "MonitorFileTailSource.h"
#include "MonitorLiveSource.h"
#include "MonitorLogAccumulator.h"
#include "MonitorLogReader.h"
#include "MonitorSeriesModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>

using aerosim::monitor::MonitorAxis;
using aerosim::monitor::MonitorColumn;
using aerosim::monitor::MonitorFileTailSource;
using aerosim::monitor::MonitorLiveSource;
using aerosim::monitor::MonitorLog;
using aerosim::monitor::MonitorLogAccumulator;
using aerosim::monitor::MonitorLogReader;
using aerosim::monitor::MonitorScale;
using aerosim::monitor::MonitorSeriesModel;
using aerosim::monitor::MonitorUpdate;

namespace {

int failures = 0;

// tests/fixtures/postprocess/su2_history_steady.csv keeps the first 40 and the
// last 39 data rows of the real SU2 run so that both the initial and the
// converged state are covered; VerifyFixtures.cmake enforces the same shape.
const int kFixtureRows = 79;

void check(bool passed, const QString &what)
{
    if (passed)
        return;
    ++failures;
    QTextStream(stderr) << "FAIL: " << what << '\n';
}

QByteArray fixtureBytes(const QString &name)
{
    // The test binary receives the fixture directory so it can read the same
    // file VerifyFixtures.cmake hashes.
    const QString dir = QString::fromLocal8Bit(qgetenv("AEROSIM_POSTPROCESS_FIXTURE_DIR"));
    QFile file(QDir(dir).filePath(name));
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

void testRealHistoryFixture()
{
    const QByteArray content = fixtureBytes(QStringLiteral("su2_history_steady.csv"));
    check(!content.isEmpty(), QStringLiteral("fixture su2_history_steady.csv is readable"));

    MonitorLog log;
    QString error;
    check(MonitorLogReader::readCsv(content, QStringLiteral("su2_history_steady.csv"), &log, &error),
          QStringLiteral("readCsv accepts the real SU2 history sample: ") + error);
    if (!error.isEmpty())
        return;

    // Header fields keep their original padding and quotes; the reader must trim
    // and unquote, otherwise no column would ever match by name.
    check(log.values.contains(QStringLiteral("CD")), QStringLiteral("column 'CD' found despite padded header"));
    check(log.values.contains(QStringLiteral("rms[Rho]")), QStringLiteral("column 'rms[Rho]' found"));
    check(log.values.contains(QStringLiteral("Time_Iter")), QStringLiteral("column 'Time_Iter' found"));
    check(log.values.size() == 17, QStringLiteral("all 17 columns ingested, got %1").arg(log.values.size()));
    check(log.iterationStep.size() == kFixtureRows,
          QStringLiteral("%1 data rows, got %2").arg(kFixtureRows).arg(log.iterationStep.size()));

    const QVector<double> &cd = log.values.value(QStringLiteral("CD"));
    check(cd.size() == kFixtureRows, QStringLiteral("CD column has %1 rows").arg(kFixtureRows));
    if (cd.size() == kFixtureRows) {
        check(qAbs(cd.first() - 0.0803708097) < 1e-12, QStringLiteral("first CD value parsed exactly"));
        // The fixture keeps the head and the tail of the real run, so the last
        // row is the converged value named in the plan (P0 acceptance).
        check(qAbs(cd.last() - 0.01100871693) < 1e-12, QStringLiteral("last CD value parsed exactly"));
    }

    // The sample is a steady run: Time_Iter and Outer_Iter stay 0 while
    // Inner_Iter advances, so the iteration axis must fall back to Inner_Iter.
    check(log.values.value(QStringLiteral("Outer_Iter")).last() == 0.0,
          QStringLiteral("Outer_Iter never advances in the steady sample"));
    check(log.iterationStep.first() == 0 && log.iterationStep.last() == 325,
          QStringLiteral("iteration axis falls back to Inner_Iter 0..325"));
    check(log.outerIterationProgression == 0, QStringLiteral("outer progression stays zero"));
    check(log.innerIterationProgression == kFixtureRows - 1, QStringLiteral("every fixture row advances Inner_Iter"));

    MonitorColumn axisColumn;
    bool sawAxisFlag = false;
    for (const MonitorColumn &column : log.columns) {
        if (column.key == QStringLiteral("Inner_Iter")) {
            axisColumn = column;
            sawAxisFlag = column.isAxis;
        }
    }
    check(sawAxisFlag, QStringLiteral("Inner_Iter is flagged as an axis, not a series"));

    bool sawResidual = false;
    for (const MonitorColumn &column : log.columns) {
        if (column.key == QStringLiteral("rms[Rho]"))
            sawResidual = column.isResidual;
    }
    check(sawResidual, QStringLiteral("rms[Rho] is flagged as a residual"));
    check(MonitorLogReader::looksLikeResidual(QStringLiteral("rms[RhoU]")),
          QStringLiteral("looksLikeResidual recognises rms columns"));
    check(!MonitorLogReader::looksLikeResidual(QStringLiteral("CD")),
          QStringLiteral("looksLikeResidual rejects CD"));
}

void testHostileInputs()
{
    MonitorLog log;
    QString error;

    check(!MonitorLogReader::readCsv(QByteArray(), QStringLiteral("empty.csv"), &log, &error),
          QStringLiteral("empty file is rejected"));
    check(!error.isEmpty(), QStringLiteral("empty file produces a readable error"));

    error.clear();
    check(!MonitorLogReader::readCsv(QByteArray("a,b\n1,2\n"), QStringLiteral("nohistory.csv"), &log, &error),
          QStringLiteral("a CSV without an iteration column is rejected"));

    // A binary blob must not be parsed as numbers; the reader reports that
    // instead of producing a bogus curve.
    QByteArray binary;
    binary.append('\x01');
    binary.append('\x02');
    binary.append('\x03');
    binary.append('\xFF');
    error.clear();
    check(!MonitorLogReader::readCsv(binary, QStringLiteral("bin.csv"), &log, &error),
          QStringLiteral("binary content is rejected"));

    // CRLF, BOM and blank lines occur in files written on Windows.
    QByteArray messy;
    messy.append("\xEF\xBB\xBFTime_Iter,Outer_Iter,Inner_Iter,CD\r\n");
    messy.append("0,0,0,0.5\r\n");
    messy.append("\r\n");
    messy.append("0,0,1,0.4\r\n");
    error.clear();
    check(MonitorLogReader::readCsv(messy, QStringLiteral("messy.csv"), &log, &error),
          QStringLiteral("BOM + CRLF + blank line accepted: ") + error);
    check(log.iterationStep.size() == 2, QStringLiteral("blank line skipped, got %1 rows").arg(log.iterationStep.size()));
    check(log.values.value(QStringLiteral("CD")).size() == 2, QStringLiteral("CD kept two rows"));

    // One malformed row must not discard the whole file; it is skipped loudly.
    QByteArray ragged;
    ragged.append("Time_Iter,Outer_Iter,Inner_Iter,CD\n");
    ragged.append("0,0,0,0.5\n");
    ragged.append("0,0,1,not-a-number\n");
    ragged.append("0,0,2,0.3\n");
    error.clear();
    const bool ok = MonitorLogReader::readCsv(ragged, QStringLiteral("ragged.csv"), &log, &error);
    check(ok || !log.warnings.isEmpty(), QStringLiteral("a bad row is reported, never silent"));
    if (ok) {
        check(log.iterationStep.size() == 3, QStringLiteral("ragged file keeps its three rows"));
        check(log.warnings.size() >= 1, QStringLiteral("ragged file emits a warning"));
    }
}

bool writeTempFile(const QDir &dir, const QString &name, const QByteArray &content)
{
    QFile file(dir.filePath(name));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const bool ok = file.write(content) == content.size();
    file.close();
    return ok;
}

void testCanRead()
{
    // canRead() probes the real header, so it needs real files rather than
    // plausible-looking paths.
    QTemporaryDir scratch;
    check(scratch.isValid(), QStringLiteral("temporary directory is available"));
    if (!scratch.isValid())
        return;
    const QDir dir(scratch.path());
    const QByteArray historyHeader("Time_Iter,Outer_Iter,Inner_Iter,CD\n"
                                    "0,0,0,0.5\n");
    check(writeTempFile(dir, QStringLiteral("history.csv"), historyHeader)
              && MonitorLogReader::canRead(dir.filePath(QStringLiteral("history.csv"))),
          QStringLiteral("history.csv is recognised"));
    check(writeTempFile(dir, QStringLiteral("history_coarse.csv"), historyHeader)
              && MonitorLogReader::canRead(dir.filePath(QStringLiteral("history_coarse.csv"))),
          QStringLiteral("history_coarse.csv is recognised"));

    const QByteArray curvesHeader("seriesId,caseId,xName,xUnit,x,yName,yUnit,y\n"
                                  "body-z-coefficient,case-00,alpha,deg,0,Cz,1,0.0\n");
    check(writeTempFile(dir, QStringLiteral("curves.csv"), curvesHeader),
          QStringLiteral("curves.csv scratch copy written"));
    check(!MonitorLogReader::canRead(dir.filePath(QStringLiteral("curves.csv"))),
          QStringLiteral("the fixture curves.csv is not mistaken for a solver history"));

    check(writeTempFile(dir, QStringLiteral("notes.txt"), historyHeader),
          QStringLiteral("non-csv scratch copy written"));
    check(!MonitorLogReader::canRead(dir.filePath(QStringLiteral("notes.txt"))),
          QStringLiteral("a non-csv suffix is refused"));

    check(MonitorLogReader::datasetIdForPath(QStringLiteral("C:/cases/history.csv")) == QStringLiteral("history"),
          QStringLiteral("dataset id is the file stem"));
}

void testSeriesModel()
{
    const QByteArray content = fixtureBytes(QStringLiteral("su2_history_steady.csv"));
    MonitorLog log;
    QString error;
    if (!MonitorLogReader::readCsv(content, QStringLiteral("su2_history_steady.csv"), &log, &error))
        return;

    MonitorSeriesModel model;
    check(model.setLog(log, &error), QStringLiteral("model accepts the parsed log: ") + error);
    check(model.isValid(), QStringLiteral("model reports itself valid"));
    check(model.rowCount() == kFixtureRows, QStringLiteral("model exposes %1 rows").arg(kFixtureRows));
    check(model.sourcePath() == QStringLiteral("su2_history_steady.csv"), QStringLiteral("model keeps the source path"));

    // Degenerate time axis: the whole point of axisAvailable().
    QString reason;
    check(!model.axisAvailable(MonitorAxis::TimeIndex, &reason),
          QStringLiteral("time axis is unavailable for a steady run"));
    check(!reason.isEmpty(), QStringLiteral("the unavailable axis explains why"));
    check(reason.contains(QStringLiteral("0")), QStringLiteral("the reason names the constant value"));
    check(model.axisAvailable(MonitorAxis::IterationStep), QStringLiteral("iteration axis stays available"));
    check(model.axisLabel(MonitorAxis::IterationStep).contains(QStringLiteral("Inner_Iter")),
          QStringLiteral("axis label names the column actually used, got: ") + model.axisLabel(MonitorAxis::IterationStep));

    const QVector<QPointF> cd = model.points(QStringLiteral("CD"), MonitorAxis::IterationStep);
    check(cd.size() == kFixtureRows, QStringLiteral("CD series has %1 points, got %2").arg(kFixtureRows).arg(cd.size()));
    if (cd.size() == kFixtureRows) {
        check(cd.first().x() == 0.0 && cd.last().x() == 325.0, QStringLiteral("points use Inner_Iter as x"));
        check(qAbs(cd.last().y() - 0.01100871693) < 1e-12, QStringLiteral("last CD point keeps its value"));
    }
    check(model.points(QStringLiteral("CD"), MonitorAxis::TimeIndex).isEmpty(),
          QStringLiteral("degenerate axis yields no points instead of a flat line"));
    check(model.points(QStringLiteral("no-such-column"), MonitorAxis::IterationStep).isEmpty(),
          QStringLiteral("unknown column yields no points"));

    check(model.isResidual(QStringLiteral("rms[Rho]")), QStringLiteral("model flags residues"));
    check(!model.isResidual(QStringLiteral("CD")), QStringLiteral("model does not flag CD as a residue"));
    check(model.suggestedScale(QStringLiteral("rms[Rho]")) == MonitorScale::Logarithmic,
          QStringLiteral("residuals default to a logarithmic axis"));
    check(model.suggestedScale(QStringLiteral("CD")) == MonitorScale::Linear,
          QStringLiteral("coefficients default to a linear axis"));
    check(model.suggestedScale(QStringList{QStringLiteral("CD"), QStringLiteral("rms[Rho]")}) == MonitorScale::Logarithmic,
          QStringLiteral("a mixed group follows the residual rule"));

    check(model.residualColumns().size() == 4, QStringLiteral("four residual columns, got %1").arg(model.residualColumns().size()));
    check(model.residualColumns().contains(QStringLiteral("rms[Rho]")), QStringLiteral("residual list contains rms[Rho]"));
    check(model.coefficientColumns().contains(QStringLiteral("CD")), QStringLiteral("coefficient list contains CD"));
    check(!model.seriesColumns().contains(QStringLiteral("Inner_Iter")),
          QStringLiteral("axis columns are not offered as series"));
    check(model.seriesColumns().size() == 14, QStringLiteral("14 plottable columns, got %1").arg(model.seriesColumns().size()));

    // A model built from nothing must not pretend to have data.
    MonitorSeriesModel empty;
    check(!empty.isValid(), QStringLiteral("a default model is invalid"));
    check(empty.points(QStringLiteral("CD"), MonitorAxis::IterationStep).isEmpty(),
          QStringLiteral("an invalid model yields no points"));
    check(!empty.axisAvailable(MonitorAxis::IterationStep), QStringLiteral("an invalid model has no available axis"));

    // A CSV carrying a real advancing time column must enable the time axis.
    QByteArray unsteady;
    unsteady.append("Time_Iter,Outer_Iter,Inner_Iter,CD\n");
    unsteady.append("0,0,0,0.5\n");
    unsteady.append("1,0,1,0.4\n");
    unsteady.append("2,0,2,0.3\n");
    MonitorLog unsteadyLog;
    error.clear();
    if (MonitorLogReader::readCsv(unsteady, QStringLiteral("unsteady.csv"), &unsteadyLog, &error)) {
        MonitorSeriesModel unsteadyModel;
        error.clear();
        check(unsteadyModel.setLog(unsteadyLog, &error), QStringLiteral("unsteady log accepted: ") + error);
        check(unsteadyModel.axisAvailable(MonitorAxis::TimeIndex),
              QStringLiteral("a real advancing time column enables the time axis"));
        check(unsteadyModel.axisLabel(MonitorAxis::TimeIndex).contains(QStringLiteral("Time_Iter")),
              QStringLiteral("time axis is labelled with its column"));
        const QVector<QPointF> byTime = unsteadyModel.points(QStringLiteral("CD"), MonitorAxis::TimeIndex);
        check(byTime.size() == 3 && byTime.last().x() == 2.0, QStringLiteral("time axis yields three points ending at t=2"));
    } else {
        check(false, QStringLiteral("synthetic unsteady log should parse"));
    }
}

void testCurveFit()
{
    using aerosim::monitor::MonitorCurveFit;
    using aerosim::monitor::MonitorFitMethod;
    using aerosim::monitor::MonitorFitResult;

    // A straight line must be reproduced exactly by the linear fit, and the fit
    // must not extrapolate beyond the sampled x range.
    QVector<QPointF> line;
    for (int i = 0; i < 21; ++i)
        line.append(QPointF(i, 2.0 * i + 1.0));
    const MonitorFitResult linear = MonitorCurveFit::fit(line, MonitorFitMethod::Linear);
    check(linear.isValid(), QStringLiteral("a straight line yields a usable linear fit: ") + linear.note);
    check(qAbs(linear.rSquared - 1.0) < 1e-9,
          QStringLiteral("a straight line has R2=1, got %1").arg(linear.rSquared));
    check(!linear.points.isEmpty()
              && qAbs(linear.points.first().x() - line.first().x()) < 1e-9
              && qAbs(linear.points.last().x() - line.last().x()) < 1e-9,
          QStringLiteral("the fit spans exactly the sampled x range"));
    check(qAbs(linear.points.last().y() - line.last().y()) < 1e-6,
          QStringLiteral("the linear fit passes through the line's end point"));

    // Noise must lower R2 but keep the fit usable.
    QVector<QPointF> noisy = line;
    noisy[3].setY(noisy[3].y() + 5.0);
    noisy[11].setY(noisy[11].y() - 4.0);
    const MonitorFitResult noisyFit = MonitorCurveFit::fit(noisy, MonitorFitMethod::Linear);
    check(noisyFit.isValid(), QStringLiteral("a noisy line still fits"));
    check(noisyFit.rSquared < 1.0 && noisyFit.rSquared > 0.0,
          QStringLiteral("noise pushes R2 strictly between 0 and 1, got %1").arg(noisyFit.rSquared));

    // A parabola needs degree 2; requesting 2 must reach R2=1.
    QVector<QPointF> parabola;
    for (int i = 0; i < 25; ++i)
        parabola.append(QPointF(i, 0.5 * i * i - 3.0 * i + 7.0));
    MonitorFitResult quad = MonitorCurveFit::fit(parabola, MonitorFitMethod::Polynomial, 2);
    check(quad.isValid() && quad.degree == 2, QStringLiteral("a parabola fits with degree 2"));
    check(qAbs(quad.rSquared - 1.0) < 1e-6,
          QStringLiteral("a parabola has R2=1 at degree 2, got %1").arg(quad.rSquared));
    const MonitorFitResult straightTry = MonitorCurveFit::fit(parabola, MonitorFitMethod::Polynomial, 1);
    check(straightTry.isValid() && straightTry.rSquared < 0.99,
          QStringLiteral("degree 1 cannot explain a parabola"));

    // The degree is clamped to what the sample count can support, and the result
    // has to say so rather than silently returning a different model.
    QVector<QPointF> few;
    for (int i = 0; i < 4; ++i)
        few.append(QPointF(i, i * i));
    const MonitorFitResult clamped = MonitorCurveFit::fit(few, MonitorFitMethod::Polynomial, 9);
    check(clamped.degree <= 3,
          QStringLiteral("degree is clamped to n-1 samples, got %1").arg(clamped.degree));
    check(clamped.note.contains(QStringLiteral("阶数")),
          QStringLiteral("a clamped degree is reported in the note, got '%1'").arg(clamped.note));

    // The moving average covers the first and last sample by shrinking the
    // window instead of dropping the ends.
    QVector<QPointF> spiky;
    for (int i = 0; i < 15; ++i)
        spiky.append(QPointF(i, 1.0));
    spiky[7].setY(11.0);
    const MonitorFitResult average = MonitorCurveFit::fit(spiky, MonitorFitMethod::MovingAverage, 3, 5);
    check(average.isValid(), QStringLiteral("a moving average is usable: ") + average.note);
    check(qAbs(average.points.first().x() - spiky.first().x()) < 1e-9
              && qAbs(average.points.last().x() - spiky.last().x()) < 1e-9,
          QStringLiteral("the moving average still covers both ends"));
    double peak = 0.0;
    for (int i = 0; i < average.points.size(); ++i)
        peak = qMax(peak, average.points[i].y());
    check(peak < 11.0 && peak > 1.0,
          QStringLiteral("the moving average smooths the spike, peak %1").arg(peak));

    // Unsorted input is accepted and sorted, because the model never promises an
    // ordering and a solve history may be appended out of order.
    QVector<QPointF> shuffled;
    shuffled << QPointF(3, 3) << QPointF(1, 1) << QPointF(2, 2) << QPointF(0, 0);
    const MonitorFitResult ordered = MonitorCurveFit::fit(shuffled, MonitorFitMethod::Linear);
    check(ordered.isValid(), QStringLiteral("unsorted samples still fit"));
    bool ascending = true;
    for (int i = 1; i < ordered.points.size(); ++i)
        ascending = ascending && ordered.points[i].x() > ordered.points[i - 1].x();
    check(ascending, QStringLiteral("the fit comes out sorted by x"));

    // Degenerate inputs are refused with a reason rather than drawn as a bogus
    // trend.
    QVector<QPointF> tooFew;
    tooFew << QPointF(0, 0) << QPointF(1, 1);
    const MonitorFitResult fewFit = MonitorCurveFit::fit(tooFew, MonitorFitMethod::Linear);
    check(!fewFit.isValid() && !fewFit.note.isEmpty(),
          QStringLiteral("two samples are refused with a note"));

    QVector<QPointF> flatX;
    flatX << QPointF(2, 1) << QPointF(2, 2) << QPointF(2, 3) << QPointF(2, 4);
    const MonitorFitResult flatFit = MonitorCurveFit::fit(flatX, MonitorFitMethod::Linear);
    check(!flatFit.isValid(), QStringLiteral("a vertical run of samples is not fittable"));

    QVector<QPointF> withNaN;
    withNaN << QPointF(0, 0) << QPointF(1, qQNaN()) << QPointF(2, 2) << QPointF(3, 3);
    const MonitorFitResult nanFit = MonitorCurveFit::fit(withNaN, MonitorFitMethod::Linear);
    check(nanFit.isValid(), QStringLiteral("non-finite samples are skipped, not fatal"));
    check(qAbs(nanFit.rSquared - 1.0) < 1e-9,
          QStringLiteral("skipping the NaN leaves a perfect line, got %1").arg(nanFit.rSquared));

    // Asking for no fit must not invent one.
    const MonitorFitResult none = MonitorCurveFit::fit(line, MonitorFitMethod::None);
    check(!none.isValid() && none.points.isEmpty(), QStringLiteral("MonitorFitMethod::None produces no trend"));
    check(!MonitorCurveFit::methodName(MonitorFitMethod::None).isEmpty(),
          QStringLiteral("every fit method has a name"));
    check(!MonitorCurveFit::methodName(MonitorFitMethod::Polynomial).isEmpty()
              && !MonitorCurveFit::methodName(MonitorFitMethod::MovingAverage).isEmpty(),
          QStringLiteral("polynomial and moving average are named"));
    check(MonitorCurveFit::maxDegree() >= 2, QStringLiteral("the fit engine offers at least a quadratic"));
}

// One delivery of a live stream, built the way a real source would: the columns
// are classified by the same code the offline reader uses, so the two paths can
// never disagree about what is a residual or which column is an axis.
MonitorUpdate liveUpdate(const QStringList &keys, const QVector<QVector<double>> &values,
                         const QVector<qint64> &steps)
{
    MonitorUpdate update;
    for (const QString &key : keys)
        update.columns.append(MonitorLogReader::classifyColumn(key));
    update.values = values;
    update.iterationStep = steps;
    return update;
}

void testLiveAccumulator()
{
    MonitorLogAccumulator accumulator;
    QString error;

    // Two deliveries must read exactly like one whole file.
    // values is indexed [column][row], and every column must have exactly as many
    // rows as iterationStep -- that is the shape a source has to honour.
    QVector<QVector<double>> first;
    QVector<QVector<double>> second;
    for (int column = 0; column < 4; ++column) {
        first.append(QVector<double>(3, 0.0));
        second.append(QVector<double>(3, 0.0));
    }
    for (int row = 0; row < 3; ++row) {
        first[2][row] = 10.0 * (row + 1);   // CD
        second[2][row] = 10.0 * (row + 4);
        first[3][row] = 1.0;                // CL
        second[3][row] = 2.0;
    }
    const QStringList keys = {QStringLiteral("Inner_Iter"), QStringLiteral("Outer_Iter"),
                              QStringLiteral("CD"), QStringLiteral("CL")};

    check(accumulator.append(liveUpdate(keys, first, {0, 1, 2}), &error),
          QStringLiteral("the first delivery is accepted: ") + error);
    check(accumulator.rowCount() == 3, QStringLiteral("the first delivery lands 3 rows"));

    const qint64 afterFirst = accumulator.revision();
    check(accumulator.append(liveUpdate(keys, second, {3, 4, 5}), &error),
          QStringLiteral("the second delivery is accepted: ") + error);
    check(accumulator.rowCount() == 6, QStringLiteral("appends stack instead of replacing"));
    check(accumulator.revision() > afterFirst, QStringLiteral("the revision moves on every accepted append"));

    const MonitorLog &log = accumulator.log();
    check(log.values.value(QStringLiteral("CD")).size() == 6,
          QStringLiteral("the appended rows reach the column"));
    check(qAbs(log.values.value(QStringLiteral("CD")).last() - 60.0) < 1e-12,
          QStringLiteral("the appended value is the one that was sent"));

    // An empty delivery is the ordinary "nothing new" case, not an error and not a
    // row: a live caller polls far more often than the solver writes.
    MonitorUpdate empty;
    check(accumulator.append(empty, &error), QStringLiteral("an empty delivery is not an error"));
    check(accumulator.rowCount() == 6, QStringLiteral("an empty delivery adds no rows"));

    // A malformed delivery (a column whose length disagrees with the iteration
    // column) must be rejected whole, never half-applied.
    MonitorUpdate ragged = liveUpdate(keys, first, {6, 7, 8});
    ragged.values[2].removeLast();
    check(!accumulator.append(ragged, &error), QStringLiteral("a ragged delivery is rejected"));
    check(!error.isEmpty(), QStringLiteral("the rejection explains itself"));
    check(accumulator.rowCount() == 6, QStringLiteral("a rejected delivery leaves the log untouched"));

    // A column that only appears after the first deliveries is the normal way a
    // solver starts reporting a new coefficient. The rows before it are holes, not
    // zeros: a zero would draw a flat line the solver never reported.
    MonitorUpdate late;
    late.columns.append(MonitorLogReader::classifyColumn(QStringLiteral("CEff")));
    late.values.append(QVector<double>{7.0, 8.0});
    late.iterationStep = {9, 10};
    check(accumulator.append(late, &error), QStringLiteral("a late column is accepted: ") + error);
    const QVector<double> cEff = accumulator.log().values.value(QStringLiteral("CEff"));
    check(cEff.size() == 8, QStringLiteral("the late column covers every row"));
    // Rows 0..5 predate the column, so they are holes; 6 and 7 are what it reported.
    check(std::isnan(cEff.at(0)) && std::isnan(cEff.at(5)) && std::isfinite(cEff.at(6)),
          QStringLiteral("rows the late column missed are holes, not zeros"));
    check(qAbs(cEff.at(6) - 7.0) < 1e-12 && qAbs(cEff.at(7) - 8.0) < 1e-12,
          QStringLiteral("the late column starts where it appeared"));

    // A solver restart means everything gathered so far describes a run that no
    // longer exists.
    MonitorUpdate restart = liveUpdate(keys, first, {0, 1, 2});
    restart.restarted = true;
    check(accumulator.append(restart, &error), QStringLiteral("a restart is applied, not rejected"));
    check(accumulator.rowCount() == 3, QStringLiteral("a restart drops the previous run"));
    check(!accumulator.log().values.contains(QStringLiteral("CEff")),
          QStringLiteral("columns from the abandoned run do not survive the restart"));

    // A source with no iteration column fabricates the step from the row index and
    // restarts at zero in every delivery; the accumulator has to continue it.
    MonitorLogAccumulator fabricating;
    MonitorUpdate rowsOnly = liveUpdate({QStringLiteral("CD")},
                                        QVector<QVector<double>>{QVector<double>{1.0, 2.0}},
                                        {0, 1});
    rowsOnly.axisFromRowIndex = true;
    check(fabricating.append(rowsOnly, &error), QStringLiteral("a row-indexed stream is accepted"));
    check(fabricating.append(rowsOnly, &error), QStringLiteral("its next delivery is accepted"));
    // Compared element by element on purpose: QVector<T>::operator== pulls in
    // stdext::make_checked_array_iterator, which MSVC 19.51 no longer has while
    // Qt 5.12 still expands it.
    const QVector<qint64> steps = fabricating.log().iterationStep;
    const bool numbered = steps.size() == 4 && steps.at(0) == 0 && steps.at(1) == 1
                          && steps.at(2) == 2 && steps.at(3) == 3;
    check(numbered, QStringLiteral("the fabricated step continues instead of restarting"));

    // That log still plots: the fabricated step is the axis, numbered by row.
    MonitorSeriesModel model;
    QString modelError;
    check(model.setLog(fabricating.log(), &modelError),
          QStringLiteral("the accumulated log is accepted by the series model: ") + modelError);
    QString reason;
    check(model.axisAvailable(MonitorAxis::IterationStep, &reason),
          QStringLiteral("a row-indexed stream still has an iteration axis: ") + reason);
    const QVector<QPointF> plotted = model.points(QStringLiteral("CD"), MonitorAxis::IterationStep);
    check(plotted.size() == 4, QStringLiteral("all four rows reach the series model"));
    check(!plotted.isEmpty() && qAbs(plotted.last().x() - 3.0) < 1e-12,
          QStringLiteral("the fabricated axis is numbered by row"));

    // A stream that carries no time index reports that axis as missing rather
    // than inventing one.
    check(!model.axisAvailable(MonitorAxis::TimeIndex, &reason) && !reason.isEmpty(),
          QStringLiteral("a stream with no time index says so"));
}

void testFileTail()
{
    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("a temporary directory exists for the tail test"));
    const QString path = QDir(dir.path()).filePath(QStringLiteral("history.csv"));
    const QString header = QStringLiteral("Inner_Iter,Outer_Iter,CD,CL\n");

    auto appendToFile = [&path](const QString &text) {
        QFile file(path);
        if (!file.open(QIODevice::Append))
            return false;
        file.write(text.toUtf8());
        return true;
    };
    auto rewriteFile = [&path](const QString &text) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        file.write(text.toUtf8());
        return true;
    };

    check(rewriteFile(header), QStringLiteral("the followed file can be written"));

    MonitorFileTailSource source(path);
    QString error;
    check(source.start(&error), QStringLiteral("the tail source starts: ") + error);
    check(source.isRunning(), QStringLiteral("the tail source reports itself running"));

    MonitorUpdate update;
    check(source.poll(&update, &error), QStringLiteral("polling a header-only file succeeds: ") + error);
    check(update.rowCount() == 0, QStringLiteral("a header on its own yields no rows"));

    // A row that is still being written must not be parsed half-way: the source
    // keeps the partial line and waits for the newline.
    check(appendToFile(QStringLiteral("0,0,0.5,")), QStringLiteral("a partial row is appended"));
    check(source.poll(&update, &error), QStringLiteral("polling a partial row succeeds: ") + error);
    check(update.rowCount() == 0, QStringLiteral("a partial row is held back, not parsed"));

    check(appendToFile(QStringLiteral("0.9\n")), QStringLiteral("the partial row is completed"));
    check(source.poll(&update, &error), QStringLiteral("polling the completed row succeeds: ") + error);
    check(update.rowCount() == 1, QStringLiteral("the completed row is delivered exactly once"));

    // Only new bytes are read: the row count of the next poll is what was
    // appended, not everything the file holds.
    const qint64 afterFirstRow = source.consumedBytes();
    check(appendToFile(QStringLiteral("1,0,0.4,0.8\n")), QStringLiteral("a second row is appended"));
    check(source.poll(&update, &error), QStringLiteral("polling the second row succeeds: "));
    check(update.rowCount() == 1, QStringLiteral("a poll returns only what is new"));
    check(source.consumedBytes() > afterFirstRow, QStringLiteral("the read position advanced"));

    check(source.poll(&update, &error), QStringLiteral("polling with nothing new succeeds"));
    check(update.rowCount() == 0, QStringLiteral("polling again yields nothing"));

    // A file that shrinks is a solver rewriting its history: the reader rewinds
    // and says so, because the accumulated run no longer describes this file.
    check(rewriteFile(header), QStringLiteral("the followed file is truncated"));
    check(source.poll(&update, &error), QStringLiteral("polling after a truncation succeeds: ") + error);
    check(update.restarted, QStringLiteral("a shrinking file is reported as a restart"));
    check(!update.warnings.isEmpty(), QStringLiteral("the restart is explained"));

    check(source.start(&error), QStringLiteral("the tail source restarts cleanly"));
    source.stop();
    check(!source.isRunning(), QStringLiteral("stop() is honoured"));
}

// The live path and the offline path have to agree on column identity. They once
// did not: the tail source trimmed the header field but kept the quotes the
// solver writes around every name, so the live curves were called "rms[Rho]"
// with the quotes while the ones read from disk were not, and appending to the
// curves already on screen silently matched nothing.
// QVector/QList operator== for these element types trips the stdext macro that
// Qt 5.12 still expands and MSVC 19.51 no longer provides, so compare by hand.
bool sameStrings(const QStringList &left, const QStringList &right)
{
    if (left.size() != right.size())
        return false;
    for (int i = 0; i < left.size(); ++i) {
        if (left.at(i) != right.at(i))
            return false;
    }
    return true;
}

void testLiveAndOfflineAgreeOnColumnKeys()
{
    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("a temporary directory exists for the identity test"));
    const QString path = QDir(dir.path()).filePath(QStringLiteral("quoted_history.csv"));

    // Padded, quoted header plus two data rows, shaped like the SU2 writer output.
    const QString body = QStringLiteral("   \"Inner_Iter\",   \"Outer_Iter\",   \"CD\",   \"rms[Rho]\"\n"
                                        "          0,           0,     0.5,     -3.25\n"
                                        "          1,           0,     0.4,     -3.50\n");
    {
        QFile file(path);
        check(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
              QStringLiteral("the quoted fixture can be written"));
        file.write(body.toUtf8());
    }

    MonitorLog offline;
    QString error;
    check(MonitorLogReader::read(path, &offline, &error),
          QStringLiteral("the quoted fixture reads through the offline path: ") + error);

    MonitorFileTailSource source(path);
    check(source.start(&error), QStringLiteral("the tail source opens the quoted fixture: ") + error);
    MonitorUpdate update;
    check(source.poll(&update, &error),
          QStringLiteral("the quoted fixture polls through the live path: ") + error);
    check(update.rowCount() == 2, QStringLiteral("the live path delivers both rows"));

    QStringList offlineKeys;
    for (const MonitorColumn &column : offline.columns)
        offlineKeys.append(column.key);
    QStringList liveKeys;
    for (const MonitorColumn &column : update.columns)
        liveKeys.append(column.key);

    check(sameStrings(offlineKeys, liveKeys),
          QStringLiteral("both paths name the same columns (offline %1, live %2)")
              .arg(offlineKeys.join(QLatin1Char('/')))
              .arg(liveKeys.join(QLatin1Char('/'))));
    check(!liveKeys.contains(QStringLiteral("\"CD\"")),
          QStringLiteral("no column key keeps the quotes around it"));
    check(liveKeys.contains(QStringLiteral("rms[Rho]")),
          QStringLiteral("a bracketed residual column keeps its exact name"));
    bool residualMarked = false;
    for (const MonitorColumn &column : offline.columns) {
        if (column.key == QStringLiteral("rms[Rho]"))
            residualMarked = column.isResidual;
    }
    check(residualMarked, QStringLiteral("the residual is still classified as a residual"));

    // The values have to survive the same trimming: "     -3.25" is a number.
    const int cdIndex = liveKeys.indexOf(QStringLiteral("CD"));
    check(cdIndex >= 0 && update.values.size() > cdIndex,
          QStringLiteral("the live path carries a column for CD"));
    if (cdIndex >= 0 && update.values.size() > cdIndex) {
        const QVector<double> &cd = update.values.at(cdIndex);
        check(cd.size() == 2 && qAbs(cd.at(0) - 0.5) < 1e-12 && qAbs(cd.at(1) - 0.4) < 1e-12,
              QStringLiteral("padded numbers are parsed through the live path"));
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    testRealHistoryFixture();
    testHostileInputs();
    testCanRead();
    testSeriesModel();
    testCurveFit();
    testLiveAccumulator();
    testFileTail();
    testLiveAndOfflineAgreeOnColumnKeys();
    if (failures != 0) {
        QTextStream(stderr) << failures << " monitor check(s) failed\n";
        return 1;
    }
    QTextStream(stdout) << "Monitor log reader and series model OK\n";
    return 0;
}
