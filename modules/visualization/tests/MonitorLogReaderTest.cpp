#include "MonitorCurveFit.h"
#include "MonitorFileTailSource.h"
#include "MonitorLiveSource.h"
#include "MonitorLogAccumulator.h"
#include "MonitorLogReader.h"
#include "MonitorSeriesModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>

using aerosim::monitor::MonitorAxis;
using aerosim::monitor::MonitorColumn;
using aerosim::monitor::MonitorColumnRegistry;
using aerosim::monitor::MonitorColumnRules;
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

void testRegisteredColumnRules()
{
    // No convention is registered by default: the built-ins have to be exactly
    // what they were, otherwise this feature changed behaviour for everyone who
    // never asked for it.
    check(MonitorColumnRegistry::instance().registeredIds().isEmpty(),
          QStringLiteral("no column convention is registered out of the box"));
    check(!MonitorLogReader::looksLikeResidual(QStringLiteral("continuity")),
          QStringLiteral("an unknown name is not a residual until a rule says so"));
    check(!MonitorLogReader::classifyColumn(QStringLiteral("Time")).isAxis,
          QStringLiteral("an unknown name is not an axis until a rule says so"));
    check(MonitorLogReader::unitForColumn(QStringLiteral("p")).isEmpty(),
          QStringLiteral("an unknown name has no unit until a rule says so"));

    // A convention shaped like an OpenFOAM-style log: a step column called
    // "Time", a residual called "continuity", and a pressure with a unit.
    MonitorColumnRules rules;
    rules.id = QStringLiteral("test-convention");
    rules.description = QStringLiteral("a convention that only exists in this test");
    rules.isResidual = [](const QString &key) {
        return key.compare(QLatin1String("continuity"), Qt::CaseInsensitive) == 0;
    };
    rules.isAxis = [](const QString &key) {
        return key.compare(QLatin1String("Time"), Qt::CaseInsensitive) == 0;
    };
    rules.isPrimaryAxis = [](const QString &key) {
        return key.compare(QLatin1String("Time"), Qt::CaseInsensitive) == 0;
    };
    rules.unitFor = [](const QString &key) {
        if (key.compare(QLatin1String("p"), Qt::CaseInsensitive) == 0)
            return QStringLiteral("Pa");
        return QString();
    };
    MonitorColumnRegistry::instance().registerRules(rules);

    check(MonitorLogReader::looksLikeResidual(QStringLiteral("continuity")),
          QStringLiteral("a registered residual rule is honoured"));
    check(MonitorLogReader::classifyColumn(QStringLiteral("Time")).isAxis,
          QStringLiteral("a registered axis rule is honoured"));
    check(MonitorLogReader::unitForColumn(QStringLiteral("p")) == QStringLiteral("Pa"),
          QStringLiteral("a registered unit rule is honoured"));
    // The built-ins keep precedence, so registering a convention cannot quietly
    // reinterpret a header the reader already understood.
    check(MonitorLogReader::looksLikeResidual(QStringLiteral("rms[Rho]")),
          QStringLiteral("built-in residual names still work alongside a registration"));
    check(MonitorLogReader::unitForColumn(QStringLiteral("CD")) == QStringLiteral("-"),
          QStringLiteral("built-in units still work alongside a registration"));

    const QByteArray content("Time,continuity,p\n"
                             "0,1.0,101325\n"
                             "1,0.5,101300\n"
                             "2,0.25,101280\n");
    MonitorLog log;
    QString error;
    check(MonitorLogReader::readCsv(content, QStringLiteral("openfoam-ish.csv"), &log, &error),
          QStringLiteral("a registered convention makes an unfamiliar header readable"));
    if (log.columns.size() == 3) {
        check(log.columns.at(0).isAxis, QStringLiteral("the registered step column is an axis"));
        check(log.columns.at(1).isResidual, QStringLiteral("the registered residual column is a residual"));
        check(log.columns.at(2).unit == QStringLiteral("Pa"), QStringLiteral("the registered unit is applied"));
    } else {
        check(false, QStringLiteral("the unfamiliar header produced three columns"));
    }
    check(log.iterationStep.size() == 3 && log.iterationStep.at(2) == 2,
          QStringLiteral("the registered primary axis supplies the step values"));
    bool mentionedAxis = false;
    for (const QString &warning : log.warnings)
        if (warning.contains(QStringLiteral("注册规则")))
            mentionedAxis = true;
    check(mentionedAxis, QStringLiteral("using a registered axis is recorded, not silent"));

    // Probing and reading must agree: a file the probe rejects but the reader
    // accepts would let a UI hide a file it could actually draw.
    QTemporaryDir scratch;
    check(scratch.isValid(), QStringLiteral("temporary directory for the probe is available"));
    if (scratch.isValid()) {
        const QDir dir(scratch.path());
        check(writeTempFile(dir, QStringLiteral("registered.csv"), content),
              QStringLiteral("the registered-convention scratch file is written"));
        check(MonitorLogReader::canRead(dir.filePath(QStringLiteral("registered.csv"))),
              QStringLiteral("canRead() accepts a file the registered rules make readable"));
    }

    // Re-registering the same id replaces rather than accumulates.
    rules.unitFor = [](const QString &) { return QStringLiteral("m2"); };
    MonitorColumnRegistry::instance().registerRules(rules);
    check(MonitorColumnRegistry::instance().registeredIds().size() == 1,
          QStringLiteral("re-registering an id replaces it instead of accumulating"));
    check(MonitorLogReader::unitForColumn(QStringLiteral("p")) == QStringLiteral("m2"),
          QStringLiteral("the replacement rule is the one in effect"));

    MonitorColumnRegistry::instance().clearRules();
    check(MonitorColumnRegistry::instance().registeredIds().isEmpty(),
          QStringLiteral("clearRules() empties the registry"));
    check(!MonitorLogReader::looksLikeResidual(QStringLiteral("continuity")),
          QStringLiteral("clearing the registry restores the built-in-only behaviour"));
}

