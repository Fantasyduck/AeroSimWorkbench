#include "aerosim/ui/BoundaryConditionsPanel.h"
#include "aerosim/ui/FarfieldParametersPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLocale>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QResizeEvent>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVector>

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}

class BoundaryValueSpinBox final : public QDoubleSpinBox {
public:
    explicit BoundaryValueSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {}
protected:
    QString textFromValue(double value) const override
    {
        QString text = QDoubleSpinBox::textFromValue(value);
        const int point = text.indexOf(locale().decimalPoint());
        if (point >= 0)
            while (text.size() > point + 2 && text.endsWith(locale().zeroDigit())) text.chop(1);
        return text;
    }
};

QStringList parameterKeys(const QString &type)
{
    if (type == "no-slip-isothermal-wall" || type == "no-slip-isothermal-wall-wall-function")
        return {"wallTemperature"};
    if (type == "mass-flow-subsonic-outlet") return {"massFlowRate"};
    if (type == "mass-flow-subsonic-inlet") return {"massFlowRate", "totalTemperature"};
    if (type == "pressure-subsonic-outlet") return {"outletStaticPressure"};
    if (type == "pressure-subsonic-inlet")
        return {"pressureRelaxationFactor", "inletTemperature", "inletTotalPressure"};
    if (type == "mixed-pressure-outlet") return {"pressureRelaxationFactor", "outletStaticPressure"};
    return {};
}

double parameterDefault(const QString &key)
{
    if (key == "pressureRelaxationFactor") return 1.0;
    if (key == "outletStaticPressure" || key == "inletTotalPressure") return 1000.0;
    return key == "massFlowRate" ? 100.0 : 300.0;
}

const QVector<QPair<QString, QString>> &boundaryTypes()
{
    // Stable UI draft identifiers; these are not solver boundary codes.
    static const QVector<QPair<QString, QString>> types = {
        {"farfield", uiText(u"远场")},
        {"freestream", uiText(u"自由来流")},
        {"symmetry", uiText(u"对称面")},
        {"overset", uiText(u"重叠网格")},
        {"supersonic-outlet", uiText(u"超声速出口(外推边界)")},
        {"inviscid-wall", uiText(u"无粘壁面")},
        {"inviscid-wall-wall-function", uiText(u"无粘壁面(壁面函数)")},
        {"no-slip-adiabatic-wall", uiText(u"无滑移绝热壁面")},
        {"no-slip-adiabatic-wall-wall-function", uiText(u"无滑移绝热壁面(壁面函数)")},
        {"no-slip-isothermal-wall", uiText(u"无滑移等温壁面")},
        {"no-slip-isothermal-wall-wall-function", uiText(u"无滑移等温壁面(壁面函数)")},
        {"mass-flow-subsonic-outlet", uiText(u"质量流量亚音速出口")},
        {"mass-flow-subsonic-inlet", uiText(u"质量流量亚音速入口")},
        {"pressure-subsonic-outlet", uiText(u"压力亚音速出口")},
        {"pressure-subsonic-inlet", uiText(u"压力亚音速入口")},
        {"mixed-pressure-outlet", uiText(u"超音速和亚声速混合压力出口")}
    };
    return types;
}

QVariantList sampleBoundaries()
{
    QVariantList rows;
    for (const QString &name : {QStringLiteral("symmetry"), QStringLiteral("wing_up"),
                               QStringLiteral("wing_edge"), QStringLiteral("wing_down"), QStringLiteral("farfield")}) {
        const QString type = name == "symmetry" ? "symmetry" : name == "farfield" ? "farfield" : "inviscid-wall";
        rows.append(QVariantMap{{"id", name}, {"name", name}, {"type", type}, {"mesh", "M6"}});
    }
    return rows;
}
}

