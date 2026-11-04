#include "aerosim/ui/StartupLandingPage.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResource>
#include <cstddef>

static QWidget *initializeLandingResources(QWidget *parent)
{
    Q_INIT_RESOURCE(RibbonIcons);
    return parent;
}

namespace aerosim {
namespace {
template <std::size_t N>
QString uiText(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

QLabel *pageTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSize(font.pointSize() + 8);
    font.setBold(true);
    label->setFont(font);
    label->setObjectName(QStringLiteral("landingPageTitle"));
    return label;
}
}

StartupLandingPage::StartupLandingPage(QWidget *parent) : QWidget(initializeLandingResources(parent))
{
    setObjectName(QStringLiteral("startupLandingPage"));
    setWindowTitle(uiText(u"AeroSimWorkbench \u00b7 \u5de5\u4f5c\u533a\u5165\u53e3"));
    resize(1180, 760);
    setMinimumSize(900, 620);
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *navigation = new QFrame(this);
    navigation->setObjectName(QStringLiteral("landingNavigation"));
    navigation->setFixedWidth(238);
    auto *navigationLayout = new QVBoxLayout(navigation);
    navigationLayout->setContentsMargins(18, 22, 18, 22);
    navigationLayout->setSpacing(8);
    returnButton_ = new QPushButton(uiText(u"\u2190  \u8fd4\u56de"), navigation);
    returnButton_->setObjectName(QStringLiteral("landingReturnButton"));
    returnButton_->setMinimumHeight(54);
    returnButton_->setEnabled(false);
    returnButton_->setToolTip(uiText(u"\u8fdb\u5165\u5de5\u4f5c\u53f0\u540e\u53ef\u4ece\u6b64\u8fd4\u56de"));
    connect(returnButton_, &QPushButton::clicked, this, &StartupLandingPage::returnToWorkbenchRequested);
    navigationLayout->addWidget(returnButton_);
    auto *brand = new QLabel(uiText(u"AeroSimWorkbench\n\u822a\u7a7a\u822a\u5929\u4eff\u771f\u5de5\u4f5c\u53f0"), navigation);
    brand->setObjectName(QStringLiteral("landingBrand"));
    brand->setWordWrap(true);
    navigationLayout->addWidget(brand);
    navigationLayout->addSpacing(16);

    navigationGroup_ = new QButtonGroup(this);
    navigationGroup_->setExclusive(true);
    navigationLayout->addWidget(createNavButton(QStringLiteral("landingStartNav"), uiText(u"\u5f00\u59cb"), QStringLiteral("workbench"), 0));
    navigationLayout->addWidget(createNavButton(QStringLiteral("landingNewNav"), uiText(u"\u65b0\u5efa"), QStringLiteral("new-project"), 1));
    navigationLayout->addWidget(createNavButton(QStringLiteral("landingOpenNav"), uiText(u"\u6253\u5f00"), QStringLiteral("open-project"), 2));
    navigationLayout->addWidget(createNavButton(QStringLiteral("landingSaveNav"), uiText(u"\u4fdd\u5b58\u5de5\u4f5c\u533a"), QStringLiteral("save-project"), 3));
    navigationLayout->addWidget(createNavButton(QStringLiteral("landingSaveTemplateNav"), uiText(u"\u4fdd\u5b58\u6a21\u677f"), QStringLiteral("template"), 4));
    navigationLayout->addWidget(createNavButton(QStringLiteral("landingSaveAsNav"), uiText(u"\u53e6\u5b58\u4e3a"), QStringLiteral("export-config"), 5));
    navigationLayout->addWidget(createNavButton(QStringLiteral("landingCloseNav"), uiText(u"\u5173\u95ed\u5de5\u4f5c\u533a"), QStringLiteral("stop"), 6));
    navigationLayout->addStretch();
    auto *version = new QLabel(uiText(u"UI \u9884\u89c8 \u00b7 Mock \u5165\u53e3"), navigation);
    version->setObjectName(QStringLiteral("landingVersion"));
    navigationLayout->addWidget(version);
    rootLayout->addWidget(navigation);

    contentStack_ = new QStackedWidget(this);
    contentStack_->setObjectName(QStringLiteral("landingContentStack"));
    contentStack_->addWidget(createStartPage());
    contentStack_->addWidget(createNewPage());
    contentStack_->addWidget(createPlaceholderPage(QStringLiteral("landingOpenPage"), uiText(u"\u6253\u5f00"),
        uiText(u"\u9009\u62e9\u5df2\u6709\u5de5\u4f5c\u533a\u6587\u4ef6\uff0c\u6216\u4ece\u6700\u8fd1\u8def\u5f84\u4e2d\u7ee7\u7eed\u3002\u672c\u671f\u4ec5\u63d0\u4f9b Mock \u5165\u53e3\u3002"), true));
    contentStack_->addWidget(createPlaceholderPage(QStringLiteral("landingSavePage"), uiText(u"\u4fdd\u5b58\u5de5\u4f5c\u533a"),
        uiText(u"\u5f53\u524d\u672a\u6253\u5f00\u5de5\u4f5c\u533a\u3002\u8fdb\u5165\u5de5\u4f5c\u533a\u540e\u53ef\u6267\u884c\u4fdd\u5b58\u64cd\u4f5c\u3002")));
    contentStack_->addWidget(createPlaceholderPage(QStringLiteral("landingSaveTemplatePage"), uiText(u"\u4fdd\u5b58\u6a21\u677f"),
        uiText(u"\u5c06\u5f53\u524d\u5de5\u4f5c\u533a\u7684\u53c2\u6570\u4e0e\u914d\u7f6e\u4fdd\u5b58\u4e3a\u6a21\u677f\u3002\u6a21\u677f\u6301\u4e45\u5316\u5c1a\u672a\u63a5\u5165\u3002")));
    contentStack_->addWidget(createPlaceholderPage(QStringLiteral("landingSaveAsPage"), uiText(u"\u53e6\u5b58\u4e3a"),
        uiText(u"\u4e3a\u5f53\u524d\u5de5\u4f5c\u533a\u9009\u62e9\u65b0\u4f4d\u7f6e\u6216\u65b0\u540d\u79f0\u3002\u672c\u671f\u4e0d\u6267\u884c\u6587\u4ef6\u5199\u5165\u3002")));
    contentStack_->addWidget(createPlaceholderPage(QStringLiteral("landingClosePage"), uiText(u"\u5173\u95ed\u5de5\u4f5c\u533a"),
        uiText(u"\u5f53\u524d\u672a\u6253\u5f00\u5de5\u4f5c\u533a\u3002\u771f\u5b9e\u5173\u95ed\u4e0e\u672a\u4fdd\u5b58\u68c0\u67e5\u5c06\u7531\u5de5\u4f5c\u533a\u670d\u52a1\u63d0\u4f9b\u3002")));
    rootLayout->addWidget(contentStack_, 1);
    navigationGroup_->button(0)->setChecked(true);
    contentStack_->setCurrentIndex(0);

    setStyleSheet(QStringLiteral(
        "QWidget#startupLandingPage { background: #eef2f7; color: #25364c; }"
        "QFrame#landingNavigation { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #173f77, stop:0.55 #215b9e, stop:1 #12335f); }"
        "QLabel#landingBrand { color: white; font-size: 17px; font-weight: 600; padding: 8px 4px; }"
        "QLabel#landingVersion { color: #cbdaf0; padding: 4px; }"
        "QPushButton#landingReturnButton { color: white; text-align: left; font-size: 16px; background: transparent; border: 1px solid #6f91ba; padding: 10px; }"
        "QPushButton#landingReturnButton:hover:enabled { background: #356fae; }"
        "QPushButton#landingReturnButton:disabled { color: #8fa8c8; border-color: #54749b; }"
        "QPushButton[landingNav='true'] { color: white; text-align: left; font-size: 15px; min-height: 42px; background: transparent; border: 0; padding: 7px 12px; }"
        "QPushButton[landingNav='true']:hover { background: #356fae; }"
        "QPushButton[landingNav='true']:checked { color: #173f77; background: #f3f6fa; border-left: 4px solid #65a8e8; padding-left: 8px; }"
        "QStackedWidget#landingContentStack { background: #f3f6fa; }"
        "QWidget[landingPage='true'] { background: #f3f6fa; }"
        "QLabel#landingPageTitle { color: #1c3555; }"
        "QLabel#landingSectionTitle { color: #294a70; font-size: 18px; font-weight: 600; }"
        "QLineEdit#landingSearch { background: white; border: 1px solid #cbd5e2; border-radius: 3px; padding: 9px 12px; font-size: 14px; }"
        "QTableWidget#recentWorkspaceTable { background: white; border: 1px solid #d4dde8; alternate-background-color: #f7f9fc; }"
        "QTableWidget#recentWorkspaceTable::item { padding: 8px; }"
        "QHeaderView::section { background: #e6ecf4; color: #39546f; border: 0; border-bottom: 1px solid #ccd6e2; padding: 8px; }"
        "QPushButton[landingCard='true'] { background: white; color: #243b56; border: 1px solid #cad6e4; border-radius: 4px; text-align: left; padding: 18px; font-size: 16px; }"
        "QPushButton[landingCard='true']:hover { background: #eaf3fd; border-color: #4b91d1; }"
        "QPushButton[landingCard='true']:checked { background: #dcecff; border: 2px solid #2f78bb; }"
        "QFrame#landingPlaceholderPanel { background: white; border: 1px solid #d3dce7; border-radius: 4px; }"));
}

void StartupLandingPage::setReturnAvailable(bool available)
{
    returnButton_->setEnabled(available);
    returnButton_->setToolTip(available ? uiText(u"\u8fd4\u56de\u5f53\u524d\u4e3b\u5de5\u4f5c\u53f0")
                                    : uiText(u"\u5f53\u524d\u5c1a\u672a\u8fdb\u5165\u4e3b\u5de5\u4f5c\u53f0"));
}

QPushButton *StartupLandingPage::createNavButton(const QString &objectName, const QString &text,
                                                  const QString &iconName, int pageIndex)
{
    auto *button = new QPushButton(QIcon(QStringLiteral(":/aerosim/ribbon/") + iconName + QStringLiteral(".svg")), text, this);
    button->setObjectName(objectName);
    button->setProperty("landingNav", true);
    button->setCheckable(true);
    button->setIconSize(QSize(22, 22));
    navigationGroup_->addButton(button, pageIndex);
    connect(button, &QPushButton::clicked, this, [this, pageIndex] { contentStack_->setCurrentIndex(pageIndex); });
    return button;
}

QWidget *StartupLandingPage::createStartPage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("landingStartPage"));
    page->setProperty("landingPage", true);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(54, 42, 54, 42);
    layout->setSpacing(18);
    recentSearch_ = new QLineEdit(page);
    recentSearch_->setObjectName(QStringLiteral("landingSearch"));
    recentSearch_->setPlaceholderText(uiText(u"\u641c\u7d22"));
    recentSearch_->setClearButtonEnabled(true);
    recentSearch_->setMaximumWidth(520);
    layout->addWidget(recentSearch_);
    layout->addWidget(pageTitle(uiText(u"\u6700\u8fd1"), page));
    auto *hint = new QLabel(uiText(u"\u6700\u8fd1\u5de5\u4f5c\u533a\uff08Mock \u6570\u636e\uff09\u00b7 \u53cc\u51fb\u53ef\u8fdb\u5165\u4e3b\u5de5\u4f5c\u53f0"), page);
    hint->setObjectName(QStringLiteral("landingRecentHint"));
    layout->addWidget(hint);
    recentTable_ = new QTableWidget(1, 2, page);
    recentTable_->setObjectName(QStringLiteral("recentWorkspaceTable"));
    recentTable_->setHorizontalHeaderLabels({uiText(u"\u540d\u79f0"), uiText(u"\u4fee\u6539\u65e5\u671f")});
    recentTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    recentTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    recentTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recentTable_->setAlternatingRowColors(true);
    recentTable_->verticalHeader()->hide();
    recentTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    recentTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    auto *name = new QTableWidgetItem(uiText(u"DemoWorkspace\uff08Mock\uff09\nF:/AeroSim/DemoWorkspace"));
    name->setData(Qt::UserRole, QStringLiteral("recent-mock"));
    recentTable_->setItem(0, 0, name);
    recentTable_->setItem(0, 1, new QTableWidgetItem(QStringLiteral("2026-11-03 18:11")));
    recentTable_->setRowHeight(0, 58);
    connect(recentTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        emit enterWorkbenchRequested(recentTable_->item(row, 0)->data(Qt::UserRole).toString());
    });
    connect(recentSearch_, &QLineEdit::textChanged, this, [this](const QString &term) {
        for (int row = 0; row < recentTable_->rowCount(); ++row)
            recentTable_->setRowHidden(row, !recentTable_->item(row, 0)->text().contains(term, Qt::CaseInsensitive));
    });
    layout->addWidget(recentTable_, 1);
    return page;
}

