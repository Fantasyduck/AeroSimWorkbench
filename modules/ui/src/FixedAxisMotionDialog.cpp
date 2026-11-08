#include "aerosim/ui/FixedAxisMotionDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
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
    explicit Number(QWidget *parent) : QDoubleSpinBox(parent) {}
    QString textFromValue(double value) const override {
        auto result = QDoubleSpinBox::textFromValue(value);
        const int point = result.indexOf(locale().decimalPoint());
        if (point >= 0) while (result.size() > point + 2 && result.endsWith(locale().zeroDigit())) result.chop(1);
        return result;
    }
};
QVariantMap defaultForce() {
    const QVariantList zero{0., 0., 0., 0., 0., 0., 0.};
    return {{"frame", "inertial"}, {"pointCount", 2}, {"points", QVariantList{zero, zero}}};
}
}
FixedAxisMotionDialog::FixedAxisMotionDialog(const QVariantMap &parameters, QWidget *parent)
    : QDialog(parent), parameters_(parameters), forces_(parameters.value("forces").toList())
{
    setObjectName("fixedAxisMotionDialog");
    setWindowTitle(text(u"定轴转动参数设置"));
    setProperty("parameterEditor", true);
    setModal(true);
    resize(880, 740);
    setStyleSheet(QStringLiteral("QAbstractSpinBox, QComboBox { min-height: 22px; max-height: 22px; padding: 2px 6px; }"));
    auto *layout = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    content->setProperty("parameterSurface", true);
    auto *body = new QVBoxLayout(content);
    auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    body->addLayout(form);
    scroll->setWidget(content);
    layout->addWidget(scroll);
    const auto number = [this](QWidget *owner, const QString &key, double fallback, double minimum = -1e9) {
        auto *value = new Number(owner);
        value->setObjectName(key);
        value->setDecimals(6);
        value->setRange(minimum, 1e12);
        value->setValue(parameters_.value(key, fallback).toDouble());
        value->setMinimumWidth(85);
        parameters_[key] = value->value();
        connect(value, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, key](double v) { parameters_[key] = v; });
        return value;
    };
    const auto vector = [content, number](const QString &key, bool unitX) {
        auto *row = new QWidget(content);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        for (int axis = 0; axis < 3; ++axis) {
            auto *value = number(row, key + QString::number(axis), unitX && axis == 0 ? 1.0 : 0.0);
            value->setAccessibleName(key + QString(QChar('X' + axis)));
            rowLayout->addWidget(value);
        }
        return row;
    };
    auto *definition = new QComboBox(content);
    definition->setObjectName("axisDefinition");
    definition->addItem(text(u"方向定义"), "direction");
    definition->addItem(text(u"两点定义"), "two-points");
    definition->setCurrentIndex(parameters_.value("axisDefinition") == "two-points" ? 1 : 0);
    parameters_["axisDefinition"] = definition->currentData();
    form->addRow(text(u"转动轴定义方式"), definition);
    form->addRow(text(u"转动轴坐标点1 (m)"), vector("axisPoint1", false));
    auto *axis = vector("axisDirection", true);
    auto *point2 = vector("axisPoint2", true);
    auto *axisLabel = new QLabel(text(u"转动轴方向"), content);
    auto *pointLabel = new QLabel(text(u"转动轴坐标点2 (m)"), content);
    form->addRow(axisLabel, axis);
    form->addRow(pointLabel, point2);
    const auto updateDefinition = [=] {
        const bool direction = definition->currentIndex() == 0;
        axis->setVisible(direction); axisLabel->setVisible(direction);
        point2->setVisible(!direction); pointLabel->setVisible(!direction);
    };
    connect(definition, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) {
        parameters_["axisDefinition"] = definition->currentData(); updateDefinition();
    });
    updateDefinition();
    form->addRow(text(u"转动惯量 (kg·m²)"), number(content, "momentOfInertia", 1, 0.000001));
    form->addRow(text(u"重心位置 (m)"), vector("centerOfGravity", false));
    auto *ignore = new QCheckBox(text(u"忽略气动力矩"), content);
    ignore->setObjectName("ignoreAerodynamicMoment");
    ignore->setChecked(parameters_.value("ignoreAerodynamicMoment", false).toBool());
    parameters_["ignoreAerodynamicMoment"] = ignore->isChecked();
    connect(ignore, &QCheckBox::toggled, this, [this](bool v) { parameters_["ignoreAerodynamicMoment"] = v; });
    form->addRow(ignore);
    auto *count = new QSpinBox(content);
    count->setObjectName("externalForceCount");
    count->setRange(0, 100);
    count->setValue(parameters_.value("forceCount", 1).toInt());
    form->addRow(text(u"外力数目"), count);
    auto *group = new QGroupBox(text(u"外力渐变设置"), content);
    body->addWidget(group);
    auto *groupLayout = new QVBoxLayout(group);
    auto *forceForm = new QFormLayout;
    groupLayout->addLayout(forceForm);
    force_ = new QComboBox(group); force_->setObjectName("currentExternalForce");
    frame_ = new QComboBox(group); frame_->setObjectName("externalForceFrame");
    frame_->addItem(text(u"惯性系"), "inertial");
    frame_->addItem(text(u"随体系"), "body");
    points_ = new QSpinBox(group); points_->setObjectName("externalForcePointCount");
    points_->setRange(1, 1000);
    forceForm->addRow(text(u"当前外力"), force_);
    forceForm->addRow(text(u"当前外力参考系"), frame_);
    forceForm->addRow(text(u"外力渐变点"), points_);
    table_ = new QTableWidget(group); table_->setObjectName("externalForceTable");
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({"Fx (N)", "Fy (N)", "Fz (N)", "Px (m)", "Py (m)", "Pz (m)", text(u"时刻 (s)")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setMinimumHeight(165);
    groupLayout->addWidget(table_);
    auto *save = new QPushButton(text(u"保存"), group); save->setObjectName("saveExternalForce");
    groupLayout->addWidget(save, 0, Qt::AlignRight);
    auto *feedback = new QLabel(group); feedback->setProperty("parameterHint", true);
    groupLayout->addWidget(feedback);
    connect(save, &QPushButton::clicked, this, [=] { storeForce(); feedback->setText(text(u"当前外力已保存到窗口草稿；确定后提交。")); });
    connect(force_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) { storeForce(); loadForce(index); });
    connect(points_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        storeForce();
        if (selected_ < 0) return;
        auto force = forces_[selected_].toMap(); force["pointCount"] = value;
        forces_[selected_] = force; loadForce(selected_);
    });
    const auto resizeForces = [=](int value) {
        storeForce();
        while (forces_.size() < value) forces_.append(defaultForce());
        const QSignalBlocker block(force_);
        const int previous = selected_;
        force_->clear();
        for (int index = 0; index < value; ++index) force_->addItem(text(u"外力%1").arg(index + 1));
        const int selected = value ? qMin(qMax(0, previous), value - 1) : -1;
        force_->setCurrentIndex(selected); loadForce(selected);
        group->setVisible(value > 0);
        parameters_["forceCount"] = value;
    };
    connect(count, QOverload<int>::of(&QSpinBox::valueChanged), this, resizeForces);
    resizeForces(count->value());
    auto *error = new QLabel(this); error->setWordWrap(true); error->hide(); layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(text(u"确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(text(u"取消"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [=] {
        bool nonzero = false;
        for (int i = 0; i < 3; ++i) {
            const QString suffix = QString::number(i);
            const double component = definition->currentIndex() == 0 ? parameters_.value("axisDirection" + suffix).toDouble()
                : parameters_.value("axisPoint2" + suffix).toDouble() - parameters_.value("axisPoint1" + suffix).toDouble();
            nonzero = nonzero || component != 0;
        }
        if (!nonzero) { error->setText(text(u"转动轴方向不能为零，两点定义时两点不能重合。")); error->show(); return; }
        storeForce(); parameters_["forces"] = forces_; accept();
    });
    layout->addWidget(buttons);
}
void FixedAxisMotionDialog::storeForce()
{
    if (selected_ < 0) return;
    auto force = forces_[selected_].toMap();
    auto rows = force.value("points").toList();
    while (rows.size() < table_->rowCount()) rows.append(QVariant(QVariantList{}));
    for (int r = 0; r < table_->rowCount(); ++r) {
        QVariantList values;
        for (int c = 0; c < 7; ++c) values.append(qobject_cast<QDoubleSpinBox *>(table_->cellWidget(r, c))->value());
        rows[r] = values;
    }
    force["points"] = rows; force["frame"] = frame_->currentData();
    forces_[selected_] = force;
}
void FixedAxisMotionDialog::loadForce(int index)
{
    selected_ = index;
    if (index < 0) { table_->setRowCount(0); return; }
    const auto force = forces_[index].toMap();
    const QSignalBlocker block(points_);
    points_->setValue(force.value("pointCount", 2).toInt());
    frame_->setCurrentIndex(qMax(0, frame_->findData(force.value("frame", "inertial"))));
    table_->setRowCount(0); table_->setRowCount(points_->value());
    const auto rows = force.value("points").toList();
    for (int r = 0; r < table_->rowCount(); ++r) {
        const auto values = rows.value(r).toList();
        for (int c = 0; c < 7; ++c) {
            auto *value = new Number(table_); value->setDecimals(6);
            value->setRange(c == 6 ? 0 : -1e12, 1e12);
            value->setValue(values.value(c, 0.0).toDouble());
            table_->setCellWidget(r, c, value);
        }
    }
}
}
