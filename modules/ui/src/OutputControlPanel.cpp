#include "aerosim/ui/OutputControlPanel.h"
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
namespace aerosim { namespace {
template <size_t N> QString text(const char16_t (&s)[N]) { return QString::fromUtf16(reinterpret_cast<const ushort *>(s), int(N)-1); }
}}
namespace aerosim {
OutputControlPanel::OutputControlPanel(const QVariantMap &draft, QWidget *parent) : QWidget(parent), draft_(draft) {
    setObjectName("outputControlPanel"); setProperty("parameterPanel", true); setAttribute(Qt::WA_StyledBackground, true);
    const QStringList flowNames{"压力","密度","速度u","速度v","速度w","温度","湍流动能","湍流耗散率","组分"};
    const QStringList flowKeys{"pressure","density","velocity-u","velocity-v","velocity-w","temperature","turbulence-k","turbulence-dissipation","species"};
    const QStringList wallNames{"马赫数","节点状态","部件编号","初始化状态","热流","x方向摩擦力","y方向摩擦力","z方向摩擦力","摩擦力值","总温","总压"};
    const QStringList wallKeys{"mach","node-status","component-id","initialization-status","heat-flux","friction-x","friction-y","friction-z","friction-magnitude","total-temperature","total-pressure"};
    const QStringList variableNames = flowNames + wallNames;
    const QStringList variableKeys = flowKeys + wallKeys;
    const QStringList boundaryNames{"farfield","symmetry","wing_down","wing_edge","wing_up"};
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(14);
    auto *file = new QGroupBox(text(u"输出文件控制"), this);
    auto *intervals = new QGridLayout(file);
    intervals->setHorizontalSpacing(24);
    intervals->setVerticalSpacing(8);
    const auto addInterval = [this, file, intervals](int column, const QString &caption,
                                                   const QString &key, int fallback, int minimum) {
        auto *editor = new QSpinBox(file);
        editor->setObjectName(key);
        editor->setRange(minimum, 1000000000);
        editor->setValue(draft_.value(key, fallback).toInt());
        editor->setKeyboardTracking(false);
        auto *label = new QLabel(caption, file);
        label->setBuddy(editor);
        intervals->addWidget(label, 0, column);
        intervals->addWidget(editor, 1, column);
        intervals->setColumnStretch(column, 1);
        draft_[key] = editor->value();
        connect(editor, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, key](int value) {
            draft_[key] = value;
            emit draftChanged(draft_);
        });
    };
    addInterval(0, text(u"续算文件输出间隔"), "restartInterval", 100, 0);
    addInterval(1, text(u"壁面结果输出间隔"), "wallInterval", -1, -1);
    addInterval(2, text(u"流场结果输出间隔"), "flowInterval", -1, -1);
    layout->addWidget(file);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(14);
    const auto addChoices = [this, columns](const QString &title, const QStringList &names,
                                           const QStringList &keys, const QString &key, int columnCount) {
        auto *group = new QGroupBox(title, this);
        group->setObjectName(key + "Group");
        auto *body = new QVBoxLayout(group);
        body->setSpacing(10);
        auto *toolbar = new QHBoxLayout;
        auto *count = new QLabel(group);
        count->setProperty("parameterHint", true);
        auto *all = new QPushButton(text(u"全选"), group);
        auto *invert = new QPushButton(text(u"反选"), group);
        all->setObjectName(key + "SelectAll");
        invert->setObjectName(key + "Invert");
        all->setAccessibleName(title + text(u"全选"));
        invert->setAccessibleName(title + text(u"反选"));
        toolbar->addWidget(count);
        toolbar->addStretch();
        toolbar->addWidget(all);
        toolbar->addWidget(invert);
        body->addLayout(toolbar);

        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(2);
        QStringList selected = draft_.value(key).toStringList();
        if (!draft_.contains(key) && key == "flowVariables") selected = keys.mid(0, 5);
        if (!draft_.contains(key) && key == "wallVariables") selected = QStringList{"mach"};
        QList<QCheckBox *> choices;
        const int rowCount = (names.size() + columnCount - 1) / columnCount;
        for (int index = 0; index < names.size(); ++index) {
            auto *choice = new QCheckBox(names[index], group);
            choice->setObjectName(key + "_" + keys[index]);
            choice->setChecked(selected.contains(keys[index]));
            choices.append(choice);
            grid->addWidget(choice, index % rowCount, index / rowCount);
        }
        for (int column = 0; column < columnCount; ++column) grid->setColumnStretch(column, 1);
        body->addLayout(grid);
        body->addStretch();
        const auto updateSelection = [this, choices, keys, key, count] {
            QStringList checked;
            for (int index = 0; index < choices.size(); ++index)
                if (choices[index]->isChecked()) checked.append(keys[index]);
            draft_[key] = checked;
            count->setText(text(u"已选 %1 / %2").arg(checked.size()).arg(choices.size()));
        };
        updateSelection();
        for (auto *choice : choices)
            connect(choice, &QCheckBox::toggled, this, [this, updateSelection](bool) {
                updateSelection();
                emit draftChanged(draft_);
            });
        const auto bulkSelect = [this, choices, updateSelection](bool invertSelection) {
            for (auto *choice : choices) {
                const QSignalBlocker blocker(choice);
                choice->setChecked(invertSelection ? !choice->isChecked() : true);
            }
            updateSelection();
            emit draftChanged(draft_);
        };
        connect(all, &QPushButton::clicked, this, [bulkSelect] { bulkSelect(false); });
        connect(invert, &QPushButton::clicked, this, [bulkSelect] { bulkSelect(true); });
        columns->addWidget(group, columnCount);
    };
    addChoices(text(u"流场输出变量"), variableNames, variableKeys, "flowVariables", 2);
    addChoices(text(u"壁面输出变量"), variableNames, variableKeys, "wallVariables", 2);
    addChoices(text(u"边界条件收敛历史"), boundaryNames, boundaryNames, "boundaryHistory", 1);
    layout->addLayout(columns);
}
}