QPushButton *StartupLandingPage::createCardButton(const QString &objectName, const QString &title,
                                                   const QString &subtitle, const QString &iconName)
{
    auto *button = new QPushButton(QIcon(QStringLiteral(":/aerosim/ribbon/") + iconName + QStringLiteral(".svg")),
                                   title + QLatin1Char('\n') + subtitle, this);
    button->setObjectName(objectName);
    button->setProperty("landingCard", true);
    button->setIconSize(QSize(44, 44));
    button->setMinimumSize(220, 105);
    return button;
}

QWidget *StartupLandingPage::createNewPage()
{
    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("landingNewPage"));
    page->setProperty("landingPage", true);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(54, 42, 54, 42);
    layout->setSpacing(18);
    layout->addWidget(pageTitle(uiText(u"\u65b0\u5efa"), page));
    auto *entryLayout = new QHBoxLayout;
    auto *createWorkspace = createCardButton(QStringLiteral("createWorkspaceCard"), uiText(u"\u65b0\u5efa\u5de5\u4f5c\u533a"),
                                              uiText(u"\u521b\u5efa\u7a7a\u767d\u5de5\u4f5c\u533a\uff08Mock\uff09"), QStringLiteral("new-project"));
    auto *importTemplate = createCardButton(QStringLiteral("importTemplateCard"), uiText(u"\u5bfc\u5165\u6a21\u677f"),
                                             uiText(u"\u4ece\u73b0\u6709\u6a21\u677f\u521b\u5efa\uff08\u5360\u4f4d\uff09"), QStringLiteral("import-config"));
    connect(createWorkspace, &QPushButton::clicked, this, [this] { emit enterWorkbenchRequested(QStringLiteral("new-workspace-mock")); });
    connect(importTemplate, &QPushButton::clicked, this, [importTemplate] {
        importTemplate->setToolTip(uiText(u"\u6a21\u677f\u6587\u4ef6\u9009\u62e9\u5c1a\u672a\u63a5\u5165"));
    });
    entryLayout->addWidget(createWorkspace);
    entryLayout->addWidget(importTemplate);
    entryLayout->addStretch();
    layout->addLayout(entryLayout);
    auto *templateTitle = new QLabel(uiText(u"\u5185\u7f6e\u6a21\u677f"), page);
    templateTitle->setObjectName(QStringLiteral("landingSectionTitle"));
    layout->addWidget(templateTitle);
    auto *templates = new QGridLayout;
    const struct TemplateItem { const char *id; const char16_t *title; const char *icon; } items[] = {
        {"dynamicTemplateCard", u"\u52a8\u6001\u7279\u6027", "curve"},
        {"separationTemplateCard", u"\u591a\u4f53\u5206\u79bb", "transform"},
        {"aerodynamicTemplateCard", u"\u6c14\u52a8\u529b", "physics"},
        {"thermalTemplateCard", u"\u6c14\u52a8\u70ed", "contour"}};
    for (int index = 0; index < 4; ++index) {
        auto *card = createCardButton(QString::fromLatin1(items[index].id),
            QString::fromUtf16(reinterpret_cast<const ushort *>(items[index].title)),
            uiText(u"\u5185\u7f6e\u6a21\u677f\uff08Mock\uff09"), QString::fromLatin1(items[index].icon));
        card->setCheckable(true);
        templates->addWidget(card, index / 2, index % 2);
    }
    templates->setColumnStretch(2, 1);
    layout->addLayout(templates);
    layout->addStretch();
    return page;
}