BoundaryConditionsPanel::BoundaryConditionsPanel(const QVariantMap &draft, QWidget *parent)
    : QWidget(parent), draft_(draft)
{
    setObjectName(QStringLiteral("boundaryConditionsPanel"));
    draft_.insert("boundaries", draft.value("boundaries", sampleBoundaries()));
    draft_.insert("sampleData", true);
    setProperty("parameterPanel", true);
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *sample = new QLabel(uiText(u"M6 示例边界 · 尚未从实际网格读取"), this);
    sample->setProperty("parameterHint", true);
    layout->addWidget(sample);
    auto *content = new QHBoxLayout;
    bodyLayout_ = content;
    content->setSpacing(16);
    table_ = new QTreeWidget(this);
    table_->setObjectName(QStringLiteral("boundaryTable"));
    table_->setHeaderLabels({uiText(u"边界名称"), uiText(u"边界类型"), uiText(u"所属网格")});
    table_->setRootIsDecorated(false);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setMinimumHeight(260);
    table_->header()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSortingEnabled(false);
    QTreeWidgetItem *initial = nullptr;
    for (const QVariant &value : draft_.value("boundaries").toList()) {
        const auto row = value.toMap();
        auto *item = new QTreeWidgetItem(table_, {row.value("name").toString(), typeLabel(row.value("type").toString()), row.value("mesh").toString()});
        item->setData(0, Qt::UserRole, row.value("id"));
        item->setData(1, Qt::UserRole, row.value("type"));
        item->setToolTip(1, item->text(1));
        item->setIcon(1, QIcon(QStringLiteral(":/aerosim/postprocess/parameters.svg")));
        if (row.value("id").toString() == draft_.value("selectedId", "wing_edge").toString()) initial = item;
    }
    content->addWidget(table_, 3);
    auto *editColumn = new QVBoxLayout;
    editColumn->setSpacing(12);
    auto *editor = new QGroupBox(uiText(u"边界条件"), this);
    auto *form = new QFormLayout(editor);
    editorForm_ = form;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setVerticalSpacing(10);
    name_ = new QLineEdit(editor);
    name_->setObjectName(QStringLiteral("boundaryName"));
    name_->setMaxLength(128);
    type_ = new QComboBox(editor);
    type_->setObjectName(QStringLiteral("boundaryType"));
    type_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    type_->setMinimumContentsLength(16);
    type_->setMaxVisibleItems(16);
    for (const auto &type : boundaryTypes()) {
        type_->addItem(type.second, type.first);
        type_->setItemData(type_->count() - 1, type.second, Qt::ToolTipRole);
    }
    form->addRow(uiText(u"名称"), name_);
    form->addRow(uiText(u"边界类型"), type_);
    const auto addTypeField = [this, editor, form](const QString &key, const QString &label) {
        auto *number = new BoundaryValueSpinBox(editor);
        number->setObjectName(QStringLiteral("boundary_") + key);
        number->setDecimals(6);
        number->setRange(key == "massFlowRate" || key == "pressureRelaxationFactor" ? 0.0 : 0.000001,
                         key == "pressureRelaxationFactor" ? 1.0 : 1e9);
        number->setValue(parameterDefault(key));
        number->setSingleStep(0.1);
        number->setMaximumWidth(180);
        number->setKeyboardTracking(false);
        form->addRow(label, number);
        typeFields_.insert(key, qMakePair(form->labelForField(number), static_cast<QDoubleSpinBox *>(number)));
        connect(number, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
            if (!editingType_.isEmpty()) typeParameters_[editingType_] = currentTypeParameters();
            validateEdit();
        });
    };
    addTypeField("wallTemperature", uiText(u"壁面温度 (K)"));
    addTypeField("massFlowRate", uiText(u"质量流量"));
    addTypeField("totalTemperature", uiText(u"总温 (K)"));
    addTypeField("pressureRelaxationFactor", uiText(u"压力松弛因子"));
    addTypeField("outletStaticPressure", uiText(u"出口静压 (Pa)"));
    addTypeField("inletTemperature", uiText(u"进口温度 (K)"));
    addTypeField("inletTotalPressure", uiText(u"进口总压 (Pa)"));
    editColumn->addWidget(editor);
    auto *farfieldGroup = new QGroupBox(uiText(u"远场来流参数"), this);
    farfieldGroup->setObjectName(QStringLiteral("farfieldSettings"));
    auto *farfieldLayout = new QVBoxLayout(farfieldGroup);
    farfield_ = new FarfieldParametersPanel(farfieldGroup);
    farfieldLayout->addWidget(farfield_);
    editColumn->addWidget(farfieldGroup);
    auto *footer = new QHBoxLayout;
    feedback_ = new QLabel(this);
    feedback_->setObjectName(QStringLiteral("boundaryFeedback"));
    feedback_->setWordWrap(true);
    feedback_->setProperty("parameterHint", true);
    footer->addWidget(feedback_, 1);
    apply_ = new QPushButton(uiText(u"应用"), this);
    apply_->setObjectName(QStringLiteral("applyBoundary"));
    footer->addWidget(apply_);
    editColumn->addLayout(footer);
    editColumn->addStretch();
    content->addLayout(editColumn, 2);
    layout->addLayout(content);
    connect(table_, &QTreeWidget::currentItemChanged, this, [this] { loadSelection(); });
    connect(name_, &QLineEdit::textChanged, this, [this] { validateEdit(); });
    connect(type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, farfieldGroup] {
        updateTypeFields();
        farfieldGroup->setVisible(type_->currentData() == "farfield");
        type_->setToolTip(type_->currentText());
        validateEdit();
    });
    connect(farfield_, &FarfieldParametersPanel::parametersChanged, this, &BoundaryConditionsPanel::validateEdit);
    connect(apply_, &QPushButton::clicked, this, &BoundaryConditionsPanel::applyEdit);
    if (!initial) initial = table_->topLevelItem(0);
    table_->setCurrentItem(initial);
    if (!initial) loadSelection();
    // Keep screenshot order initially; header clicks can sort without changing row identity.
    table_->header()->setSortIndicator(-1, Qt::AscendingOrder);
    table_->setSortingEnabled(true);
}