void testProbeAndReaderAgree()
{
    // The probe and the reader used to disagree about coefficient-only headers:
    // canRead() counted index and residual columns, readCsv() also counted any
    // column with a known unit. A file of CD,CL with no step column was reported
    // as unreadable by the probe and then read fine.
    const QByteArray coefficientsOnly("CD,CL\n"
                                      "0.08,0.31\n"
                                      "0.07,0.30\n");
    MonitorLog log;
    QString error;
    const bool readOk = MonitorLogReader::readCsv(coefficientsOnly, QStringLiteral("coeffs.csv"), &log, &error);
    check(readOk, QStringLiteral("a coefficient-only file is readable"));

    QTemporaryDir scratch;
    check(scratch.isValid(), QStringLiteral("temporary directory for the agreement check is available"));
    if (scratch.isValid()) {
        const QDir dir(scratch.path());
        check(writeTempFile(dir, QStringLiteral("coeffs.csv"), coefficientsOnly),
              QStringLiteral("the coefficient-only scratch file is written"));
        const bool probeOk = MonitorLogReader::canRead(dir.filePath(QStringLiteral("coeffs.csv")));
        check(probeOk == readOk,
              QStringLiteral("canRead() and readCsv() agree on a coefficient-only header"));
        check(probeOk, QStringLiteral("both accept it, rather than the probe hiding it"));
    }

    // And the refusal has to agree too: a header with nothing recognisable is
    // rejected by both, not accepted by one and refused by the other.
    const QByteArray generic("a,b\n"
                             "1,2\n");
    MonitorLog genericLog;
    QString genericError;
    const bool genericRead = MonitorLogReader::readCsv(generic, QStringLiteral("generic.csv"),
                                                       &genericLog, &genericError);
    check(!genericRead, QStringLiteral("a generic CSV is refused by the reader"));
    if (scratch.isValid()) {
        const QDir dir(scratch.path());
        check(writeTempFile(dir, QStringLiteral("generic.csv"), generic),
              QStringLiteral("the generic scratch file is written"));
        check(!MonitorLogReader::canRead(dir.filePath(QStringLiteral("generic.csv"))),
              QStringLiteral("and the probe refuses it too"));
    }
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

    // A degree-9 fit through exactly ten samples is the worst-conditioned case the
    // library allows (the degree is clamped to n-1). The data comes from a known
    // degree-9 polynomial, so a correct solve has to reproduce it; this is the one
    // assertion that tells the QR solve apart from forming the normal equations,
    // which throws away roughly four digits here. Measured on this fixture: the
    // normal equations land at 3.6e-12, the QR solve at 2.7e-15.
    const auto worstCasePolynomial = [](double x) {
        const double t = x / 100.0;
        const double coefficients[10] = {1.7, -0.9, 2.3, 1.4, -2.1, 0.8, 1.9, -1.3, 0.6, 2.2};
        double value = 0.0;
        double power = 1.0;
        for (int i = 0; i < 10; ++i) {
            value += coefficients[i] * power;
            power *= t;
        }
        return value;
    };
    QVector<QPointF> exact;
    for (int i = 0; i < 10; ++i)
        exact.append(QPointF(i * 10.0, worstCasePolynomial(i * 10.0)));
    const MonitorFitResult recovered = MonitorCurveFit::fit(exact, MonitorFitMethod::Polynomial, 9);
    check(recovered.isValid(), QStringLiteral("ten samples support a degree-9 fit: ") + recovered.note);
    double worstError = 0.0;
    for (const QPointF &p : recovered.points)
        worstError = qMax(worstError, qAbs(p.y() - worstCasePolynomial(p.x())));
    check(worstError < 1e-12,
          QStringLiteral("a degree-9 fit reproduces its own polynomial (max error %1, "
                         "the normal equations reach 3.6e-12 and fail this)")
              .arg(worstError));

    // A degree the samples cannot support has to be refused, not answered with a
    // curve whose real degree is lower than the one it claims. Eight samples over
    // five distinct x values cannot carry a degree-7 polynomial.
    QVector<QPointF> repeated;
    const double repeatedX[8] = {0.0, 1.0, 1.0, 2.0, 2.0, 2.0, 3.0, 4.0};
    for (int i = 0; i < 8; ++i)
        repeated.append(QPointF(repeatedX[i], 1.0 + 0.1 * i));
    const MonitorFitResult unreachable = MonitorCurveFit::fit(repeated, MonitorFitMethod::Polynomial, 7);
    check(!unreachable.isValid(),
          QStringLiteral("a rank-deficient design is refused rather than faked"));
    check(!unreachable.note.isEmpty(),
          QStringLiteral("refusing an unreachable degree explains why"));

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

// The live path has to refuse the bytes the offline path refuses. It once accepted
// everything: a history file with a NUL in the middle was tailed happily and drew
// curves from a file nothing else could read.
void testLiveRefusesNonText()
{
    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("a temporary directory exists for the text guard test"));
    if (!dir.isValid())
        return;
    const QDir scratch(dir.path());

    // 1) Binary content, in the middle of an otherwise ordinary file.
    QByteArray binary("Inner_Iter,CD\n0,0.5\n1,0.4\n");
    binary.append('\0');
    binary.append("2,0.3\n");
    const QString binaryPath = scratch.filePath(QStringLiteral("binary.csv"));
    check(writeTempFile(scratch, QStringLiteral("binary.csv"), binary),
          QStringLiteral("the binary history file is written"));

    // The offline reader is the reference for what these bytes mean.
    MonitorLog offline;
    QString offlineError;
    check(!MonitorLogReader::readCsv(binary, binaryPath, &offline, &offlineError),
          QStringLiteral("the offline reader refuses a file with a NUL in it"));

    MonitorFileTailSource source(binaryPath);
    QString error;
    check(source.start(&error), QStringLiteral("the tail source starts on that file: ") + error);
    MonitorUpdate update;
    check(!source.poll(&update, &error), QStringLiteral("the live path refuses the same binary file"));
    check(error.contains(QStringLiteral("二进制")),
          QStringLiteral("the live refusal names the binary content, like the offline one"));
    check(update.rowCount() == 0, QStringLiteral("nothing is delivered from a refused file"));

    // Sticky: the file is not going to heal, so a later poll must not look healthy
    // just because the rows after the bad bytes happen to parse.
    error.clear();
    check(!source.poll(&update, &error), QStringLiteral("the refusal is not forgotten on the next poll"));
    check(!error.isEmpty(), QStringLiteral("the repeated refusal still explains itself"));

    // 2) Bytes that are not UTF-8. These decode to U+FFFD rather than failing, so a
    // reader that trusts fromUtf8() keeps going and mislabels every column.
    QByteArray badUtf8("Inner_Iter,CD\n0,0.5\n");
    badUtf8.append('\xE4');
    badUtf8.append('\xB8');
    badUtf8.append(",0.4\n");     // truncated three-byte sequence
    badUtf8.append("2,0.3\n");
    const QString utf8Path = scratch.filePath(QStringLiteral("badutf8.csv"));
    check(writeTempFile(scratch, QStringLiteral("badutf8.csv"), badUtf8),
          QStringLiteral("the badly encoded history file is written"));
    check(!MonitorLogReader::readCsv(badUtf8, utf8Path, &offline, &offlineError),
          QStringLiteral("the offline reader refuses text that is not UTF-8"));

    MonitorFileTailSource utf8Source(utf8Path);
    error.clear();
    check(utf8Source.start(&error), QStringLiteral("the tail source starts on the bad text: ") + error);
    check(!utf8Source.poll(&update, &error), QStringLiteral("the live path refuses text that is not UTF-8"));
    check(error.contains(QStringLiteral("UTF-8")),
          QStringLiteral("the live refusal names the encoding, like the offline one"));
}

