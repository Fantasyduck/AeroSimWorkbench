#include "aerosim/ui/TrajectorySettingsPage.h"

#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVariant>
#include <cstddef>

namespace aerosim {
namespace {
// Match the main window: explicit UTF-16 literals keep Chinese UI text
// independent from the source/execution code page on Qt 5.12 / MSVC 2017.
template <std::size_t N>
QString uiText(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}
}

TrajectorySettingsPage::TrajectorySettingsPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("trajectorySettingsPage"));
    setStyleSheet(QStringLiteral(
        "#trajectorySettingsHeader { background:#edf3fa; border-bottom:1px solid #c8d5e4; }"
        "#trajectorySettingsTitle { color:#1f3b5b; font-size:15px; font-weight:600; }"
        "#trajectorySettingsScope, #trajectorySettingsHint { color:#637083; }"
        "#trajectorySettingsSubtitle { color:#1f3b5b; font-weight:600; }"
        "QGroupBox { font-weight:600; margin-top:8px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 3px; }"));

    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(8, 8, 8, 8);
    pageLayout->setSpacing(6);

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("trajectorySettingsHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 7, 12, 7);
    auto *title = new QLabel(uiText(u"\u5f39\u9053\u8bbe\u7f6e"), header);
    title->setObjectName(QStringLiteral("trajectorySettingsTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    auto *scope = new QLabel(
        uiText(u"\u524d\u7aef Mock \u4ea4\u4e92 \u00b7 \u672a\u8fde\u63a5\u5f39\u9053\u89e3\u6790\u4e0e\u72b6\u6001\u63d0\u53d6\u670d\u52a1"),
        header);
    scope->setObjectName(QStringLiteral("trajectorySettingsScope"));
    headerLayout->addWidget(scope);
    pageLayout->addWidget(header);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("trajectorySettingsScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("trajectorySettingsContent"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(4, 4, 4, 4);
    contentLayout->setSpacing(14);

    auto *hint = new QLabel(
        uiText(u"\u7edf\u4e00\u5f39\u9053\u8bbe\u7f6e\u9875\u9762\u524d\u7aef Mock \u4ea4\u4e92\uff1b\u4e0d\u8bfb\u53d6\u5f39\u9053\u6587\u4ef6\u3001\u4e0d\u6267\u884c\u72b6\u6001\u63d0\u53d6\u6216\u6c42\u89e3\u8ba1\u7b97\u3002"),
        content);
    hint->setObjectName(QStringLiteral("trajectorySettingsHint"));
    hint->setWordWrap(true);
    contentLayout->addWidget(hint);

    // The three former leaf pages stay reachable as in-page sections instead of
    // separate navigation pages. Only their descriptions are preserved; no new
    // trajectory capability is introduced here.
    struct Section {
        const char *objectName;
        const char16_t *title;
        const char16_t *description;
        const char16_t *action;
        const char *actionObjectName;
    };
    const Section sections[] = {
        {"trajectoryFileSection", u"\u5f39\u9053\u6587\u4ef6",
         u"\u5f39\u9053\u6587\u4ef6\u9009\u62e9\u4e0e\u7ba1\u7406\u5165\u53e3\u3002\u672c\u671f\u4e0d\u5b9e\u73b0\u6587\u4ef6\u89e3\u6790\u3002",
         u"\u9009\u62e9\u5f39\u9053\u6587\u4ef6", "selectTrajectoryFileButton"},
        {"trajectoryDataSection", u"\u5f39\u9053\u6570\u636e",
         u"\u5f39\u9053\u6570\u636e\u67e5\u770b\u5165\u53e3\u3002\u5f53\u524d\u5de5\u4f5c\u533a\u6ca1\u6709\u53ef\u5c55\u793a\u7684\u771f\u5b9e\u6570\u636e\u3002",
         u"\u8f7d\u5165\u5f39\u9053\u6570\u636e", "loadTrajectoryDataButton"},
        {"stateExtractionSection", u"\u72b6\u6001\u63d0\u53d6",
         u"\u4ece\u5f39\u9053\u6570\u636e\u63d0\u53d6\u8ba1\u7b97\u72b6\u6001\u7684\u914d\u7f6e\u5165\u53e3\u3002\u672c\u671f\u4e0d\u6267\u884c\u63d0\u53d6\u8ba1\u7b97\u3002",
         u"\u63d0\u53d6\u8ba1\u7b97\u72b6\u6001", "extractTrajectoryStateButton"},
    };
    for (const auto &section : sections) {
        contentLayout->addWidget(createSection(
            QString::fromLatin1(section.objectName),
            QString::fromUtf16(reinterpret_cast<const ushort *>(section.title)),
            QString::fromUtf16(reinterpret_cast<const ushort *>(section.description)),
            QString::fromLatin1(section.actionObjectName),
            QString::fromUtf16(reinterpret_cast<const ushort *>(section.action))));
    }
    contentLayout->addStretch();
    scroll->setWidget(content);
    pageLayout->addWidget(scroll, 1);
}

QWidget *TrajectorySettingsPage::createSection(const QString &objectName, const QString &title,
                                               const QString &description,
                                               const QString &actionObjectName,
                                               const QString &actionText)
{
    auto *group = new QGroupBox(title, this);
    group->setObjectName(objectName);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto *subtitle = new QLabel(title, group);
    subtitle->setObjectName(QStringLiteral("trajectorySettingsSubtitle"));
    layout->addWidget(subtitle);

    auto *descriptionLabel = new QLabel(description, group);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    auto *status = new QLabel(
        uiText(u"\u72b6\u6001\uff1a\u7b49\u5f85\u4e1a\u52a1\u670d\u52a1\u63a5\u5165\uff08Mock\uff0c\u4e0d\u6267\u884c\u6587\u4ef6\u89e3\u6790\u6216\u8ba1\u7b97\uff09"),
        group);
    status->setProperty("trajectorySectionStatus", true);
    status->setWordWrap(true);
    layout->addWidget(status);

    auto *actions = new QHBoxLayout;
    auto *actionButton = new QPushButton(actionText, group);
    actionButton->setObjectName(actionObjectName);
    // The trajectory services are not wired to the UI yet, so the entries stay
    // visible but disabled instead of pretending a file or a state exists.
    actionButton->setEnabled(false);
    actionButton->setToolTip(uiText(u"\u7b49\u5f85\u4e1a\u52a1\u670d\u52a1\u63a5\u5165"));
    actions->addWidget(actionButton);
    actions->addStretch();
    layout->addLayout(actions);
    return group;
}

} // namespace aerosim
