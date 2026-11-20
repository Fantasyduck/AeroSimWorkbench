#include "aerosim/configuration/XcfProjectDocument.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <limits>
using namespace aerosim;
namespace {
void set(ParameterCategoryConfig &c, const QString &key, const ParameterValue &v) {
    for (auto &e : c.entries) if (e.key == key) { e.value = v; return; }
    ParameterEntry e; e.key = key; e.value = v; c.entries.append(e);
}
QByteArray read(const QString &path) { QFile f(path); f.open(QIODevice::ReadOnly); return f.readAll(); }
void write(const QString &path, const QByteArray &b) { QFile f(path); f.open(QIODevice::WriteOnly); f.write(b); }
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); QTemporaryDir dir; QString error;
    const auto path = dir.filePath("solution.xcf"), bad = dir.filePath("bad.xcf");
    XcfProjectDocument doc;
    if (!doc.save(path, &error)) return 1;
    auto seed = read(path);
    seed.replace("<SolutionMethodData>", "<SolutionMethodData preserved=\"yes\"><!--keep--><TURBCFL_VAL>2</TURBCFL_VAL><TurblstLn2>0.01,1</TurblstLn2><extra>keep</extra>");
    write(path, seed);
    if (!doc.load(path, &error) || doc.isModified()) return 2;
    if (!doc.setSolutionMethod(doc.solutionMethod(), &error) || doc.isModified()) return 3;
    const QStringList spatial{"roe", "vanleer", "ausm", "ausmpw-plus", "hllc"};
    const QStringList limiter{"none", "vanleer", "vanalbada", "hvanleer", "hvanalbada"};
    const QStringList time{"bdf1", "bdf2", "optimized-bdf2", "bdf3", "modified-bdf4", "ssprk3"};
    for (int i = 0; i < 6; ++i) {
        auto c = defaultSolutionMethod();
        set(c, "spatialScheme", ParameterValue::enumeration(spatial[i % 5]));
        set(c, "limiter", ParameterValue::enumeration(limiter[i % 5]));
        set(c, "timeScheme", ParameterValue::enumeration(time[i]));
        set(c, "entropyFix", ParameterValue::boolean(i % 2));
        set(c, "strongStability", ParameterValue::boolean(i % 2));
        set(c, "timeStep", ParameterValue::real(0.0123456789012345, "s"));
        set(c, "subIterations", ParameterValue::integer(333));
        set(c, "cflPointCount", ParameterValue::integer(3));
        set(c, "cflPoints/0/cfl", ParameterValue::real(0.123456789012345));
        set(c, "cflPoints/2/iteration", ParameterValue::integer(4000));
        set(c, "cflPoints/2/cfl", ParameterValue::real(12.5));
        if (!doc.setSolutionMethod(c, &error) || !doc.isModified() || !doc.save(path, &error) || doc.isModified()) return 4;
        const auto bytes = read(path);
        QDomDocument xml; xml.setContent(bytes); const auto e = xml.documentElement().firstChildElement("SolutionMethodData");
        if (e.firstChildElement("SPADIS").text().toInt() != i % 5
            || e.firstChildElement("FLOWLIM").text().toInt() != i % 5
            || e.firstChildElement("TIMEDIS").text().toInt() != i
            || e.firstChildElement("lstLn1").text() != "1,2000,4000"
            || e.firstChildElement("CFL_VAL").text() != "3"
            || !bytes.contains("<!--keep-->") || !bytes.contains("<TURBCFL_VAL>2</TURBCFL_VAL>")
            || !bytes.contains("<TurblstLn2>0.01,1</TurblstLn2>") || !bytes.contains("preserved=\"yes\"")) return 5;
        XcfProjectDocument reload;
        if (!reload.load(path, &error)) return 6;
        for (const auto &entry : c.entries) {
            const auto *actual = reload.solutionMethod().find(entry.key);
            if (!actual || entry.value.kind != actual->value.kind || entry.value.integerValue != actual->value.integerValue
                || entry.value.realValue != actual->value.realValue || entry.value.booleanValue != actual->value.booleanValue
                || entry.value.textValue != actual->value.textValue) return 7;
        }
    }
    const auto valid = read(path);
    const QList<QPair<QByteArray,QByteArray>> invalid{
        {"<SPADIS>0</SPADIS>", "<SPADIS>5</SPADIS>"},
        {"<CFL_VAL>3</CFL_VAL>", "<CFL_VAL>2</CFL_VAL>"},
        {"<lstLn1>1,2000,4000</lstLn1>", "<lstLn1>1,,4000</lstLn1>"},
        {"<linear_projection>1</linear_projection>", "<linear_projection>2</linear_projection>"},
        {"<subiterations>333</subiterations>", "<subiterations>0</subiterations>"},
        {"<TIMEDIS>5</TIMEDIS>", "<TIMEDIS>5</TIMEDIS><TIMEDIS>0</TIMEDIS>"},
        {"<time_step>", "<time_step><nested/>"},
        {"<lstLn2>", "<lstLn2>nan,"}
    };
    for (const auto &change : invalid) {
        auto b = valid; if (!b.contains(change.first)) return 8;
        b.replace(change.first, change.second); write(bad, b);
        if (doc.load(bad, &error) || doc.filePath() != path || doc.isModified()
            || doc.solutionMethod().find("timeScheme")->value.textValue != "ssprk3") return 9;
    }
    auto invalidConfig = doc.solutionMethod();
    set(invalidConfig, "cflPoints/0/cfl", ParameterValue::real(std::numeric_limits<double>::infinity()));
    if (doc.setSolutionMethod(invalidConfig, &error) || doc.isModified()) return 10;
    // Saving a reduced table must not leak the removed rows.
    auto reduced = defaultSolutionMethod();
    for (int i = reduced.entries.size()-1; i >= 0; --i) if (reduced.entries[i].key.startsWith("cflPoints/1/")) reduced.entries.removeAt(i);
    set(reduced, "cflPointCount", ParameterValue::integer(1));
    if (!doc.setSolutionMethod(reduced, &error) || !doc.save(path, &error) || !doc.load(path, &error)
        || doc.solutionMethod().find("cflPoints/1/cfl")) return 11;
    QTextStream(stdout) << "Solution method encodings, CFL rows, preservation and transactional failures passed.\n";
    return 0;
}
