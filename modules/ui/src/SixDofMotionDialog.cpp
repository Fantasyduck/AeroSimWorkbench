#include "aerosim/ui/SixDofMotionDialog.h"
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
    explicit Number(QWidget *parent) : QDoubleSpinBox(parent) {
        setDecimals(6); setRange(-1e12, 1e12);
    }
    QString textFromValue(double value) const override {
        auto result = QDoubleSpinBox::textFromValue(value);
        const int point = result.indexOf(locale().decimalPoint());
        if (point >= 0) while (result.size() > point + 2 && result.endsWith(locale().zeroDigit())) result.chop(1);
        return result;
    }
};

// Each editor owns a copy; switching entries or reducing counts retains inactive data.
class LoadEditor final : public QGroupBox {
public:
    LoadEditor(bool moment, const QVariantList &entries, int activeCount, QWidget *parent)
        : QGroupBox(moment ? text(u"力矩渐变设置") : text(u"外力渐变设置"), parent), entries_(entries), moment_(moment) {
        const QString prefix = moment ? "externalMoment" : "externalForce";
        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        layout->addLayout(form);
        current_ = new QComboBox(this); current_->setObjectName(prefix + "Current");
        frame_ = new QComboBox(this); frame_->setObjectName(prefix + "Frame");
        frame_->addItem(text(u"惯性系"), "inertial"); frame_->addItem(text(u"随体系"), "body");
        points_ = new QSpinBox(this); points_->setObjectName(prefix + "PointCount"); points_->setRange(1, 1000);
        form->addRow(moment ? text(u"当前力矩") : text(u"当前外力"), current_);
        form->addRow(moment ? text(u"当前力矩参考系") : text(u"当前外力参考系"), frame_);
        form->addRow(moment ? text(u"力矩渐变数") : text(u"外力渐变数"), points_);
        table_ = new QTableWidget(this); table_->setObjectName(prefix + "Table");
        table_->setColumnCount(moment ? 4 : 7);
        table_->setHorizontalHeaderLabels(moment
            ? QStringList{"Mx (N·m)", "My (N·m)", "Mz (N·m)", text(u"时刻 (s)")}
            : QStringList{"Fx (N)", "Fy (N)", "Fz (N)", "Px (m)", "Py (m)", "Pz (m)", text(u"时刻 (s)")});
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table_->verticalHeader()->setDefaultSectionSize(36);
        table_->setMinimumHeight(130); table_->setMaximumHeight(200);
        layout->addWidget(table_);
        auto *save = new QPushButton(text(u"保存"), this); save->setObjectName(prefix + "Save");
        layout->addWidget(save, 0, Qt::AlignRight);
        auto *feedback = new QLabel(this); feedback->setProperty("parameterHint", true);
        feedback->setWordWrap(true); layout->addWidget(feedback);
        connect(save, &QPushButton::clicked, this, [=] {
            store(); feedback->setText(text(u"已保存到窗口草稿，点击确定后提交。"));
        });
        connect(current_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) { store(); load(index); });
        connect(points_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
            store(); if (selected_ < 0) return;
            auto entry = entries_[selected_].toMap(); entry["pointCount"] = value;
            entries_[selected_] = entry; load(selected_);
        });
        resizeEntries(activeCount);
    }
    void resizeEntries(int count) {
        store();
        while (entries_.size() < count) {
            QVariantList zero;
            for (int c = 0; c < table_->columnCount(); ++c) zero.append(0.);
            entries_.append(QVariantMap{{"frame", "inertial"}, {"pointCount", 1}, {"points", QVariantList{QVariant(zero)}}});
        }
        const QSignalBlocker block(current_);
        const int previous = selected_;
        current_->clear();
        for (int i = 0; i < count; ++i) current_->addItem((moment_ ? text(u"力矩%1") : text(u"外力%1")).arg(i + 1));
        const int index = count ? qMin(qMax(0, previous), count - 1) : -1;
        current_->setCurrentIndex(index); load(index); setVisible(count > 0);
    }
    QVariantList entries() { store(); return entries_; }
