// Entry point of the standalone monitor chart demo.
//
// Usage:
//   aerosim_monitor_demo [history.csv]
//
// With a path the file is loaded on startup; without one the window opens empty
// and "导入数据文件…" asks for it. --selftest writes PNGs and prints a report
// instead of opening a window, so the same code path can be checked headlessly.
#include "MonitorChartDemoWindow.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

using namespace aerosim::monitor::demo;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTextStream out(stdout);

    QStringList arguments = app.arguments();
    arguments.removeFirst();

    bool selfTest = false;
    QString shotDirectory;
    QString path;
    for (const QString &argument : arguments) {
        if (argument == QLatin1String("--selftest")) {
            selfTest = true;
        } else if (argument.startsWith(QLatin1String("--shots="))) {
            shotDirectory = argument.mid(8);
        } else if (!argument.startsWith(QLatin1String("--"))) {
            path = argument;
        }
    }

    if (selfTest) {
        if (shotDirectory.isEmpty())
            shotDirectory = QDir::current().filePath(QStringLiteral("demo_shots"));
        MonitorChartDemoWindow window;
        window.resize(1280, 900);
        // Shown, but off screen and without waiting for input: the window grab is
        // part of what the self test checks, and a hidden widget has no pixels.
        window.show();
        // The self test has to drive a real drawing: a run that never loaded a
        // file would report a tidy "no curves" and prove nothing.
        if (path.isEmpty()) {
            out << "SELFTEST_FAIL: no history file given\n";
            return 1;
        }
        QString loadError;
        if (!window.loadFile(path, &loadError)) {
            out << "SELFTEST_FAIL: " << loadError << '\n';
            return 1;
        }
        const QString log = window.selfTest(shotDirectory);
        out << log << '\n';
        return log.contains(QLatin1String("SELFTEST_OK")) ? 0 : 1;
    }

    MonitorChartDemoWindow window;
    window.show();
    if (!path.isEmpty()) {
        QString error;
        if (!window.loadFile(path, &error))
            out << "load failed: " << error << '\n';
    }
    return app.exec();
}
