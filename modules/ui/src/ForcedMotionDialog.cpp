#include "aerosim/ui/ForcedMotionDialog.h"
#include "MotionDialogLayout.h"
#include <QGroupBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace aerosim {
namespace {
template <size_t N> QString text(const char16_t (&s)[N]) {
    return QString::fromUtf16(reinterpret_cast<const ushort *>(s), int(N) - 1);
}
class Number final : public QDoubleSpinBox {
public:
    explicit Number(QWidget *parent) : QDoubleSpinBox(parent) {}
    QString textFromValue(double value) const override {
        auto result = QDoubleSpinBox::textFromValue(value);
        const int point = result.indexOf(locale().decimalPoint());
        if (point >= 0) while (result.size() > point + 2 && result.endsWith(locale().zeroDigit())) result.chop(1);
        return result;
    }
};
}
ForcedMotionDialog::ForcedMotionDialog(const QVariantMap &parameters, QWidget *parent)
    : QDialog(parent), parameters_(parameters)
{
    setObjectName("forcedMotionDialog");
    setWindowTitle(text(u"强迫运动参数设置"));
    setProperty("parameterEditor", true);
    setStyleSheet(QStringLiteral("QAbstractSpinBox, QComboBox, QLineEdit { min-height: 22px; max-height: 22px; padding: 2px 6px; }"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    content->setProperty("parameterSurface", true);
    auto *body = new QVBoxLayout(content);
    auto *columns = new motionLayout::Columns(content);
    body->addWidget(columns);
    body->addStretch();
    auto *general = new QGroupBox(text(u"运动类型与位置"), columns);
    auto *form = new QFormLayout(general);
    columns->items->addWidget(general, 1);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setVerticalSpacing(8);
    scroll->setWidget(content);
    layout->addWidget(scroll);
    const auto number = [this](QWidget *owner, const QString &key, double fallback, double minimum = -1e9) {
        auto *value = new Number(owner);
        value->setObjectName(key);
        value->setDecimals(6);
        value->setRange(minimum, 1e9);
        value->setValue(parameters_.value(key, fallback).toDouble());
        value->setMinimumWidth(85);
        value->setMaximumWidth(180);
        parameters_[key] = value->value();
        connect(value, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, key](double v) { parameters_[key] = v; });
        return value;
    };
    const auto vector = [content, number](const QString &key, bool direction) {
        auto *row = new QWidget(content);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        for (int axis = 0; axis < 3; ++axis) {
            auto *value = number(row, key + QString::number(axis), direction && axis == 1 ? 1.0 : 0.0);
            value->setAccessibleName(key + QString(QChar('X' + axis)));
            rowLayout->addWidget(value);
        }
        return row;
    };
    const auto choice = [this](QWidget *owner, const QString &key, const QStringList &labels, const QStringList &ids) {
        auto *box = new QComboBox(owner);
        box->setObjectName(key);
        for (int i = 0; i < labels.size(); ++i) box->addItem(labels[i], ids[i]);
        box->setCurrentIndex(qMax(0, box->findData(parameters_.value(key, ids.first()))));
        parameters_[key] = box->currentData();
        connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, box, key](int) { parameters_[key] = box->currentData(); });
        return box;
    };
    form->addRow(text(u"重心位置 (m)"), vector("centerOfGravity", false));
    auto *rotation = choice(content, "rotationType", {text(u"无旋转"), text(u"匀速旋转"), text(u"正弦旋转")},
                            {"none", "constant", "sinusoidal"});
    form->addRow(text(u"旋转运动类型"), rotation);
    auto *rotationFields = new QGroupBox(text(u"旋转参数"), columns);
    rotationFields->setObjectName("rotationSettings");
    auto *rotationForm = new QFormLayout(rotationFields);

    rotationForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    rotationForm->addRow(text(u"旋转中心 (m)"), vector("rotationCenter", false));
    rotationForm->addRow(text(u"旋转方向矢量"), vector("rotationAxis", true));
    rotationForm->addRow(text(u"旋转开始时间 (s)"), number(rotationFields, "rotationStartTime", 0, 0));
    auto *duration = new QLineEdit(QString::number(parameters_.value("rotationDuration", 1e99).toDouble(), 'g', 12), rotationFields);
    duration->setObjectName("rotationDuration");
    auto *validator = new QDoubleValidator(0, 1e100, 12, duration);
    validator->setLocale(QLocale::c());
    validator->setNotation(QDoubleValidator::ScientificNotation);
    duration->setValidator(validator);
    rotationForm->addRow(text(u"旋转持续时间 (s)"), duration);
    auto *constant = new QWidget(rotationFields);
    constant->setObjectName("constantRotationSettings");
    auto *constantForm = new QFormLayout(constant);
    constantForm->setContentsMargins(0, 0, 0, 0);
    constantForm->addRow(text(u"旋转速率 (°/s)"), number(constant, "rotationRate", 0));
    rotationForm->addRow(constant);
    auto *sine = new QWidget(rotationFields);
    sine->setObjectName("sinusoidalRotationSettings");
    auto *sineForm = new QFormLayout(sine);
    sineForm->setContentsMargins(0, 0, 0, 0);
    auto *frequencyType = choice(sine, "frequencyType", {text(u"旋转频率"), text(u"减缩频率")}, {"frequency", "reduced"});
    sineForm->addRow(text(u"旋转频率类型"), frequencyType);
    auto *frequency = number(sine, "rotationFrequency", 0, 0);
    auto *reduced = number(sine, "reducedFrequency", 0, 0);
    auto *frequencyLabel = new QLabel(text(u"旋转频率 (Hz)"), sine);
    auto *reducedLabel = new QLabel(text(u"减缩频率 (k=ωl/2v)"), sine);
    sineForm->addRow(frequencyLabel, frequency);
    sineForm->addRow(reducedLabel, reduced);
    sineForm->addRow(text(u"旋转相位 (°)"), number(sine, "rotationPhase", 0));
    sineForm->addRow(text(u"旋转偏移值 (°)"), number(sine, "rotationOffset", 0));
    sineForm->addRow(text(u"旋转振幅 (°)"), number(sine, "rotationAmplitude", 0, 0));
    rotationForm->addRow(sine);
    columns->items->addWidget(rotationFields, 1);
    auto *translation = choice(content, "translationType", {text(u"无平动")}, {"none"});
    form->addRow(text(u"平移运动类型"), translation);
    const auto update = [=] {
        rotationFields->setVisible(rotation->currentData() != "none");
        constant->setVisible(rotation->currentData() == "constant");
        sine->setVisible(rotation->currentData() == "sinusoidal");
        frequency->setVisible(frequencyType->currentData() == "frequency");
        frequencyLabel->setVisible(!frequency->isHidden());
        reduced->setVisible(frequencyType->currentData() == "reduced");
        reducedLabel->setVisible(!reduced->isHidden());
    };
    connect(rotation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
        update();
        motionLayout::fit(this, rotation->currentData() == "none" ? QSize(600, 260) : QSize(1080, 440));
    });
    connect(frequencyType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [update](int) { update(); });
    update();
    auto *error = new QLabel(this);
    error->setWordWrap(true);
    error->setProperty("parameterHint", true);
    error->hide();
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(text(u"确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(text(u"取消"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, duration, error] {
        if (!duration->hasAcceptableInput()) {
            error->setText(text(u"请输入有效的持续时间，支持 1e99 等科学计数法。"));
            error->show(); duration->setFocus(); return;
        }
        if (parameters_.value("rotationType") != "none"
            && parameters_.value("rotationAxis0").toDouble() == 0
            && parameters_.value("rotationAxis1").toDouble() == 0
            && parameters_.value("rotationAxis2").toDouble() == 0) {
            error->setText(text(u"旋转方向矢量不能为零。"));
            error->show(); return;
        }
        parameters_["rotationDuration"] = duration->text().toDouble();
        accept();
    });
    layout->addWidget(buttons);
    motionLayout::fit(this, rotation->currentData() == "none" ? QSize(600, 260) : QSize(1080, 440));
}
}