private:
    void store() {
        if (selected_ < 0) return;
        auto entry = entries_[selected_].toMap(); auto rows = entry.value("points").toList();
        while (rows.size() < table_->rowCount()) rows.append(QVariant(QVariantList{}));
        for (int r = 0; r < table_->rowCount(); ++r) {
            QVariantList values;
            for (int c = 0; c < table_->columnCount(); ++c)
                values.append(qobject_cast<QDoubleSpinBox *>(table_->cellWidget(r, c))->value());
            rows[r] = values;
        }
        entry["points"] = rows; entry["frame"] = frame_->currentData(); entries_[selected_] = entry;
    }
    void load(int index) {
        selected_ = index;
        if (index < 0) { table_->setRowCount(0); return; }
        const auto entry = entries_[index].toMap(); const QSignalBlocker block(points_);
        points_->setValue(entry.value("pointCount", 1).toInt());
        frame_->setCurrentIndex(qMax(0, frame_->findData(entry.value("frame", "inertial"))));
        table_->setRowCount(0); table_->setRowCount(points_->value());
        const auto rows = entry.value("points").toList();
        for (int r = 0; r < table_->rowCount(); ++r) {
            const auto values = rows.value(r).toList();
            for (int c = 0; c < table_->columnCount(); ++c) {
                auto *value = new Number(table_);
                if (c == table_->columnCount() - 1) value->setMinimum(0);
                value->setValue(values.value(c, 0.).toDouble()); table_->setCellWidget(r, c, value);
            }
        }
    }
    QVariantList entries_;
    bool moment_;
    int selected_ = -1;
    QComboBox *current_ = nullptr;
    QComboBox *frame_ = nullptr;
    QSpinBox *points_ = nullptr;
    QTableWidget *table_ = nullptr;
};
}