QString BoundaryConditionsPanel::typeLabel(const QString &type) const
{
    for (const auto &entry : boundaryTypes())
        if (entry.first == type) return entry.second;
    return type;
}

void BoundaryConditionsPanel::loadSelection()
{
    const QSignalBlocker blockName(name_);
    const QSignalBlocker blockType(type_);
    auto *item = table_->currentItem();
    name_->setEnabled(item != nullptr);
    type_->setEnabled(item != nullptr);
    name_->setText(item ? item->text(0) : QString());
    type_->setCurrentIndex(item ? type_->findData(item->data(1, Qt::UserRole)) : -1);
    type_->setToolTip(type_->currentText());
    QVariantMap savedParameters;
    typeParameters_.clear();
    if (item) {
        for (const auto &value : draft_.value("boundaries").toList()) {
            const auto row = value.toMap();
            if (row.value("id") == item->data(0, Qt::UserRole)) {
                savedParameters = row.value("farfield").toMap();
                typeParameters_ = row.value("typeParameters").toMap();
                break;
            }
        }
    }
    farfield_->setParameters(savedParameters);
    updateTypeFields();
    farfield_->parentWidget()->setVisible(item && type_->currentData() == "farfield");
    validateEdit();
}

void BoundaryConditionsPanel::validateEdit()
{
    auto *item = table_->currentItem();
    QString message;
    const QString name = name_->text().trimmed();
    if (!item) message = uiText(u"请选择边界。");
    else if (name.isEmpty()) message = uiText(u"边界名称不能为空。");
    else {
        for (int i = 0; i < table_->topLevelItemCount(); ++i) {
            auto *other = table_->topLevelItem(i);
            if (other != item && other->text(2) == item->text(2) && other->text(0) == name) {
                message = uiText(u"同一网格中的边界名称不能重复。");
                break;
            }
        }
    }
    bool parametersChanged = false;
    if (item) {
        for (const auto &value : draft_.value("boundaries").toList()) {
            const auto row = value.toMap();
            if (row.value("id") == item->data(0, Qt::UserRole)) {
                if (type_->currentData() == "farfield")
                    parametersChanged = row.value("farfield").toMap() != farfield_->parameters();
                else if (!parameterKeys(editingType_).isEmpty())
                    parametersChanged = row.value("typeParameters").toMap().value(editingType_).toMap() != currentTypeParameters();
                break;
            }
        }
    }
    const bool changed = item && (name != item->text(0) || type_->currentData() != item->data(1, Qt::UserRole) || parametersChanged);
    apply_->setEnabled(message.isEmpty() && changed && type_->currentIndex() >= 0);
    feedback_->setText(!message.isEmpty() ? message : changed ? uiText(u"修改尚未应用；切换边界将放弃未应用的修改。") : uiText(u"选择边界后编辑名称和类型。"));
}

