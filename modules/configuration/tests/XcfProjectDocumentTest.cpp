#include "aerosim/configuration/XcfProjectDocument.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace aerosim;
namespace {
bool check(bool ok, const QString &message)
{
    if (!ok) QTextStream(stderr) << message << '\n';
    return ok;
}
void set(ParameterCategoryConfig &config, const QString &key, const ParameterValue &value)
{
    for (auto &entry : config.entries) if (entry.key == key) entry.value = value;
}
QByteArray read(const QString &path)
{
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    return file.readAll();
}
bool write(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    const auto path = dir.filePath(QString::fromUtf8("工程.xcf"));
    QString error;
    XcfProjectDocument document;
    if (!check(document.save(path, &error), error)) return 1;
    auto bytes = read(path);
    bytes.replace("</DATA_ROOT>", "<!--retained--><Unknown attribute=\"keep\"><Value>42</Value></Unknown></DATA_ROOT>");
    bytes.replace("<EquationSettingData>", "<EquationSettingData><PERIODIC_BODY_FORCE>1.25</PERIODIC_BODY_FORCE>");
    bytes.replace("<TurbModelData>", "<TurbModelData><Advanced>unchanged</Advanced>");
    if (!write(path, bytes) || !check(document.load(path, &error), error)) return 2;
    // All three flows, both time schemes, six turbulence models, both BGK
    // submodels, three collision models and the disabled rarefied branch.
    const QStringList flows = {"laminar", "turbulent", "inviscid"};
    const QStringList models = {"sa", "kw-menter", "des", "ddes", "wilcox-kw1998", "wilcox-kw2006"};
    const QStringList collisions = {"vss", "hs", "vhs"};
    int caseIndex = 0;
    for (int flow = 0; flow < 3; ++flow) for (int model = 0; model < 6; ++model) {
        auto config = document.governingEquations();
        set(config, "equation", ParameterValue::enumeration(flow == 2 ? "euler" : "navier-stokes"));
        set(config, "flowRegime", ParameterValue::enumeration(flows[flow]));
        set(config, "calculationType", ParameterValue::enumeration(model % 2 ? "unsteady" : "steady"));
        set(config, "turbulenceModel", ParameterValue::enumeration(models[model]));
        set(config, "rarefiedGas", ParameterValue::boolean(model % 2));
        set(config, "collisionSubmodel", ParameterValue::enumeration(model % 2 ? "es-bgk" : "standard-bgk"));
        set(config, "collisionModel", ParameterValue::enumeration(collisions[model % 3]));
        set(config, "knudsenNumber", ParameterValue::real(0.123456789012345));
        set(config, "freestreamParticleCount", ParameterValue::integer(12345));
        set(config, "gridParticleCount", ParameterValue::integer(57));
        if (!check(document.setGoverningEquations(config, &error), error)
            || !check(document.save(path, &error), error)) return 3;
        QDomDocument xml;
        xml.setContent(read(path));
        const auto root = xml.documentElement();
        const auto equation = root.firstChildElement("EquationSettingData");
        if (!check(equation.firstChildElement("TURB_MODEL").text() == QString::number(flow)
            && root.firstChildElement("TurbModelData").firstChildElement("TURB_MODEL").text() == QString::number(model)
            && equation.firstChildElement("ISSTEADY").text() == QString::number(model % 2)
            && equation.firstChildElement("EQUATION_TYPE").text() == (flow == 2 ? "1" : "0"), "Incorrect XCF enum encoding")) return 4;
        if (!check(read(path).contains("<!--retained-->") && read(path).contains("attribute=\"keep\"")
            && equation.firstChildElement("PERIODIC_BODY_FORCE").text() == "1.25"
            && root.firstChildElement("TurbModelData").firstChildElement("Advanced").text() == "unchanged",
            "Unmapped XML content lost")) return 5;
        XcfProjectDocument loaded;
        if (!check(loaded.load(path, &error), error)) return 6;
        for (const auto &entry : config.entries) {
            const auto actual = loaded.governingEquations().find(entry.key)->value;
            if (!check(actual.kind == entry.value.kind && actual.textValue == entry.value.textValue
                && actual.booleanValue == entry.value.booleanValue && actual.integerValue == entry.value.integerValue
                && actual.realValue == entry.value.realValue, "Typed roundtrip mismatch: " + entry.key)) return 7;
        }
        ++caseIndex;
    }
    const auto source = read(path);
    const auto savedPath = document.filePath();
    auto bad = source;
    bad.replace("<ISSTEADY>1</ISSTEADY>", "<ISSTEADY>9</ISSTEADY>");
    const auto invalidPath = dir.filePath("invalid.xcf");
    write(invalidPath, bad);
    if (!check(!document.load(invalidPath, &error) && !error.isEmpty() && document.filePath() == savedPath,
        "Invalid enum must fail without replacing current document")) return 8;
    write(invalidPath, "<DATA_ROOT><broken>");
    if (!check(!document.load(invalidPath, &error), "Malformed XML accepted")) return 9;
    auto config = document.governingEquations();
    set(config, "gridParticleCount", ParameterValue::integer(0));
    if (!check(!document.setGoverningEquations(config, &error), "Invalid particle count accepted")) return 10;
    if (!check(!document.save(dir.filePath("missing/file.xcf"), &error) && read(path) == source,
        "Failed save changed the source")) return 11;
    bad = source;
    bad.replace("<TURB_MODEL>5</TURB_MODEL>", "<TURB_MODEL>8</TURB_MODEL>");
    write(invalidPath, bad); // inviscid: retain unknown inactive detailed model.
    if (!check(document.load(invalidPath, &error) && !document.warnings().isEmpty(), error)
        || !document.save(invalidPath, &error)
        || !check(read(invalidPath).contains("<TURB_MODEL>8</TURB_MODEL>"), "Inactive unknown model lost")) return 12;
    bad = source;
    bad.replace("<ISSTEADY>1</ISSTEADY>", "<ISSTEADY>1</ISSTEADY><ISSTEADY>0</ISSTEADY>");
    write(invalidPath, bad);
    if (!check(!document.load(invalidPath, &error), "Duplicate mapped tag accepted")) return 13;
    // Confirmed calculation fields update without touching relative residuals,
    // MPI policy, GPU selection encoding, or unrelated regions.
    XcfProjectDocument calculation;
    const auto calcPath = dir.filePath("calculation.xcf");
    if (!calculation.save(calcPath, &error)) return 15;
    auto calcBytes = read(calcPath);
    calcBytes.replace("<dABS_RES>-10</dABS_RES>", "<dABS_RES>-10</dABS_RES><dREL_RES>-7.25</dREL_RES><ISABS_RES>0</ISABS_RES><NITSFG>40000</NITSFG><GPUIndex>4294967295</GPUIndex><STARTNUM>9</STARTNUM>");
    calcBytes.replace("<NITSCG>30000</NITSCG>", "<NITSCG>882</NITSCG>");
    calcBytes.replace("<_nProcess>4</_nProcess>", "<_nProcess>4</_nProcess><_nMPI>3</_nMPI>");
    write(calcPath, calcBytes);
    if (!check(calculation.load(calcPath, &error), error)) return 16;
    for (const auto &entry : calculation.calculationControl().entries)
        if (entry.key == "maximumIterations" && !check(entry.value.integerValue == 882,
            "Maximum iterations must read NITSCG, not NITSFG")) return 24;
    auto control = calculation.calculationControl();
    for (auto &entry : control.entries) {
        if (entry.key == "firstOrderIterations") entry.value = ParameterValue::integer(731);
        if (entry.key == "maximumIterations") entry.value = ParameterValue::integer(45678);
        if (entry.key == "convergenceResidual") entry.value = ParameterValue::real(-12.1234567890123);
        if (entry.key == "residualMonitorThreshold") entry.value = ParameterValue::real(125.987654321012);
        if (entry.key == "useGpu") entry.value = ParameterValue::boolean(true);
        if (entry.key == "processCount") entry.value = ParameterValue::integer(12);
        if (entry.key == "gpuCount") entry.value = ParameterValue::integer(2);
    }
    if (!check(calculation.setCalculationControl(control, &error) && calculation.isModified()
        && calculation.save(calcPath, &error), error)) return 17;
    XcfProjectDocument calcReload;
    if (!check(calcReload.load(calcPath, &error), error)) return 18;
    for (int i = 0; i < control.entries.size(); ++i) {
        const auto &expected = control.entries[i].value;
        const auto &actual = calcReload.calculationControl().entries[i].value;
        if (!check(expected.integerValue == actual.integerValue && expected.realValue == actual.realValue
            && expected.booleanValue == actual.booleanValue, "Calculation control roundtrip failed")) return 19;
    }
    const auto retained = read(calcPath);
    for (const auto &fragment : {"<dREL_RES>-7.25</dREL_RES>", "<ISABS_RES>0</ISABS_RES>",
         "<NITSFG>40000</NITSFG>", "<NITSCG>45678</NITSCG>", "<GPUIndex>4294967295</GPUIndex>", "<STARTNUM>9</STARTNUM>", "<_nMPI>3</_nMPI>"})
        if (!check(retained.contains(fragment), QString("Unconfirmed field changed: ") + fragment)) return 20;
    auto invalid = retained;
    invalid.replace("<ISGPU>1</ISGPU>", "<ISGPU>7</ISGPU>");
    write(invalidPath, invalid);
    if (!check(!calcReload.load(invalidPath, &error) && calcReload.filePath() == calcPath,
        "Invalid GPU switch replaced current document")) return 21;
    invalid = retained;
    invalid.replace("<GPUNUM>2</GPUNUM>", "<GPUNUM>0</GPUNUM>");
    write(invalidPath, invalid);
    if (!check(!calcReload.load(invalidPath, &error), "Invalid GPU count accepted")) return 22;
    invalid = retained;
    invalid.replace("<FOIS>731</FOIS>", "<FOIS>731</FOIS><FOIS>22</FOIS>");
    write(invalidPath, invalid);
    if (!check(!calcReload.load(invalidPath, &error), "Duplicate calculation field accepted")) return 23;
    XcfProjectDocument outputDoc;
    const auto outputPath = dir.filePath("output-control.xcf");
    if (!outputDoc.save(outputPath, &error)) return 25;
    auto outputBytes = read(outputPath);
    outputBytes.replace("<OutputSettingData>", "<OutputSettingData><RESTART_OUT>5000.0</RESTART_OUT><PLOT_FUNCTIONS>1;0;1</PLOT_FUNCTIONS><ISWALL>0</ISWALL>");
    outputBytes.replace("<RefenceParametersData>", "<RefenceParametersData><RESULT_INTERVAL>321</RESULT_INTERVAL>");
    write(outputPath, outputBytes);
    if (!outputDoc.load(outputPath, &error)) return 26;
    // Every variable must occupy exactly its confirmed position, independently
    // for flow and wall; also exercise empty and non-empty boundary lists.
    for (int bit = -1; bit < 17; ++bit) {
        auto outputConfig = defaultOutputControl();
        const auto flowKeys = outputFlowVariableKeys(), wallKeys = outputWallVariableKeys();
        for (auto &entry : outputConfig.entries) {
            if (entry.key == "restartInterval") entry.value = ParameterValue::integer(111);
            if (entry.key == "wallInterval") entry.value = ParameterValue::integer(-11);
            if (entry.key == "flowInterval") entry.value = ParameterValue::integer(-12);
            if (entry.key.startsWith("flowVariables/")) entry.value = ParameterValue::boolean(bit >= 0 && entry.key == "flowVariables/" + flowKeys[bit]);
            if (entry.key.startsWith("wallVariables/")) entry.value = ParameterValue::boolean(bit >= 0 && entry.key == "wallVariables/" + wallKeys[16 - bit]);
        }
        for (const auto &name : {QString("custom_wall"), QString("custom_farfield")}) {
            ParameterEntry entry; entry.key = "boundaryHistory/" + name;
            entry.value = ParameterValue::boolean(bit >= 0); outputConfig.entries.append(entry);
        }
        if (!check(outputDoc.setOutputControl(outputConfig, &error) && outputDoc.save(outputPath, &error), error)) return 27;
        QDomDocument xml; xml.setContent(read(outputPath));
        const auto root = xml.documentElement();
        const auto outputElement = root.firstChildElement("OutputSettingData");
        const auto flowFlags = outputElement.firstChildElement("VOLUME").text().split(';');
        const auto wallFlags = outputElement.firstChildElement("BOUNDARY").text().split(';');
        if (flowFlags.size() != 17 || wallFlags.size() != 17) return 28;
        for (int i = 0; i < 17; ++i)
            if (flowFlags[i] != (i == bit ? "1" : "0") || wallFlags[i] != (bit >= 0 && i == 16 - bit ? "1" : "0")) return 29;
        if (outputElement.firstChildElement("OUTPUTBC").text() != (bit < 0 ? "" : "custom_wall;custom_farfield")) return 30;
        if (!check(read(outputPath).contains("<RESTART_OUT>5000.0</RESTART_OUT>")
            && read(outputPath).contains("<PLOT_FUNCTIONS>1;0;1</PLOT_FUNCTIONS>")
            && read(outputPath).contains("<ISWALL>0</ISWALL>")
            && read(outputPath).contains("<RESULT_INTERVAL>321</RESULT_INTERVAL>"), "Unmapped output values changed")) return 31;
        XcfProjectDocument reloaded;
        if (!check(reloaded.load(outputPath, &error), error)) return 32;
        for (const auto &entry : outputConfig.entries) {
            const auto *actual = reloaded.outputControl().find(entry.key);
            // Unselected boundary choices without BC definitions are UI-only.
            if (!actual && entry.key.startsWith("boundaryHistory/") && !entry.value.booleanValue) continue;
            if (!actual || entry.value.integerValue != actual->value.integerValue
                || entry.value.booleanValue != actual->value.booleanValue) return 33;
        }
    }
    auto invalidOutput = read(outputPath);
    invalidOutput.replace("<FLOW_INTERVAL>-12</FLOW_INTERVAL>", "<FLOW_INTERVAL>bad</FLOW_INTERVAL>");
    write(invalidPath, invalidOutput);
    if (outputDoc.load(invalidPath, &error) || outputDoc.filePath() != outputPath) return 34;
    invalidOutput = read(outputPath);
    const auto start = invalidOutput.indexOf("<VOLUME>");
    const auto end = invalidOutput.indexOf("</VOLUME>", start);
    invalidOutput.replace(start, end - start + 9, "<VOLUME>1;0;2</VOLUME>");
    write(invalidPath, invalidOutput);
    if (outputDoc.load(invalidPath, &error)) return 35;
    if (argc > 1) {
        XcfProjectDocument reference;
        if (!check(reference.load(QString::fromLocal8Bit(argv[1]), &error), error)
            || !check(reference.save(dir.filePath("reference-copy.xcf"), &error), error)) return 14;
        QTextStream(stdout) << "Reference XCF loaded and saved to a temporary copy.\n";
        for (const auto &warning : reference.warnings()) QTextStream(stdout) << warning << '\n';
    }
    QTextStream(stdout) << "Passed " << caseIndex << " enum/numeric roundtrips, preservation and failure checks.\n";
    return 0;
}