SixDofMotionDialog::SixDofMotionDialog(const QVariantMap &parameters, QWidget *parent, Mode mode)
    : QDialog(parent), parameters_(parameters) {
    const bool release = mode == Mode::RotationRelease;
    setObjectName(release ? "rotationReleaseMotionDialog" : "sixDofMotionDialog");
    setWindowTitle(release ? text(u"旋转抛置参数设置") : text(u"六自由度运动参数设置"));
    setProperty("parameterEditor", true); setModal(true); resize(900, 780);
    setStyleSheet(QStringLiteral("QAbstractSpinBox, QComboBox { min-height: 22px; max-height: 22px; padding: 2px 6px; }"));
    auto *layout = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this); scroll->setObjectName("sixDofScroll");
    scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll); content->setProperty("parameterSurface", true);
    auto *body = new QVBoxLayout(content); auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *rigidBody = release ? new QGroupBox(text(u"运动体参数"), content) : nullptr;
    QVBoxLayout *loadLayout = body;
    if (rigidBody) {
        loadLayout = new QVBoxLayout(rigidBody); loadLayout->addLayout(form); body->addWidget(rigidBody);
    } else body->addLayout(form);
    scroll->setWidget(content); layout->addWidget(scroll);
    const auto number = [this](QWidget *owner, const QString &key, double fallback, double minimum = -1e12) {
        auto *value = new Number(owner); value->setObjectName(key); value->setMinimum(minimum);
        value->setMinimumWidth(85); value->setValue(parameters_.value(key, fallback).toDouble());
        parameters_[key] = value->value();
        connect(value, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, key](double v) { parameters_[key] = v; });
        return value;
    };
    const auto vector = [=](const QString &key, bool gravity = false, bool unitX = false) {
        auto *row = new QWidget(content); auto *items = new QHBoxLayout(row); items->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < 3; ++i) {
            auto *value = number(row, key + QString::number(i), gravity && i == 2 ? -1. : (unitX && i == 0 ? 1. : 0.));
            value->setAccessibleName(key + QString(QChar('X' + i))); items->addWidget(value);
        }
        return row;
    };
    if (release) {
        auto *axisGroup = new QGroupBox(text(u"旋转抛置参数"), content);
        auto *axisForm = new QFormLayout(axisGroup); body->insertWidget(0, axisGroup);
        auto *definition = new QComboBox(axisGroup); definition->setObjectName("axisDefinition");
        definition->addItem(text(u"方向定义"), "direction"); definition->addItem(text(u"两点定义"), "two-points");
        definition->setCurrentIndex(parameters_.value("axisDefinition") == "two-points" ? 1 : 0);
        parameters_["axisDefinition"] = definition->currentData();
        axisForm->addRow(text(u"转动轴定义方式"), definition);
        axisForm->addRow(text(u"转动轴坐标点1 (m)"), vector("axisPoint1"));
        auto *axis = vector("axisDirection", false, true);
        auto *point2 = vector("axisPoint2", false, true);
        auto *axisLabel = new QLabel(text(u"转动轴方向"), axisGroup);
        auto *pointLabel = new QLabel(text(u"转动轴坐标点2 (m)"), axisGroup);
        axisForm->addRow(axisLabel, axis); axisForm->addRow(pointLabel, point2);
        const auto updateAxis = [=] {
            const bool direction = definition->currentIndex() == 0;
            axis->setVisible(direction); axisLabel->setVisible(direction);
            point2->setVisible(!direction); pointLabel->setVisible(!direction);
            parameters_["axisDefinition"] = definition->currentData();
        };
        connect(definition, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int) { updateAxis(); });
        updateAxis();
        axisForm->addRow(text(u"转动惯量 (kg·m²)"), number(axisGroup, "momentOfInertia", 1., 0.000001));
        axisForm->addRow(text(u"最大旋转角 (°)"), number(axisGroup, "maximumRotationAngle", 0., 0.));
        auto *ignore = new QCheckBox(text(u"忽略气动力矩"), axisGroup); ignore->setObjectName("ignoreAerodynamicMoment");
        ignore->setChecked(parameters_.value("ignoreAerodynamicMoment", false).toBool());
        parameters_["ignoreAerodynamicMoment"] = ignore->isChecked();
        connect(ignore, &QCheckBox::toggled, this, [this](bool v) { parameters_["ignoreAerodynamicMoment"] = v; });
        axisForm->addRow(ignore);
        for (int row = 0; row < axisForm->rowCount(); ++row) {
            auto *labelItem = axisForm->itemAt(row, QFormLayout::LabelRole);
            if (labelItem && labelItem->widget()) labelItem->widget()->setMinimumWidth(175);
        }
    }
    form->addRow(text(u"运动体质量 (kg)"), number(content, "mass", 1., 0.000001));
    form->addRow(text(u"重心位置 (m)"), vector("centerOfGravity"));
    for (const auto &component : QStringList{"xx", "yy", "zz", "xy", "xz", "yz"})
        form->addRow(text(u"惯性矩 I%1 (kg·m²)").arg(component),
                     number(content, "inertia" + component, component[0] == component[1] ? 1. : 0.,
                            component[0] == component[1] ? 0.000001 : -1e12));
    if (!release) {
        form->addRow(text(u"初始线速度 (m/s)"), vector("initialVelocity"));
        form->addRow(text(u"初始角速度 (°/s)"), vector("initialAngularVelocity"));
        form->addRow(text(u"初始姿态角 (°)"), vector("initialAttitude"));
    }
    for (const auto &key : QStringList{"ignoreTranslation", "ignoreRotation"}) {
        auto *row = new QWidget(content); auto *items = new QHBoxLayout(row); items->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < 3; ++i) {
            const auto field = key + QString::number(i);
            auto *check = new QCheckBox(QString(QChar('x' + i)), row); check->setObjectName(field);
            check->setChecked(parameters_.value(field, false).toBool()); parameters_[field] = check->isChecked();
            connect(check, &QCheckBox::toggled, this, [this, field](bool v) { parameters_[field] = v; }); items->addWidget(check);
        }
        form->addRow(key == "ignoreTranslation" ? text(u"忽略平动方向") : text(u"忽略转动方向"), row);
    }
    form->addRow(text(u"重力方向矢量"), vector("gravityDirection", true));
    form->addRow(text(u"重力大小 (m/s²)"), number(content, "gravityMagnitude", 9.8, 0));
    for (int row = 0; row < form->rowCount(); ++row) {
        auto *labelItem = form->itemAt(row, QFormLayout::LabelRole);
        if (labelItem && labelItem->widget()) labelItem->widget()->setMinimumWidth(175);
    }
    LoadEditor *forces = nullptr; LoadEditor *moments = nullptr;
    for (bool moment : {false, true}) {
        const QString key = moment ? "momentCount" : "forceCount";
        auto *count = new QSpinBox(content); count->setObjectName(moment ? "externalMomentCount" : "externalForceCount");
        count->setRange(0, 100); count->setValue(parameters_.value(key, release ? 0 : 1).toInt()); parameters_[key] = count->value();
        auto *countForm = new QFormLayout;
        auto *countLabel = new QLabel(moment ? text(u"外力矩数目") : text(u"外力数目"), content);
        countLabel->setMinimumWidth(175);
        countForm->addRow(countLabel, count); loadLayout->addLayout(countForm);
        auto *editor = new LoadEditor(moment, parameters_.value(moment ? "moments" : "forces").toList(), count->value(), content);
        loadLayout->addWidget(editor);
        if (moment) moments = editor; else forces = editor;
        connect(count, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
            editor->resizeEntries(value); parameters_[key] = value;
        });
    }
    body->addStretch();
    auto *error = new QLabel(this); error->setObjectName("motionValidationError");
    error->setWordWrap(true); error->hide(); layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(text(u"确定")); buttons->button(QDialogButtonBox::Cancel)->setText(text(u"取消"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [=] {
        if (release) {
            bool nonzero = false;
            for (int i = 0; i < 3; ++i) {
                const QString suffix = QString::number(i);
                const double component = parameters_.value("axisDefinition") == "direction"
                    ? parameters_.value("axisDirection" + suffix).toDouble()
                    : parameters_.value("axisPoint2" + suffix).toDouble() - parameters_.value("axisPoint1" + suffix).toDouble();
                nonzero = nonzero || component != 0.;
            }
            if (!nonzero) { error->setText(text(u"转动轴方向不能为零，两点定义时两点不能重合。")); error->show(); return; }
        }
        parameters_["forces"] = forces->entries(); parameters_["moments"] = moments->entries(); accept();
    });
    layout->addWidget(buttons);
}
}