// A registered convention's own step column has to drive the live axis as well. The
// tail used to look only for the built-in names, so every row of an OpenFOAM-style
// log arrived with step zero and every curve collapsed onto one vertical line.
void testLiveRegisteredPrimaryAxis()
{
    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("a temporary directory exists for the registered axis test"));
    if (!dir.isValid())
        return;
    const QDir scratch(dir.path());

    MonitorColumnRules rules;
    rules.id = QStringLiteral("test-convention-live");
    rules.description = QStringLiteral("a convention that only exists in this test");
    rules.isResidual = [](const QString &key) {
        return key.compare(QLatin1String("continuity"), Qt::CaseInsensitive) == 0;
    };
    rules.isAxis = [](const QString &key) {
        return key.compare(QLatin1String("Time"), Qt::CaseInsensitive) == 0;
    };
    rules.isPrimaryAxis = [](const QString &key) {
        return key.compare(QLatin1String("Time"), Qt::CaseInsensitive) == 0;
    };
    rules.unitFor = [](const QString &key) {
        if (key.compare(QLatin1String("p"), Qt::CaseInsensitive) == 0)
            return QStringLiteral("Pa");
        return QString();
    };
    MonitorColumnRegistry::instance().registerRules(rules);

    // No built-in index column at all, so only the registration can name the axis.
    // The step values deliberately do not look like row numbers: a Time column of
    // 0,1,2 is indistinguishable from the row-index fallback, which is exactly the
    // bug being tested and would let a broken implementation pass.
    const QByteArray content("Time,continuity,p\n"
                             "10,1.0,101325\n"
                             "20,0.5,101300\n"
                             "30,0.25,101280\n");
    const QString path = scratch.filePath(QStringLiteral("openfoam-ish.csv"));
    check(writeTempFile(scratch, QStringLiteral("openfoam-ish.csv"), content),
          QStringLiteral("the registered-convention history file is written"));

    MonitorLog offline;
    QString error;
    check(MonitorLogReader::readCsv(content, path, &offline, &error),
          QStringLiteral("the offline reader reads the registered convention: ") + error);

    MonitorFileTailSource source(path);
    error.clear();
    check(source.start(&error), QStringLiteral("the tail source starts on the registered file: ") + error);
    MonitorUpdate update;
    check(source.poll(&update, &error), QStringLiteral("polling the registered file succeeds: ") + error);
    check(update.rowCount() == 3, QStringLiteral("the registered file delivers its three rows"));

    // Compared element by element: Qt 5.12's QVector<qint64>::operator== pulls in
    // stdext, which MSVC 19.51 removed, so the comparison has to be explicit.
    bool sameAxis = update.iterationStep.size() == offline.iterationStep.size();
    if (sameAxis) {
        for (int row = 0; row < offline.iterationStep.size(); ++row) {
            if (update.iterationStep.at(row) != offline.iterationStep.at(row))
                sameAxis = false;
        }
    }
    check(sameAxis, QStringLiteral("the live axis matches the offline axis on a registered convention"));
    check(update.iterationStep.size() == 3 && update.iterationStep.at(0) == 10
              && update.iterationStep.at(1) == 20 && update.iterationStep.at(2) == 30,
          QStringLiteral("the live axis carries the registered column's own values, "
                         "not the row-index fallback"));
    bool mentionedRule = false;
    for (const QString &warning : update.warnings) {
        if (warning.contains(QStringLiteral("注册规则")))
            mentionedRule = true;
    }
    check(mentionedRule, QStringLiteral("the live path records that a registered column became the axis"));

    // The built-ins still win: a file where Inner_Iter really advances must take the
    // steps from Inner_Iter, not from the registered Time column.
    const QByteArray builtIn("Inner_Iter,Time,continuity\n"
                             "5,900,1.0\n"
                             "6,901,0.5\n");
    const QString builtInPath = scratch.filePath(QStringLiteral("builtin-wins.csv"));
    check(writeTempFile(scratch, QStringLiteral("builtin-wins.csv"), builtIn),
          QStringLiteral("the built-in-plus-registered history file is written"));
    MonitorFileTailSource builtInSource(builtInPath);
    error.clear();
    check(builtInSource.start(&error),
          QStringLiteral("the tail source starts on the built-in-plus-registered file: ") + error);
    check(builtInSource.poll(&update, &error),
          QStringLiteral("polling the built-in-plus-registered file succeeds: ") + error);
    check(update.iterationStep.size() == 2 && update.iterationStep.at(0) == 5
              && update.iterationStep.at(1) == 6,
          QStringLiteral("a built-in index column keeps precedence over a registered one"));

    MonitorColumnRegistry::instance().clearRules();
}

