#pragma once

#include <QWidget>

class QButtonGroup;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace aerosim {

// Entry page emits file-operation requests; the application binds the existing
// document controller and keeps XML/file operations outside this widget.
class StartupLandingPage final : public QWidget {
    Q_OBJECT
public:
    explicit StartupLandingPage(QWidget *parent = nullptr);
    void setReturnAvailable(bool available);
    void setWorkspaceStatus(const QString &path, bool modified);

signals:
    void enterWorkbenchRequested(const QString &source);
    void returnToWorkbenchRequested();
    void openWorkspaceRequested();
    void saveWorkspaceRequested();
    void saveWorkspaceAsRequested();

private:
    QWidget *createStartPage();
    QWidget *createNewPage();
    QWidget *createFilePage(const QString &name, const QString &title,
                            const QString &description, const QString &buttonName,
                            const QString &buttonText, const QString &icon);
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