QWidget *StartupLandingPage::createPlaceholderPage(const QString &objectName, const QString &title,
                                                    const QString &description, bool canEnterWorkbench)
{
    auto *page = new QWidget(this);
    page->setObjectName(objectName);
    page->setProperty("landingPage", true);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(54, 42, 54, 42);
    layout->setSpacing(20);
    layout->addWidget(pageTitle(title, page));
    auto *panel = new QFrame(page);
    panel->setObjectName(QStringLiteral("landingPlaceholderPanel"));
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(34, 30, 34, 30);
    auto *descriptionLabel = new QLabel(description, panel);
    descriptionLabel->setWordWrap(true);
    panelLayout->addWidget(descriptionLabel);
    if (canEnterWorkbench) {
        auto *open = new QPushButton(QIcon(QStringLiteral(":/aerosim/ribbon/open-project.svg")),
                                     uiText(u"\u9009\u62e9\u5de5\u4f5c\u533a\u6587\u4ef6\uff08Mock\uff09"), panel);
        open->setObjectName(QStringLiteral("openWorkspaceMockButton"));
        open->setProperty("landingCard", true);
        open->setMaximumWidth(360);
        connect(open, &QPushButton::clicked, this, [this] { emit enterWorkbenchRequested(QStringLiteral("open-workspace-mock")); });
        panelLayout->addWidget(open);
    }
    panelLayout->addStretch();
    layout->addWidget(panel, 1);
    return page;
}

} // namespace aerosim