// Progression used to be recomputed from row one on every delivery, so the cost of
// a stream grew with the square of its length: measured on this machine, 40000 rows
// arriving one at a time took over two seconds and kept climbing.
void testProgressionIsIncremental()
{
    MonitorLogAccumulator accumulator;
    QString error;
    accumulator.setDatasetId(QStringLiteral("live"));

    // One row per delivery: the shape that makes a rescan cost everything it has.
    // The cost is measured at two sizes and compared, not checked against a wall
    // clock: an absolute bound would fail on a slower machine while telling us
    // nothing about whether the algorithm changed, whereas doubling the row count
    // separates linear (about twice the time) from quadratic (about four times).
    auto streamRows = [](MonitorLogAccumulator *target, int rows) {
        QString failure;
        bool appended = true;
        QElapsedTimer timer;
        timer.start();
        for (int line = 0; line < rows && appended; ++line) {
            QVector<QVector<double>> values(3);
            values[0].append(static_cast<double>(line));   // Inner_Iter: advances every row
            values[1].append(0.0);                         // Outer_Iter: constant
            values[2].append(0.5 - 0.00001 * line);        // CD: an ordinary column
            QVector<qint64> steps;
            steps.append(line);
            const MonitorUpdate update =
                liveUpdate({QStringLiteral("Inner_Iter"), QStringLiteral("Outer_Iter"), QStringLiteral("CD")},
                           values, steps);
            if (!target->append(update, &failure))
                appended = false;
        }
        return appended ? timer.elapsed() : -1;
    };

    const int small = 10000;
    const qint64 smallMs = streamRows(&accumulator, small);
    check(smallMs >= 0, QStringLiteral("the row-by-row stream is accepted"));
    check(accumulator.rowCount() == small, QStringLiteral("every delivered row is kept"));

    // The counts must stay right, not merely get fast: the first row is the
    // reference, so every later one counts when it differs from it.
    check(accumulator.log().innerIterationProgression == small - 1,
          QStringLiteral("a strictly advancing column counts every row after the first"));
    check(accumulator.log().outerIterationProgression == 0,
          QStringLiteral("a constant column has no progression"));

    MonitorLogAccumulator doubled;
    doubled.setDatasetId(QStringLiteral("live"));
    const qint64 doubledMs = streamRows(&doubled, small * 2);
    check(doubledMs >= 0, QStringLiteral("the doubled stream is accepted"));
    check(doubled.log().innerIterationProgression == small * 2 - 1,
          QStringLiteral("the counts stay right at twice the length"));

    // Rescanning the whole column per delivery makes this ratio approach four.
    // Anything under three still separates the two shapes with room for timer noise.
    if (smallMs > 0 && doubledMs >= 0) {
        check(doubledMs < smallMs * 3,
              QStringLiteral("doubling the row count does not quadruple the cost "
                             "(%1 ms -> %2 ms, so progression is not being rescanned)")
                  .arg(smallMs)
                  .arg(doubledMs));
    }

    // A restart rewinds the rows, so the memo has to go with them. A stale "already
    // scanned" count would keep counting from where the previous run stopped.
    MonitorUpdate restart;
    restart.columns = {MonitorLogReader::classifyColumn(QStringLiteral("Inner_Iter")),
                       MonitorLogReader::classifyColumn(QStringLiteral("Outer_Iter")),
                       MonitorLogReader::classifyColumn(QStringLiteral("CD"))};
    restart.values = QVector<QVector<double>>(3);
    restart.values[0].append(100.0);
    restart.values[1].append(0.0);
    restart.values[2].append(0.5);
    restart.iterationStep.append(100);
    restart.restarted = true;
    check(accumulator.append(restart, &error),
          QStringLiteral("a restart is accepted: ") + error);
    check(accumulator.rowCount() == 1, QStringLiteral("a restart drops the previous run"));
    check(accumulator.log().innerIterationProgression == 0,
          QStringLiteral("a single row cannot advance the axis, however many rows came before"));
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

// The two readers used to disagree about which data rows are admissible: the
// offline path dropped a row with too few fields and said so, while the tail
// padded it with missing values and said nothing. The same file therefore
// produced two different series depending on how it was read.
void testLiveAndOfflineAgreeOnRowAdmission()
{
    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("a temporary directory exists for the admission test"));
    if (!dir.isValid())
        return;
    const QString path = QDir(dir.path()).filePath(QStringLiteral("ragged_history.csv"));

    // Every way a row can disagree with the header, in one file: a short row, a
    // long row, an unparsable cell and a quoted number.
    const QString body = QStringLiteral("Inner_Iter,CD,CL\n"
                                        "0,0.5,0.9\n"
                                        "1,0.4\n"
                                        "2,0.3,0.8,0.7\n"
                                        "3,not-a-number,0.7\n"
                                        "4,\"0.25\",0.6\n");
    {
        QFile file(path);
        check(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
              QStringLiteral("the ragged fixture can be written"));
        file.write(body.toUtf8());
    }

    MonitorLog offline;
    QString error;
    check(MonitorLogReader::read(path, &offline, &error),
          QStringLiteral("the ragged fixture reads offline: ") + error);

    MonitorFileTailSource source(path);
    check(source.start(&error), QStringLiteral("the tail source opens the ragged fixture: ") + error);
    MonitorUpdate update;
    check(source.poll(&update, &error),
          QStringLiteral("the ragged fixture polls through the live path: ") + error);

    // The short row is the one that decides it. Both paths must drop it, so both
    // must end up with the same number of usable rows.
    check(offline.iterationStep.size() == 4,
          QStringLiteral("offline drops the short row, leaving %1").arg(offline.iterationStep.size()));
    check(update.rowCount() == offline.iterationStep.size(),
          QStringLiteral("the live path drops the same row, got %1 against %2")
              .arg(update.rowCount()).arg(offline.iterationStep.size()));
    check(source.skippedRows() == 1,
          QStringLiteral("the live source counts the dropped row, got %1").arg(source.skippedRows()));
    check(source.raggedRows() == 2,
          QStringLiteral("the live source counts both field-count mismatches, got %1").arg(source.raggedRows()));

    // Dropping must never be silent on either side.
    check(!update.warnings.isEmpty(), QStringLiteral("the live path explains the dropped row"));
    bool offlineSaidSo = false;
    for (const QString &warning : offline.warnings) {
        if (warning.contains(QStringLiteral("已跳过")))
            offlineSaidSo = true;
    }
    check(offlineSaidSo, QStringLiteral("the offline path explains the dropped row"));

    // A quoted number is a number: both paths have to read the same value, which
    // they did not while one of them trimmed padding but not the quotes.
    const QVector<double> offlineValues = offline.values.value(QStringLiteral("CD"));
    check(offlineValues.size() == 4, QStringLiteral("offline keeps four CD values"));
    if (offlineValues.size() == 4 && update.values.size() >= 2) {
        const QVector<double> &liveValues = update.values.at(1);
        check(liveValues.size() == offlineValues.size(),
              QStringLiteral("both paths carry the same number of CD values"));
        for (int row = 0; row < offlineValues.size() && row < liveValues.size(); ++row) {
            const bool offlineNan = std::isnan(offlineValues.at(row));
            const bool liveNan = std::isnan(liveValues.at(row));
            if (offlineNan != liveNan)
                check(false, QStringLiteral("row %1 disagrees about a missing CD value").arg(row));
            if (!offlineNan && qAbs(offlineValues.at(row) - liveValues.at(row)) > 1e-12)
                check(false, QStringLiteral("row %1 disagrees on CD").arg(row));
        }
        check(qAbs(offlineValues.at(3) - 0.25) < 1e-12,
              QStringLiteral("a quoted number is read as a number"));
    }
}

