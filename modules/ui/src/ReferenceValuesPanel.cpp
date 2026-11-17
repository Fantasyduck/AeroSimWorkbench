#include "aerosim/ui/ReferenceValuesPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>

namespace aerosim {
namespace {
class ReferenceValueSpinBox final : public QDoubleSpinBox {
public:
    explicit ReferenceValueSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {}

protected:
    QString textFromValue(double value) const override
    {
        QString text = QDoubleSpinBox::textFromValue(value);
        const int decimalPosition = text.indexOf(locale().decimalPoint());
        // Match the reference's one decimal place without rounding entered values.
        if (decimalPosition >= 0)
            while (text.size() > decimalPosition + 2 && text.endsWith(locale().zeroDigit()))
                text.chop(1);
        return text;
    }
};

template <size_t N> QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}
QFormLayout *makeForm(QWidget *parent)
{
    auto *form = new QFormLayout(parent);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(8);
    return form;
}
}

ReferenceValuesPanel::ReferenceValuesPanel(const QVariantMap &draft, QWidget *parent)
    : QWidget(parent), draft_(draft)
{
    setObjectName(QStringLiteral("referenceValuesPanel"));
    setProperty("parameterPanel", true);
    setAttribute(Qt::WA_StyledBackground, true);
    const QVariantMap defaults = {{"momentX", 0.0}, {"momentY", 0.0}, {"momentZ", 0.0},
        {"chordLength", 1.0}, {"spanLength", 1.0}, {"referenceArea", 1.0},
        {"inputMode", "boundary"}, {"parameterType", "reynolds-temperature"},
        {"machNumber", 3.0}, {"unitReynoldsNumber", 1.0}, {"staticTemperature", 60.0},
        {"staticPressure", 955.0}, {"totalTemperature", 60.0}, {"totalPressure", 955.0},
        {"altitude", 1.0}, {"angleOfAttack", 0.0}, {"sideslipAngle", 0.0}};
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
        if (!draft_.contains(it.key())) draft_.insert(it.key(), it.value());

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    auto *geometry = new QGroupBox(uiText(u"几何参考量"), this);
    geometry->setObjectName(QStringLiteral("referenceGeometryGroup"));
    auto *geometryForm = makeForm(geometry);
    layout->addWidget(geometry, 1);

    const auto number = [this](QWidget *owner, const QString &key, double minimum, double maximum) {
        auto *editor = new ReferenceValueSpinBox(owner);
        editor->setObjectName(key);
        editor->setDecimals(6);
        editor->setRange(minimum, maximum);
        editor->setSingleStep(0.1);
        editor->setValue(draft_.value(key).toDouble());
        editor->setMinimumWidth(100);
        editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        editor->setKeyboardTracking(false);
        draft_[key] = editor->value();
        connect(editor, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, key](double value) { draft_[key] = value; publish(); });
        return editor;
    };
    auto *point = new QWidget(geometry);
    auto *pointLayout = new QHBoxLayout(point);
    pointLayout->setContentsMargins(0, 0, 0, 0);
    pointLayout->setSpacing(8);
    const QStringList axes = {"X", "Y", "Z"};
    const QStringList pointKeys = {"momentX", "momentY", "momentZ"};
    for (int index = 0; index < 3; ++index) {
        auto *label = new QLabel(axes[index], point);
        auto *editor = number(point, pointKeys[index], -1e9, 1e9);
        editor->setMinimumWidth(80);
        editor->setAccessibleName(uiText(u"力矩参考点 ") + axes[index]);
        label->setBuddy(editor);
        pointLayout->addWidget(label);
        pointLayout->addWidget(editor, 1);
    }
    geometryForm->addRow(new QLabel(uiText(u"力矩参考点 (m)"), geometry));
    geometryForm->addRow(point);
    geometryForm->addRow(uiText(u"气动弦长 (m)"), number(geometry, "chordLength", 0.000001, 1e9));
    geometryForm->addRow(uiText(u"气动展长 (m)"), number(geometry, "spanLength", 0.000001, 1e9));
    geometryForm->addRow(uiText(u"参考面积 (m²)"), number(geometry, "referenceArea", 0.000001, 1e12));

    auto *flow = new QGroupBox(uiText(u"来流参考量"), this);
    flow->setObjectName(QStringLiteral("referenceFlowGroup"));
    flowForm_ = makeForm(flow);
    layout->addWidget(flow, 1);
    inputMode_ = new QComboBox(flow);
    inputMode_->setObjectName(QStringLiteral("referenceInputMode"));
    inputMode_->addItem(uiText(u"从边界条件获取"), "boundary");
    inputMode_->addItem(uiText(u"自定义输入"), "custom");
    inputMode_->setCurrentIndex(qMax(0, inputMode_->findData(draft_.value("inputMode"))));
    flowForm_->addRow(uiText(u"输入方式"), inputMode_);

    // Keep the source row as a single unit so custom mode hides its label as well.
    boundarySource_ = new QWidget(flow);
    boundarySource_->setObjectName(QStringLiteral("referenceBoundarySource"));
    auto *sourceLayout = new QVBoxLayout(boundarySource_);
    sourceLayout->setContentsMargins(0, 0, 0, 0);
    sourceLayout->setSpacing(4);
    auto *boundary = new QComboBox(boundarySource_);
    boundary->setObjectName(QStringLiteral("referenceBoundary"));
    boundary->addItem(uiText(u"暂无可获取的边界来流参数"));
    boundary->setEnabled(false);
    sourceLayout->addWidget(boundary);
    auto *hint = new QLabel(uiText(u"边界来流参数尚未接入，可选择自定义输入。"), boundarySource_);
    hint->setWordWrap(true);
    hint->setProperty("parameterHint", true);
    sourceLayout->addWidget(hint);
    flowForm_->addRow(uiText(u"从边界条件获取"), boundarySource_);
    conditionalRows_.insert("boundary", qMakePair(flowForm_->labelForField(boundarySource_), boundarySource_));

    parameterType_ = new QComboBox(flow);
    parameterType_->setObjectName(QStringLiteral("referenceParameterType"));
    parameterType_->addItem(uiText(u"单位雷诺数和静温"), "reynolds-temperature");
    parameterType_->addItem(uiText(u"静温和静压"), "static-temperature-pressure");
    parameterType_->addItem(uiText(u"总温和总压"), "total-temperature-pressure");
    parameterType_->addItem(uiText(u"高度"), "altitude");
    parameterType_->setCurrentIndex(qMax(0, parameterType_->findData(draft_.value("parameterType"))));
    flowForm_->addRow(uiText(u"参数类型"), parameterType_);
    flowForm_->addRow(uiText(u"马赫数"), number(flow, "machNumber", 0.0, 1e4));
    flowForm_->addRow(uiText(u"单位雷诺数 (1/m)"), number(flow, "unitReynoldsNumber", 0.000001, 1e15));
    flowForm_->addRow(uiText(u"静温 (K)"), number(flow, "staticTemperature", 0.000001, 1e7));
    flowForm_->addRow(uiText(u"静压 (Pa)"), number(flow, "staticPressure", 0.000001, 1e15));
    flowForm_->addRow(uiText(u"总温 (K)"), number(flow, "totalTemperature", 0.000001, 1e7));
    flowForm_->addRow(uiText(u"总压 (Pa)"), number(flow, "totalPressure", 0.000001, 1e15));
    flowForm_->addRow(uiText(u"高度 (m)"), number(flow, "altitude", -1e5, 1e9));
    flowForm_->addRow(uiText(u"攻角 (°)"), number(flow, "angleOfAttack", -180.0, 180.0));
    flowForm_->addRow(uiText(u"侧滑角 (°)"), number(flow, "sideslipAngle", -180.0, 180.0));
    const QStringList conditionalKeys = {"unitReynoldsNumber", "staticTemperature", "staticPressure",
                                        "totalTemperature", "totalPressure", "altitude"};
    for (const auto &key : conditionalKeys) {
        auto *editor = findChild<QDoubleSpinBox *>(key);
        conditionalRows_.insert(key, qMakePair(flowForm_->labelForField(editor), static_cast<QWidget *>(editor)));
    }
    const auto changed = [this](int) { updateVisibility(); publish(); };
    connect(inputMode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
    connect(parameterType_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
    updateVisibility();
}

void ReferenceValuesPanel::updateVisibility()
{
    draft_["inputMode"] = inputMode_->currentData();
    draft_["parameterType"] = parameterType_->currentData();
    // Qt 5 form layouts retain spacing for hidden rows. Remove the layout items
    // and reinsert only active rows, keeping the widgets and their entered values.
    for (const auto &widgets : conditionalRows_) {
        if (flowForm_->indexOf(widgets.second) >= 0) {
            const auto row = flowForm_->takeRow(widgets.second);
            delete row.labelItem;
            delete row.fieldItem;
        }
        widgets.first->hide();
        widgets.second->hide();
    }
    const auto insertRow = [this](int index, const QString &key) {
        const auto widgets = conditionalRows_.value(key);
        flowForm_->insertRow(index, widgets.first, widgets.second);
        widgets.first->show();
        widgets.second->show();
    };
    const bool boundaryMode = inputMode_->currentData() == "boundary";
    if (boundaryMode) insertRow(1, "boundary");
    int nextRow = boundaryMode ? 4 : 3;
    const QString type = parameterType_->currentData().toString();
    const QStringList keys = {"unitReynoldsNumber", "staticTemperature", "staticPressure",
                              "totalTemperature", "totalPressure", "altitude"};
    for (const auto &key : keys) {
        const bool visible = (type == "reynolds-temperature" && (key == "unitReynoldsNumber" || key == "staticTemperature"))
            || (type == "static-temperature-pressure" && (key == "staticTemperature" || key == "staticPressure"))
            || (type == "total-temperature-pressure" && (key == "totalTemperature" || key == "totalPressure"))
            || (type == "altitude" && key == "altitude");
        if (visible) insertRow(nextRow++, key);
    }
}

void ReferenceValuesPanel::publish()
{
    emit draftChanged(draft_);
}

} // namespace aerosim
