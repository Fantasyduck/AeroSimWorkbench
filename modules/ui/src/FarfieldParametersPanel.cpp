#include "aerosim/ui/FarfieldParametersPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLocale>
#include <QSignalBlocker>

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}
class FarfieldSpinBox final : public QDoubleSpinBox {
public:
    explicit FarfieldSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {}
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
}

FarfieldParametersPanel::FarfieldParametersPanel(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("farfieldParameters"));
    form_ = new QFormLayout(this);
    form_->setContentsMargins(0, 0, 0, 0);
    form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form_->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form_->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form_->setHorizontalSpacing(16);
    form_->setVerticalSpacing(8);
    type_ = new QComboBox(this);
    type_->setObjectName(QStringLiteral("farfieldParameterType"));
    type_->addItem(uiText(u"单位雷诺数和静温"), "reynolds-temperature");
    type_->addItem(uiText(u"静压和静温"), "static-temperature-pressure");
    type_->addItem(uiText(u"总压和总温"), "total-temperature-pressure");
    type_->addItem(uiText(u"高度"), "altitude");
    type_->setMaximumWidth(280);
    form_->addRow(uiText(u"选择参数"), type_);
    const auto number = [this](const QString &key, const QString &label, double minimum, double maximum) {
        auto *editor = new FarfieldSpinBox(this);
        editor->setObjectName(QStringLiteral("farfield_") + key);
        editor->setDecimals(6);
        editor->setRange(minimum, maximum);
        editor->setSingleStep(0.1);
        editor->setMaximumWidth(180);
        editor->setKeyboardTracking(false);
        numbers_.insert(key, editor);
        form_->addRow(label, editor);
        connect(editor, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, key](double value) {
            parameters_[key] = value;
            emit parametersChanged();
        });
    };
    number("angleOfAttack", uiText(u"攻角 (°)"), -180.0, 180.0);
    number("sideslipAngle", uiText(u"侧滑角 (°)"), -180.0, 180.0);
    number("machNumber", uiText(u"马赫数"), 0.0, 1e4);
    number("unitReynoldsNumber", uiText(u"单位雷诺数 (1/m)"), 0.000001, 1e15);
    number("staticPressure", uiText(u"静压 (Pa)"), 0.000001, 1e15);
    number("staticTemperature", uiText(u"静温 (K)"), 0.000001, 1e7);
    number("totalPressure", uiText(u"总压 (Pa)"), 0.000001, 1e15);
    number("totalTemperature", uiText(u"总温 (K)"), 0.000001, 1e7);
    number("altitude", uiText(u"高度 (m)"), -1e5, 1e9);
    for (const QString &key : {"unitReynoldsNumber", "staticPressure", "staticTemperature",
                              "totalPressure", "totalTemperature", "altitude"}) {
        auto *editor = numbers_.value(key);
        conditionalRows_.insert(key, qMakePair(form_->labelForField(editor), static_cast<QWidget *>(editor)));
    }
    connect(type_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        parameters_["parameterType"] = type_->currentData();
        updateFields();
        emit parametersChanged();
    });
    setParameters({});
}

void FarfieldParametersPanel::setParameters(const QVariantMap &parameters)
{
    parameters_ = parameters;
    const QVariantMap defaults = {{"parameterType", "static-temperature-pressure"},
        {"angleOfAttack", 0.0}, {"sideslipAngle", 0.0}, {"machNumber", 3.0},
        {"unitReynoldsNumber", 1.0}, {"staticPressure", 955.0}, {"staticTemperature", 60.0},
        {"totalPressure", 955.0}, {"totalTemperature", 60.0}, {"altitude", 1.0}};
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
        if (!parameters_.contains(it.key())) parameters_.insert(it.key(), it.value());
    const QSignalBlocker blockType(type_);
    type_->setCurrentIndex(qMax(0, type_->findData(parameters_.value("parameterType"))));
    parameters_["parameterType"] = type_->currentData();
    for (auto it = numbers_.cbegin(); it != numbers_.cend(); ++it) {
        const QSignalBlocker blockNumber(it.value());
        it.value()->setValue(parameters_.value(it.key()).toDouble());
        parameters_[it.key()] = it.value()->value();
    }
    updateFields();
}

QVariantMap FarfieldParametersPanel::parameters() const
{
    return parameters_;
}

void FarfieldParametersPanel::updateFields()
{
    // Remove inactive form rows entirely so Qt 5 does not leave blank gaps.
    for (const auto &widgets : conditionalRows_) {
        if (form_->indexOf(widgets.second) >= 0) {
            const auto row = form_->takeRow(widgets.second);
            delete row.labelItem;
            delete row.fieldItem;
        }
        widgets.first->hide();
        widgets.second->hide();
    }
    QStringList visible;
    const QString type = type_->currentData().toString();
    if (type == "reynolds-temperature") visible = QStringList{"unitReynoldsNumber", "staticTemperature"};
    else if (type == "static-temperature-pressure") visible = QStringList{"staticPressure", "staticTemperature"};
    else if (type == "total-temperature-pressure") visible = QStringList{"totalPressure", "totalTemperature"};
    else visible = QStringList{"altitude"};
    for (const auto &key : visible) {
        const auto widgets = conditionalRows_.value(key);
        form_->addRow(widgets.first, widgets.second);
        widgets.first->show();
        widgets.second->show();
    }
}

} // namespace aerosim