// A header field with no name, and a name that appears more than once, both used
// to make the reader drop a column and then slice every data row by the reduced
// column count. The fields to the right of the dropped one were then read one
// position to the left: CD silently received CL's value, and a repeated name
// overwrote the earlier column instead of being kept apart.
void testDuplicateAndNamelessHeaderFieldsKeepTheirColumns()
{
    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("a temporary directory exists for the header test"));
    if (!dir.isValid())
        return;

    const auto read = [&dir](const QString &name, const QByteArray &body, MonitorLog *log) {
        const QString path = QDir(dir.path()).filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return QStringLiteral("cannot write ") + name;
        file.write(body);
        file.close();
        QString error;
        const bool ok = MonitorLogReader::read(path, log, &error);
        return ok ? QString() : error;
    };

    // A nameless field must leave a hole rather than shift CD left.
    {
        MonitorLog log;
        const QString error = read(QStringLiteral("nameless.csv"),
                                   "Inner_Iter,,CD\n1,0.5,1.5\n2,0.6,1.6\n", &log);
        check(error.isEmpty(), QStringLiteral("a header with a nameless field reads: ") + error);
        const QVector<double> cd = log.values.value(QStringLiteral("CD"));
        check(cd.size() == 2,
              QStringLiteral("CD survives a nameless field to its left, size %1").arg(cd.size()));
        if (cd.size() == 2) {
            check(qAbs(cd.at(0) - 1.5) < 1e-12,
                  QStringLiteral("CD keeps its own column, got %1 where 1.5 was written").arg(cd.at(0)));
            check(qAbs(cd.at(1) - 1.6) < 1e-12,
                  QStringLiteral("CD keeps its own column on row 2, got %1").arg(cd.at(1)));
        }
    }

    // Three columns sharing one name: every one of them has to arrive.
    {
        MonitorLog log;
        const QString error = read(QStringLiteral("duplicates.csv"),
                                   "Inner_Iter,rms[Rho],CD,rms[Rho],CL,rms[Rho]\n"
                                   "1,0.1,0.5,0.2,1.5,0.3\n", &log);
        check(error.isEmpty(), QStringLiteral("a header with repeated names reads: ") + error);
        int duplicates = 0;
        for (const MonitorColumn &column : log.columns) {
            if (column.key.startsWith(QStringLiteral("rms[Rho]")))
                ++duplicates;
        }
        check(duplicates == 3,
              QStringLiteral("all three rms[Rho] columns survive, kept %1").arg(duplicates));
        check(log.columns.size() == 6,
              QStringLiteral("no column is dropped from the header, kept %1").arg(log.columns.size()));

        // The values must not overwrite each other: 0.1, 0.2 and 0.3 are three
        // different measurements and the last one used to win.
        const auto value = [&log](const QString &key) {
            const QVector<double> v = log.values.value(key);
            return v.isEmpty() ? -999.0 : v.first();
        };
        check(qAbs(value(QStringLiteral("rms[Rho]")) - 0.1) < 1e-12,
              QStringLiteral("the first rms[Rho] keeps 0.1, got %1").arg(value(QStringLiteral("rms[Rho]"))));
        check(qAbs(value(QStringLiteral("rms[Rho]#2")) - 0.2) < 1e-12,
              QStringLiteral("the second rms[Rho] keeps 0.2, got %1").arg(value(QStringLiteral("rms[Rho]#2"))));
        check(qAbs(value(QStringLiteral("rms[Rho]#3")) - 0.3) < 1e-12,
              QStringLiteral("the third rms[Rho] keeps 0.3, got %1").arg(value(QStringLiteral("rms[Rho]#3"))));
        check(qAbs(value(QStringLiteral("CL")) - 1.5) < 1e-12,
              QStringLiteral("CL still reads its own column, got %1").arg(value(QStringLiteral("CL"))));

        // A silent rename would be as bad as a silent drop.
        bool saidSo = false;
        for (const QString &warning : log.warnings) {
            if (warning.contains(QStringLiteral("出现了")))
                saidSo = true;
        }
        check(saidSo, QStringLiteral("a renamed duplicate column is explained in a warning"));
    }
}

