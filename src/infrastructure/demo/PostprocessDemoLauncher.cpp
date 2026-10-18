#include "PostprocessDemoLauncher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace aerosim {

PostprocessDemoLauncher::PostprocessDemoLauncher(QObject *parent) : QObject(parent)
{
    connect(&process_, &QProcess::started, this, [this] {
        emit statusMessage(QStringLiteral("Post-processing VTK demo process started"));
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        emit statusMessage(QStringLiteral("Post-processing demo failed to start: ") + process_.errorString());
        if (error == QProcess::FailedToStart)
            emit demoFinished(-1, QProcess::CrashExit);
    });
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        emit statusMessage(QStringLiteral("Post-processing demo exited with code %1").arg(code));
        emit demoFinished(code, status);
    });
    connect(&process_, &QProcess::readyReadStandardOutput, this, [this] {
        const QString output = QString::fromLocal8Bit(process_.readAllStandardOutput()).trimmed();
        if (!output.isEmpty())
            emit statusMessage(output);
    });
}

PostprocessDemoLauncher::~PostprocessDemoLauncher()
{
    if (!isRunning())
        return;
    process_.terminate();
    if (!process_.waitForFinished(1500)) {
        process_.kill();
        process_.waitForFinished(1500);
    }
}

QString PostprocessDemoLauncher::findProjectRoot() const
{
    const QString overrideRoot = qEnvironmentVariable("AEROSIM_PROJECT_ROOT");
    if (!overrideRoot.isEmpty() && QFileInfo::exists(QDir(overrideRoot).filePath(QStringLiteral("postprocess_demo/main.py"))))
        return QDir(overrideRoot).absolutePath();

    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 5; ++i) {
        if (QFileInfo::exists(dir.filePath(QStringLiteral("postprocess_demo/main.py"))))
            return dir.absolutePath();
        if (!dir.cdUp())
            break;
    }
    return {};
}

bool PostprocessDemoLauncher::isRunning() const
{
    return process_.state() != QProcess::NotRunning;
}

bool PostprocessDemoLauncher::launch(bool smoke)
{
    if (isRunning()) {
        emit statusMessage(QStringLiteral("Post-processing demo is already running"));
        return false;
    }

    const QString root = findProjectRoot();
    if (root.isEmpty()) {
        emit statusMessage(QStringLiteral("Post-processing demo source not found; set AEROSIM_PROJECT_ROOT"));
        return false;
    }
    const QDir project(root);
#ifdef Q_OS_WIN
    const QString python = project.filePath(smoke
        ? QStringLiteral(".venv/Scripts/python.exe")
        : QStringLiteral(".venv/Scripts/pythonw.exe"));
#else
    const QString python = project.filePath(QStringLiteral(".venv/bin/python"));
#endif
    const QString script = project.filePath(smoke
        ? QStringLiteral("postprocess_demo/smoke.py")
        : QStringLiteral("postprocess_demo/main.py"));
    if (!QFileInfo::exists(python)) {
        emit statusMessage(QStringLiteral("Post-processing Python environment not found: ") + python);
        return false;
    }
    if (!QFileInfo::exists(script)) {
        emit statusMessage(QStringLiteral("Post-processing demo script not found: ") + script);
        return false;
    }

    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("QT_PLUGIN_PATH"));
    environment.remove(QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH"));
    process_.setProcessEnvironment(environment);
    process_.setWorkingDirectory(project.filePath(QStringLiteral("postprocess_demo")));
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.start(python, QStringList{script});
    return true; // Process startup and exit errors arrive through signals.
}

} // namespace aerosim
