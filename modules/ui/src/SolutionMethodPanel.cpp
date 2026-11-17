#include "aerosim/ui/SolutionMethodPanel.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace aerosim {
namespace {
template <size_t N> QString text(const char16_t (&s)[N]) {
    return QString::fromUtf16(reinterpret_cast<const ushort *>(s), int(N) - 1);
}
class Number final : public QDoubleSpinBox {
public:
    explicit Number(QWidget *parent) : QDoubleSpinBox(parent) {
        setDecimals(8); setRange(0, 1e12); setKeyboardTracking(false);
    }
    QString textFromValue(double value) const override {
        auto result = QDoubleSpinBox::textFromValue(value);
        const int point = result.indexOf(locale().decimalPoint());
        if (point >= 0) while (result.size() > point + 2 && result.endsWith(locale().zeroDigit())) result.chop(1);
        return result;
    }
};
}
SolutionMethodPanel::SolutionMethodPanel(const QVariantMap &draft, QWidget *parent)
    : QWidget(parent), draft_(draft) {
    setObjectName("solutionMethodPanel"); setProperty("parameterPanel", true);
    setAttribute(Qt::WA_StyledBackground, true);
    const QVariantMap defaults{{"spatialScheme", "hllc"}, {"limiter", "vanleer"}, {"timeScheme", "bdf1"},
        {"timeStep", 0.001}, {"subIterations", 10}, {"cflPointCount", 2}, {"strongStability", false},
        {"cflPoints", QVariantList{QVariantMap{{"iteration", 1}, {"cfl", 0.}}, QVariantMap{{"iteration", 2000}, {"cfl", 5.}}}}};
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
        if (!draft_.contains(it.key())) draft_[it.key()] = it.value();
    auto *layout = new QGridLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(16);
    layout->setColumnStretch(0, 1); layout->setColumnStretch(1, 1);
    auto *group = new QGroupBox(text(u"求解方法"), this); layout->addWidget(group, 0, 0);
    auto *form = new QFormLayout(group); form->setHorizontalSpacing(18); form->setVerticalSpacing(8);
    form->setFormAlignment(Qt::AlignTop);
    form_ = form;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    const auto addRow = [=](const QString &caption, QWidget *editor, bool unsteady = false) {
        auto *label = new QLabel(caption, group); label->setBuddy(editor);
        form->addRow(label, editor);
        if (unsteady) { unsteadyFields_.append(label); unsteadyFields_.append(editor); }
    };
    const auto choice = [=](const QString &caption, const QString &key,
                            const QList<QPair<QString, QString>> &options, bool unsteady = false) {
        auto *editor = new QComboBox(group); editor->setObjectName(key);
        editor->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        editor->setMinimumContentsLength(16);
        for (const auto &option : options) editor->addItem(option.first, option.second);
        editor->setCurrentIndex(qMax(0, editor->findData(draft_.value(key)))); draft_[key] = editor->currentData();
        addRow(caption, editor, unsteady);
        connect(editor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
            draft_[key] = editor->currentData(); emit draftChanged(draft_);
        });
    };
    choice(text(u"空间离散格式"), "spatialScheme", {{text(u"Roe格式"), "roe"}, {"Vanleer", "vanleer"},
        {"AUSM", "ausm"}, {"AUSMPW+", "ausmpw-plus"}, {"HLLC", "hllc"}});
    choice(text(u"流场限制器"), "limiter", {{text(u"无"), "none"}, {"vanleer", "vanleer"},
        {"vanalbada", "vanalbada"}, {"hvanleer", "hvanleer"}, {"hvanalbada", "hvanalbada"}});
    choice(text(u"时间离散格式"), "timeScheme", {
        {text(u"一阶向后差分格式"), "bdf1"}, {text(u"二阶向后差分格式"), "bdf2"},
        {text(u"优化的二阶向后差分格式"), "optimized-bdf2"}, {text(u"三阶向后差分格式"), "bdf3"},
        {text(u"修正的四阶向后差分格式"), "modified-bdf4"}, {text(u"强保稳的三阶Runge-Kutta格式"), "ssprk3"}}, true);
    auto *step = new Number(group); step->setObjectName("timeStep"); step->setMinimum(0.00000001);
    step->setValue(draft_.value("timeStep").toDouble()); draft_["timeStep"] = step->value();
    addRow(text(u"时间步长 (s)"), step, true);
    connect(step, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double value) {
        draft_["timeStep"] = value; emit draftChanged(draft_);
    });
    auto *sub = new QSpinBox(group); sub->setObjectName("subIterations"); sub->setRange(1, 1000000000);
    sub->setValue(draft_.value("subIterations").toInt()); sub->setKeyboardTracking(false);
    draft_["subIterations"] = sub->value(); addRow(text(u"子迭代步数"), sub, true);
    connect(sub, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
        draft_["subIterations"] = value; emit draftChanged(draft_);
    });
    auto *cflGroup = new QGroupBox(text(u"CFL 数渐变"), this); layout->addWidget(cflGroup, 0, 1);
    auto *cflLayout = new QVBoxLayout(cflGroup);
    auto *count = new QSpinBox(cflGroup); count->setObjectName("cflPointCount"); count->setRange(1, 1000);
    count->setValue(draft_.value("cflPointCount").toInt()); count->setMaximumWidth(220); count->setKeyboardTracking(false);
    draft_["cflPointCount"] = count->value();
    auto *countForm = new QFormLayout;
    countForm->addRow(text(u"渐变点数"), count);
    cflLayout->addLayout(countForm);
    auto *stable = new QCheckBox(text(u"开启强保稳"), group); stable->setObjectName("strongStability");
    stable->setChecked(draft_.value("strongStability").toBool()); form->addRow(stable);
    connect(stable, &QCheckBox::toggled, this, [=](bool value) { draft_["strongStability"] = value; emit draftChanged(draft_); });
    auto *table = new QTableWidget(cflGroup); table->setObjectName("cflTable"); table->setColumnCount(2);
    table->setHorizontalHeaderLabels({text(u"迭代步数"), text(u"CFL 数")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setDefaultSectionSize(36); table->setMinimumHeight(220);
    cflLayout->addWidget(table);
    const auto rebuild = [=](int size) {
        auto points = draft_.value("cflPoints").toList();
        while (points.size() < size) points.append(QVariantMap{{"iteration", points.isEmpty() ? 1 :
            qMin(1000000000, points.last().toMap().value("iteration").toInt() + 1)}, {"cfl", 0.}});
        draft_["cflPoints"] = points; draft_["cflPointCount"] = size;
        table->setRowCount(0); table->setRowCount(size);
        for (int row = 0; row < size; ++row) {
            auto point = points[row].toMap();
            auto *iteration = new QSpinBox(table); iteration->setRange(1, 1000000000); iteration->setKeyboardTracking(false);
            iteration->setValue(point.value("iteration", 1).toInt());
            auto *cfl = new Number(table); cfl->setValue(point.value("cfl", 0.).toDouble());
            point["iteration"] = iteration->value(); point["cfl"] = cfl->value(); points[row] = point;
            table->setCellWidget(row, 0, iteration); table->setCellWidget(row, 1, cfl);
            const auto update = [=](const QString &key, const QVariant &value) {
                auto rows = draft_.value("cflPoints").toList(); auto entry = rows[row].toMap();
                entry[key] = value; rows[row] = entry; draft_["cflPoints"] = rows; emit draftChanged(draft_);
            };
            connect(iteration, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) { update("iteration", value); });
            connect(cfl, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double value) { update("cfl", value); });
        }
        draft_["cflPoints"] = points;
    };
    rebuild(count->value());
    connect(count, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) { rebuild(value); emit draftChanged(draft_); });
    auto *hint = new QLabel(text(u"定常计算隐藏时间离散、时间步长和子迭代设置；切换计算类型会保留已输入值。"), this);
    hint->setWordWrap(true); hint->setProperty("parameterHint", true); layout->addWidget(hint, 1, 0, 1, 2);
    setUnsteady(false);
}
void SolutionMethodPanel::setUnsteady(bool unsteady) {
    // Remove hidden rows as Qt 5 otherwise retains their vertical spacing.
    for (int i = 0; i < unsteadyFields_.size(); i += 2) {
        auto *label = unsteadyFields_[i];
        auto *editor = unsteadyFields_[i + 1];
        if (form_->indexOf(editor) >= 0) {
            const auto row = form_->takeRow(editor);
            delete row.labelItem;
            delete row.fieldItem;
        }
        label->hide(); editor->hide();
        if (unsteady) {
            form_->insertRow(2 + i / 2, label, editor);
            label->show(); editor->show();
        }
    }
}
}