// A header ending in a separator declares a trailing empty field that carries no
// data. Trimming it from the map is correct, but the rows still end with the same
// separator, so counting fields against the trimmed map reported every row of a
// perfectly regular file as ragged.
void testTrailingSeparatorIsNotRagged()
{
    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("a temporary directory exists for the trailing field test"));
    if (!dir.isValid())
        return;
    const QString path = QDir(dir.path()).filePath(QStringLiteral("trailing.csv"));
    {
        QFile file(path);
        check(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
              QStringLiteral("the trailing-comma fixture can be written"));
        file.write("Inner_Iter,CD,\n1,0.5,\n2,0.6,\n");
    }

    MonitorLog log;
    QString error;
    check(MonitorLogReader::read(path, &log, &error),
          QStringLiteral("a header ending in a separator reads: ") + error);
    check(log.iterationStep.size() == 2,
          QStringLiteral("both rows survive, got %1").arg(log.iterationStep.size()));
    check(log.columns.size() == 2,
          QStringLiteral("the trailing empty field adds no column, got %1").arg(log.columns.size()));

    bool complained = false;
    for (const QString &warning : log.warnings) {
        if (warning.contains(QStringLiteral("字段数与表头不一致")))
            complained = true;
    }
    check(!complained, QStringLiteral("a regular file with a trailing separator is not called ragged"));
}

// The offline reader rounds a fractional iteration step and the live one used to
// truncate it, so thesame file produced two different x coordinates.
void testFractionalIterationStepAgrees()
{
    QTemporaryDir dir;
    check(dir.isValid(), QStringLiteral("a temporary directory exists for the step test"));
    if (!dir.isValid())
        return;
    const QString path = QDir(dir.path()).filePath(QStringLiteral("fractional.csv"));
    {
        QFile file(path);
        check(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
              QStringLiteral("the fractional fixture can be written"));
        file.write("Inner_Iter,CD\n1.9,0.5\n2.9,0.6\n3.9,0.7\n");
    }

    MonitorLog offline;
    QString error;
    check(MonitorLogReader::read(path, &offline, &error),
          QStringLiteral("a fractional iteration column reads offline: ") + error);

    MonitorFileTailSource source(path);
    check(source.start(&error), QStringLiteral("the fractional fixture opens live: ") + error);
    MonitorLogAccumulator accumulator;
    accumulator.setDatasetId(QStringLiteral("fractional"));
    for (;;) {
        MonitorUpdate update;
        if (!source.poll(&update, &error))
            break;
        if (update.isEmpty())
            break;
        accumulator.append(update, &error);
    }
    source.stop();
    const MonitorLog &live = accumulator.log();

    // Compared element by element: QVector's operator== pulls in a std::ext
    // helper that this Qt/MSVC pair no longer provides.
    bool stepsAgree = offline.iterationStep.size() == live.iterationStep.size();
    for (int row = 0; stepsAgree && row < offline.iterationStep.size(); ++row)
        stepsAgree = offline.iterationStep.at(row) == live.iterationStep.at(row);
    check(stepsAgree,
          QStringLiteral("both paths agree on the iteration step (offline %1 first, live %2)")
              .arg(offline.iterationStep.value(0))
              .arg(live.iterationStep.value(0)));
    check(!offline.iterationStep.isEmpty() && offline.iterationStep.first() == 2,
          QStringLiteral("1.9 rounds to 2, got %1").arg(offline.iterationStep.value(0)));
}