void BoundaryConditionsPanel::applyEdit()
{
    validateEdit();
    if (!apply_->isEnabled()) return;
    auto *item = table_->currentItem();
    const QString id = item->data(0, Qt::UserRole).toString();
    auto rows = draft_.value("boundaries").toList();
    for (auto &value : rows) {
        auto row = value.toMap();
        if (row.value("id").toString() != id) continue;
        row.insert("name", name_->text().trimmed());
        row.insert("type", type_->currentData());
        if (type_->currentData() == "farfield") row.insert("farfield", farfield_->parameters());
        if (!parameterKeys(editingType_).isEmpty()) typeParameters_[editingType_] = currentTypeParameters();
        if (!typeParameters_.isEmpty()) row.insert("typeParameters", typeParameters_);
        value = row;
        item->setText(0, row.value("name").toString());
        item->setData(1, Qt::UserRole, row.value("type"));
        item->setText(1, typeLabel(row.value("type").toString()));
        item->setToolTip(1, item->text(1));
        break;
    }
    draft_.insert("boundaries", rows);
    draft_.insert("selectedId", id);
    emit draftChanged(draft_);
    loadSelection();
    feedback_->setText(uiText(u"已应用到当前窗口草稿。"));
}

QVariantMap BoundaryConditionsPanel::currentTypeParameters() const
{
    QVariantMap result = typeParameters_.value(editingType_).toMap();
    for (const auto &key : parameterKeys(editingType_)) result[key] = typeFields_.value(key).second->value();
    return result;
}

void BoundaryConditionsPanel::updateTypeFields()
{
    editingType_ = type_->currentData().toString();
    const auto saved = typeParameters_.value(editingType_).toMap();
    for (const auto &field : typeFields_) {
        if (editorForm_->indexOf(field.second) >= 0) {
            const auto row = editorForm_->takeRow(field.second);
            delete row.labelItem;
            delete row.fieldItem;
        }
        field.first->hide();
        field.second->hide();
    }
    for (const auto &key : parameterKeys(editingType_)) {
        const auto field = typeFields_.value(key);
        const QSignalBlocker block(field.second);
        field.second->setValue(saved.value(key, parameterDefault(key)).toDouble());
        editorForm_->addRow(field.first, field.second);
        field.first->show();
        field.second->show();
    }
}

void BoundaryConditionsPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const bool stacked = width() < 900;
    bodyLayout_->setDirection(stacked ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    bodyLayout_->setStretch(0, stacked ? 0 : 3);
    bodyLayout_->setStretch(1, stacked ? 0 : 2);
    table_->setMaximumHeight(stacked ? 300 : QWIDGETSIZE_MAX);
}

} // namespace aerosim
