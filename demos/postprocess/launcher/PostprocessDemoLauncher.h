#pragma once

#include <QObject>
#include <QProcess>

namespace aerosim {

// Transitional adapter for the Python/PySide2/VTK prototype.
// It runs in a separate process so the Qt installations remain isolated.
class PostprocessDemoLauncher final : public QObject {
    Q_OBJECT
public:
    explicit PostprocessDemoLauncher(QObject *parent = nullptr);
    ~PostprocessDemoLauncher() override;
    bool launch(bool smoke = false);
    bool isRunning() const;

signals:
    void statusMessage(const QString &message);
    void demoFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString findProjectRoot() const;
    QProcess process_;
};

} // namespace aerosim