// The fit reports the degree and window it applied; it used to report the ones it
// was asked for, so a clamped request printed "degree 20" beside a 9th degree
// curve and "window 4" beside an odd width of 3.
void testFitReportsTheAppliedParameters()
{
    using aerosim::monitor::MonitorCurveFit;
    using aerosim::monitor::MonitorFitMethod;
    using aerosim::monitor::MonitorFitResult;

    QVector<QPointF> samples;
    for (int i = 0; i < 100; ++i)
        samples.append(QPointF(i, std::sin(i * 0.1) + i * 0.01));

    const MonitorFitResult poly = MonitorCurveFit::fit(samples, MonitorFitMethod::Polynomial, 20);
    check(poly.isValid(), QStringLiteral("a 20th degree request still yields a fit: ") + poly.note);
    check(poly.degree == MonitorCurveFit::maxDegree(),
          QStringLiteral("the reported degree is the clamped one, got %1 of a maximum %2")
              .arg(poly.degree).arg(MonitorCurveFit::maxDegree()));
    check(poly.degree != 20, QStringLiteral("a clamped fit does not report the requested degree"));

    // An even window is narrowed to an odd one; the note says so and the field
    // now agrees with it.
    const MonitorFitResult averaged = MonitorCurveFit::fit(samples, MonitorFitMethod::MovingAverage, 3, 4);
    check(averaged.isValid(), QStringLiteral("an even window still smooths: ") + averaged.note);
    check(averaged.window == 3,
          QStringLiteral("the reported window is the odd one applied, got %1").arg(averaged.window));
    check(averaged.note.contains(QStringLiteral("调整为 %1").arg(averaged.window)),
          QStringLiteral("the note names the same window as the field, got '%1'").arg(averaged.note));
}

} // namespace

