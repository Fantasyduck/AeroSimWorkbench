#pragma once

#include <QWidget>

namespace aerosim {

// Unified front-end-only trajectory settings workspace for the preprocess
// module. It consolidates the former trajectory file / data / state extraction
// leaf pages into one page; every section stays a clearly marked Mock
// placeholder and no file parsing, state extraction or solver work happens
// here. The page owns no VTK objects and never borrows the shared viewport.
class TrajectorySettingsPage final : public QWidget {
    Q_OBJECT
public:
    explicit TrajectorySettingsPage(QWidget *parent = nullptr);

private:
    QWidget *createSection(const QString &objectName, const QString &title,
                           const QString &description, const QString &actionObjectName,
                           const QString &actionText);
};

} // namespace aerosim
