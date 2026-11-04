#pragma once

#include <QWidget>

class QButtonGroup;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace aerosim {

// Front-end only entry page. Workspace I/O remains the responsibility of the
// project service and is deliberately not implemented here.
class StartupLandingPage final : public QWidget {
    Q_OBJECT
public:
    explicit StartupLandingPage(QWidget *parent = nullptr);
    void setReturnAvailable(bool available);

signals:
    void enterWorkbenchRequested(const QString &source);
    void returnToWorkbenchRequested();

private:
    QWidget *createStartPage();
    QWidget *createNewPage();
    QWidget *createPlaceholderPage(const QString &objectName, const QString &title,
                                   const QString &description, bool canEnterWorkbench = false);
    QPushButton *createNavButton(const QString &objectName, const QString &text,
                                 const QString &iconName, int pageIndex);
    QPushButton *createCardButton(const QString &objectName, const QString &title,
                                  const QString &subtitle, const QString &iconName);

    QButtonGroup *navigationGroup_ = nullptr;
    QStackedWidget *contentStack_ = nullptr;
    QLineEdit *recentSearch_ = nullptr;
    QTableWidget *recentTable_ = nullptr;
    QPushButton *returnButton_ = nullptr;
};

} // namespace aerosim