// Tecplot ASCII is the format XCFD writes its history curves in, and it differs
// from CSV in every respect that matters: whitespace-delimited values, a record
// header instead of a first line, one ZONE per restart, and Fortran exponents
// when the solver that wrote it was Fortran. Each of those is checked against
// real numbers rather than against "it parsed".
void testTecplotReader()
{
    const QByteArray fixture = fixtureBytes(QStringLiteral("xcfd_history_tec.tec"));
    check(!fixture.isEmpty(), QStringLiteral("the Tecplot fixture is readable"));

    MonitorLog log;
    QString message;
    check(MonitorLogReader::readTecplot(fixture, QStringLiteral("xcfd_history_tec.tec"), &log, &message),
          QStringLiteral("the Tecplot fixture parses: %1").arg(message));

    // Two ZONE records, five steps each: a restart continues the run, it does not
    // start a second one, so the rows are concatenated in file order.
    check(log.iterationStep.size() == 10,
          QStringLiteral("both ZONE records are read as one series (%1 rows)").arg(log.iterationStep.size()));
    check(log.columns.size() == 5,
          QStringLiteral("the five declared variables become five columns (%1)").arg(log.columns.size()));
    check(log.datasetId == QStringLiteral("xcfd_history_tec"),
          QStringLiteral("the dataset id is the file stem"));

    const QVector<double> rho = log.values.value(QStringLiteral("rms[Rho]"));
    check(rho.size() == 10, QStringLiteral("rms[Rho] carries every row"));
    if (rho.size() == 10) {
        // "D" exponents: a Fortran writer's -1.60335846D+00 is the double -1.60335846.
        // Qt's own parser rejects the D outright, so this value proves the rewrite.
        check(std::fabs(rho.at(0) - (-1.60335846)) < 1e-9,
              QStringLiteral("a Fortran D exponent is read as a number (%1)").arg(rho.at(0)));
        check(std::fabs(rho.at(9) - (-2.243609751)) < 1e-9,
              QStringLiteral("the second ZONE's last row is appended, not dropped (%1)").arg(rho.at(9)));
    }
    const QVector<double> cEff = log.values.value(QStringLiteral("CEff"));
    check(cEff.size() == 10 && std::fabs(cEff.at(0) - 4.906209834) < 1e-9,
          QStringLiteral("a positive-D exponent column is read too"));
    // The key column is the axis the file itself declares; with no Outer_Iter or
    // Inner_Iter the reader falls back to the row index, which here coincides with
    // the step values the file writes.
    check(log.columns.at(0).isAxis, QStringLiteral("the declared Iter column is classified as an axis"));

    // --- BLOCK packing: the same numbers, grouped by variable ---
    // Tecplot lets a solver write all of one variable before the next; a reader
    // that assumed point-major would silently transpose the file.
    QByteArray block("TITLE = \"block packed\"\n"
                     "VARIABLES = \"Inner_Iter\" \"CD\" \"CL\"\n"
                     "ZONE I=3, DATAPACKING=BLOCK\n"
                     "0 1 2\n"
                     "0.08 0.04 0.02\n"
                     "0.39 0.60 0.76\n");
    MonitorLog blockLog;
    check(MonitorLogReader::readTecplot(block, QStringLiteral("block.tec"), &blockLog, &message),
          QStringLiteral("a BLOCK-packed zone parses: %1").arg(message));
    const QVector<double> blockCd = blockLog.values.value(QStringLiteral("CD"));
    const QVector<double> blockCl = blockLog.values.value(QStringLiteral("CL"));
    check(blockCd.size() == 3 && std::fabs(blockCd.at(2) - 0.02) < 1e-12,
          QStringLiteral("BLOCK packing reads column by column (%1)").arg(blockCd.value(2)));
    check(blockCl.size() == 3 && std::fabs(blockCl.at(0) - 0.39) < 1e-12,
          QStringLiteral("BLOCK packing keeps the variable order (%1)").arg(blockCl.value(0)));
    // An Inner_Iter column is the built-in axis, so the x values come from the
    // file rather than from the row index.
    check(blockLog.iterationStep.size() == 3 && blockLog.iterationStep.at(2) == 2,
          QStringLiteral("a declared Inner_Iter column supplies the x axis"));

    // --- a repeated variable name ---
    // Two columns would otherwise overwrite each other in the values map, which
    // is silent corruption: the same renaming the CSV header path applies.
    QByteArray duplicate("VARIABLES = \"Inner_Iter\" \"CD\" \"CD\"\n"
                         "ZONE I=2\n"
                         "0 0.08 0.09\n"
                         "1 0.04 0.05\n");
    MonitorLog duplicateLog;
    check(MonitorLogReader::readTecplot(duplicate, QStringLiteral("dup.tec"), &duplicateLog, &message),
          QStringLiteral("a repeated variable name parses: %1").arg(message));
    check(duplicateLog.columns.size() == 3
              && duplicateLog.columns.at(2).key == QStringLiteral("CD#2"),
          QStringLiteral("the second CD is renamed rather than dropped"));
    check(!duplicateLog.warnings.filter(QStringLiteral("CD")).isEmpty(),
          QStringLiteral("the renaming is reported, not just applied"));

    // --- a non-ORDERED zone ---
    // A FELINESEG zone is connectivity, not a per-step record. Reading its values
    // as steps would draw a history the solver never wrote.
    QByteArray finiteElement("VARIABLES = \"Inner_Iter\" \"CD\"\n"
                             "ZONE N=3, E=1, DATAPACKING=POINT, ZONETYPE=FELINESEG\n"
                             "0 0.1\n1 0.2\n2 0.3\n"
                             "1 3\n");
    MonitorLog feLog;
    const bool feOk = MonitorLogReader::readTecplot(finiteElement, QStringLiteral("fe.tec"), &feLog, &message);
    check(!feOk || feLog.iterationStep.isEmpty(),
          QStringLiteral("a finite-element zone is not read as a history"));

    // --- read() picks the reader from the content, not the suffix ---
    QTemporaryDir scratch;
    check(scratch.isValid(), QStringLiteral("temporary directory is available"));
    if (scratch.isValid()) {
        const QDir dir(scratch.path());
        // .dat is written by both formats, so the dispatch cannot be by name.
        check(writeTempFile(dir, QStringLiteral("tecplot.dat"), fixture),
              QStringLiteral("the Tecplot fixture is copied under a .dat name"));
        MonitorLog dispatched;
        check(MonitorLogReader::read(dir.filePath(QStringLiteral("tecplot.dat")), &dispatched, &message)
                  && dispatched.iterationStep.size() == 10,
              QStringLiteral("read() dispatches a .dat by its content: %1").arg(message));
        check(MonitorLogReader::canRead(dir.filePath(QStringLiteral("tecplot.dat"))),
              QStringLiteral("canRead() accepts the same .dat file"));

        // A CSV whose first column is literally called ZONE must not be mistaken
        // for a Tecplot record header.
        const QByteArray zoneNamedCsv("ZONE,CD,CL\n0,0.08,0.39\n1,0.04,0.60\n");
        check(writeTempFile(dir, QStringLiteral("zone_named.csv"), zoneNamedCsv),
              QStringLiteral("the zone-named CSV is written"));
        MonitorLog zoneNamed;
        check(MonitorLogReader::read(dir.filePath(QStringLiteral("zone_named.csv")), &zoneNamed, &message),
              QStringLiteral("a CSV with a ZONE column still reads as CSV: %1").arg(message));
        check(zoneNamed.columns.size() == 3 && zoneNamed.columns.at(0).key == QStringLiteral("ZONE"),
              QStringLiteral("its columns are the CSV header's, not a Tecplot variable list"));

        // A Tecplot file whose variables are an unrelated result has nothing to
        // plot against an iteration axis, and the dialog must not offer it.
        const QByteArray unrelated("TITLE = \"mesh\"\n"
                                   "VARIABLES = \"X\" \"Y\" \"Z\" \"Pressure\"\n"
                                   "ZONE I=2\n1 2 3 4\n5 6 7 8\n");
        check(writeTempFile(dir, QStringLiteral("mesh.tec"), unrelated),
              QStringLiteral("the unrelated Tecplot file is written"));
        check(!MonitorLogReader::canRead(dir.filePath(QStringLiteral("mesh.tec"))),
              QStringLiteral("a Tecplot file with no monitor column is refused by canRead"));
        MonitorLog unrelatedLog;
        check(!MonitorLogReader::read(dir.filePath(QStringLiteral("mesh.tec")), &unrelatedLog, &message),
              QStringLiteral("and refused by read(), so probe and reader agree"));

        // A declared size that the file does not satisfy is refused rather than
        // padded: filling the gap would draw steps that were never written.
        const QByteArray truncated("VARIABLES = \"Inner_Iter\" \"CD\"\n"
                                   "ZONE I=4\n0 0.1\n1 0.2\n");
        check(writeTempFile(dir, QStringLiteral("truncated.tec"), truncated),
              QStringLiteral("the truncated Tecplot file is written"));
        MonitorLog truncatedLog;
        check(!MonitorLogReader::read(dir.filePath(QStringLiteral("truncated.tec")), &truncatedLog, &message),
              QStringLiteral("a zone shorter than its declared I is refused, not padded"));
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    testRealHistoryFixture();
    testHostileInputs();
    testCanRead();
    testRegisteredColumnRules();
    testProbeAndReaderAgree();
    testSeriesModel();
    testCurveFit();
    testLiveAccumulator();
    testFileTail();
    testLiveRefusesNonText();
    testLiveRegisteredPrimaryAxis();
    testProgressionIsIncremental();
    testLiveAndOfflineAgreeOnColumnKeys();
    testLiveAndOfflineAgreeOnRowAdmission();
    testDuplicateAndNamelessHeaderFieldsKeepTheirColumns();
    testTrailingSeparatorIsNotRagged();
    testFractionalIterationStepAgrees();
    testFitReportsTheAppliedParameters();
    testTecplotReader();
    if (failures != 0) {
        QTextStream(stderr) << failures << " monitor check(s) failed\n";
        return 1;
    }
    QTextStream(stdout) << "Monitor log reader and series model OK\n";
    return 0;
}