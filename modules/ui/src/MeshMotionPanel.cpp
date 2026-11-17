#include "aerosim/ui/MeshMotionPanel.h"
#include "aerosim/ui/ForcedMotionDialog.h"
#include "aerosim/ui/FixedAxisMotionDialog.h"
#include "aerosim/ui/SixDofMotionDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}
}

MeshMotionPanel::MeshMotionPanel(const QVariantMap &draft, QWidget *parent)
    : QWidget(parent), draft_(draft), bodies_(draft.value("bodies").toList())
{
    setObjectName(QStringLiteral("meshMotionPanel"));
    setProperty("parameterPanel", true);
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    enabled_ = new QCheckBox(uiText(u"是否启动动网格"), this);
    enabled_->setObjectName(QStringLiteral("meshMotionEnabled"));
    enabled_->setChecked(draft.value("enabled", false).toBool());
    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(16);
    toolbar->addWidget(enabled_);
    toolbar->addStretch();
    auto *countForm = new QFormLayout;
    count_ = new QSpinBox(this);
    count_->setObjectName(QStringLiteral("motionBodyCount"));
    count_->setRange(0, 100);
    count_->setValue(draft.value("bodyCount", 0).toInt());
    count_->setMaximumWidth(220);
    count_->setEnabled(enabled_->isChecked());
    countForm->addRow(uiText(u"运动体数目"), count_);
    toolbar->addLayout(countForm);
    layout->addLayout(toolbar);
    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("motionBodyTable"));
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({uiText(u"运动体名称"), uiText(u"指定运动体"), uiText(u"运动类型"), uiText(u"参数设置")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setMinimumSectionSize(120);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(42);
    table_->setMinimumHeight(200);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setEnabled(enabled_->isChecked());
    layout->addWidget(table_);
    auto *hint = new QLabel(uiText(u"运动体和参数为本地配置草稿；指定运动体填写网格或对象名称，真实对象绑定与动网格求解尚未接入。"), this);
    hint->setWordWrap(true);
    hint->setProperty("parameterHint", true);
    layout->addWidget(hint);
    // Populate without emitting a user edit during page construction.
    while (bodies_.size() < count_->value()) {
        const int index = bodies_.size() + 1;
        bodies_.append(QVariantMap{{"name", QStringLiteral("motion_body_%1").arg(index)}, {"target", ""},
            {"type", "forced"}});
    }
    rebuildTable();
    connect(enabled_, &QCheckBox::toggled, this, [this](bool enabled) {
        count_->setEnabled(enabled);
        table_->setEnabled(enabled);
        publish();
    });
    connect(count_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MeshMotionPanel::resizeBodies);
}

void MeshMotionPanel::resizeBodies(int count)
{
    // Keep inactive bodies so reducing and restoring the count does not lose edits.
    while (bodies_.size() < count) {
        bodies_.append(QVariantMap{{"name", QStringLiteral("motion_body_%1").arg(bodies_.size() + 1)}, {"target", ""},
            {"type", "forced"}});
    }
    rebuildTable();
    publish();
}

void MeshMotionPanel::rebuildTable()
{
    const QSignalBlocker blocker(table_);
    table_->setRowCount(0);
    table_->setRowCount(count_->value());
    for (int row = 0; row < count_->value(); ++row) {
        const auto body = bodies_.at(row).toMap();
        for (int column = 0; column < 2; ++column) {
            const QString key = column == 0 ? "name" : "target";
            auto *edit = new QLineEdit(body.value(key).toString(), table_);
            edit->setObjectName(QStringLiteral("motionBody%1_%2").arg(row).arg(key));
            edit->setMaxLength(128);
            if (column == 1) edit->setPlaceholderText(uiText(u"填写网格 / 对象名称"));
            table_->setCellWidget(row, column, edit);
            connect(edit, &QLineEdit::textChanged, this, [this, row, key](const QString &value) { updateBody(row, key, value); });
        }
        auto *type = new QComboBox(table_);
        type->setObjectName(QStringLiteral("motionBody%1_type").arg(row));
        type->addItem(uiText(u"强迫运动"), QStringLiteral("forced"));
        type->addItem(uiText(u"定轴转动"), QStringLiteral("fixed-axis"));
        type->addItem(uiText(u"六自由度运动"), QStringLiteral("six-dof"));
        type->addItem(uiText(u"旋转抛置"), QStringLiteral("rotation-release"));
        const int typeIndex = type->findData(body.value("type"));
        type->setCurrentIndex(qMax(0, typeIndex));
        if (typeIndex < 0) {
            auto updatedBody = body;
            updatedBody["type"] = type->currentData();
            bodies_[row] = updatedBody;
        }
        table_->setCellWidget(row, 2, type);
        connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, row, type] {
            updateBody(row, "type", type->currentData());
        });
        auto *settings = new QPushButton(uiText(u"设置参数"), table_);
        settings->setObjectName(QStringLiteral("motionBody%1_settings").arg(row));
        table_->setCellWidget(row, 3, settings);
        connect(settings, &QPushButton::clicked, this, [this, row] {
            table_->setCurrentCell(row, 3);
            if (bodies_.at(row).toMap().value("type") == "forced") {
                ForcedMotionDialog dialog(bodies_.at(row).toMap().value("forcedMotion").toMap(), this);
                if (dialog.exec() == QDialog::Accepted) updateBody(row, "forcedMotion", dialog.parameters());
            } else if (bodies_.at(row).toMap().value("type") == "fixed-axis") {
                FixedAxisMotionDialog dialog(bodies_.at(row).toMap().value("fixedAxisMotion").toMap(), this);
                if (dialog.exec() == QDialog::Accepted) updateBody(row, "fixedAxisMotion", dialog.parameters());
            } else if (bodies_.at(row).toMap().value("type") == "six-dof") {
                SixDofMotionDialog dialog(bodies_.at(row).toMap().value("sixDofMotion").toMap(), this);
                if (dialog.exec() == QDialog::Accepted) updateBody(row, "sixDofMotion", dialog.parameters());
            } else if (bodies_.at(row).toMap().value("type") == "rotation-release") {
                SixDofMotionDialog dialog(bodies_.at(row).toMap().value("rotationReleaseMotion").toMap(), this,
                                          SixDofMotionDialog::Mode::RotationRelease);
                if (dialog.exec() == QDialog::Accepted) updateBody(row, "rotationReleaseMotion", dialog.parameters());
            }
        });
    }
}

void MeshMotionPanel::updateBody(int row, const QString &key, const QVariant &value)
{
    auto body = bodies_.at(row).toMap();
    body.insert(key, value);
    bodies_[row] = body;
    publish();
}

void MeshMotionPanel::publish()
{
    draft_.insert("enabled", enabled_->isChecked());
    draft_.insert("bodyCount", count_->value());
    draft_.insert("bodies", bodies_);
    emit draftChanged(draft_);
}

} // namespace aerosim
