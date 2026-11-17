#include "aerosim/ui/CalculationTaskInputDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <cmath>
#include <cstddef>

namespace aerosim {

template <std::size_t N>
QString uiText(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}

QString calculationInputTitle(CalculationInputKind kind)
{
    switch (kind) {
    case CalculationInputKind::ReynoldsTemperature: return uiText(u"\u5355\u4f4d\u96f7\u8bfa\u6570\u548c\u9759\u6e29");
    case CalculationInputKind::PressureTemperature: return uiText(u"\u9759\u538b\u548c\u9759\u6e29");
    case CalculationInputKind::TotalPressureTemperature: return uiText(u"\u603b\u538b\u548c\u603b\u6e29");
    case CalculationInputKind::Altitude: return uiText(u"\u9ad8\u5ea6");
    case CalculationInputKind::Trajectory: return uiText(u"\u5f39\u9053");
    }
    return {};
}

QStringList calculationInputLabels(CalculationInputKind kind)
{
    QStringList labels;
    switch (kind) {
    case CalculationInputKind::ReynoldsTemperature:
        labels << uiText(u"\u5355\u4f4d\u96f7\u8bfa\u6570 (1/m)") << uiText(u"\u9759\u6e29 (K)"); break;
    case CalculationInputKind::PressureTemperature:
        labels << uiText(u"\u9759\u538b (Pa)") << uiText(u"\u9759\u6e29 (K)"); break;
    case CalculationInputKind::TotalPressureTemperature:
        labels << uiText(u"\u603b\u538b (Pa)") << uiText(u"\u603b\u6e29 (K)"); break;
    case CalculationInputKind::Altitude:
        labels << uiText(u"\u9ad8\u5ea6 (m)"); break;
    case CalculationInputKind::Trajectory:
        labels << uiText(u"\u65f6\u95f4 (s)") << uiText(u"\u9ad8\u5ea6 (m)"); break;
    }
    return labels << uiText(u"\u9a6c\u8d6b\u6570") << uiText(u"\u653b\u89d2 (\u00b0)") << uiText(u"\u4fa7\u6ed1\u89d2 (\u00b0)");
}

CalculationTaskInputDialog::CalculationTaskInputDialog(CalculationInputKind kind, QWidget *parent)
    : QDialog(parent), kind_(kind)
{
    setObjectName(QStringLiteral("calculationTaskInputDialog"));
    setWindowTitle(uiText(u"\u591a\u72b6\u6001\u8f93\u5165 \u00b7 ") + calculationInputTitle(kind));
    resize(660, 300);
    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(calculationInputTitle(kind), this);
    QFont font = heading->font(); font.setBold(true); heading->setFont(font);
    layout->addWidget(heading);
    auto *body = new QHBoxLayout;
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    const auto labels = calculationInputLabels(kind);
    for (int i = 0; i < labels.size(); ++i) {
        auto *edit = new QLineEdit(this);
        edit->setObjectName(QStringLiteral("taskInput_%1").arg(i));
        edit->setMinimumWidth(180);
        edit->setPlaceholderText(uiText(u"\u8f93\u5165\u6570\u503c"));
        edit->setAccessibleName(labels[i]);
        form->addRow(labels[i], edit);
        inputs_.append(edit);
    }
    body->addLayout(form, 3);
    auto *hint = new QLabel(uiText(u"\u6570\u503c\u7528\u82f1\u6587\u9017\u53f7\u6216\u7a7a\u683c\u5206\u9694\u3002\n\n\u5404\u680f\u6570\u91cf\u987b\u4e00\u81f4\uff0c\u6309\u4f4d\u7f6e\u4e00\u4e00\u5bf9\u5e94\u751f\u6210\u4efb\u52a1\uff1b\u4e0d\u505a\u6392\u5217\u7ec4\u5408\u3002\n\n\u9ad8\u5ea6\u3001\u65f6\u95f4\u3001\u9a6c\u8d6b\u6570\u4e0d\u80fd\u4e3a\u8d1f\uff1b\u538b\u529b\u3001\u6e29\u5ea6\u3001\u5355\u4f4d\u96f7\u8bfa\u6570\u987b\u5927\u4e8e\u96f6\u3002"), this);
    hint->setWordWrap(true);
    hint->setMinimumWidth(195);
    body->addWidget(hint, 2);
    layout->addLayout(body);
    if (kind == CalculationInputKind::Trajectory) {
        auto *note = new QLabel(uiText(u"\u5f39\u9053\u6309\u65f6\u95f4\u70b9\u5f55\u5165\uff1b\u5f53\u524d\u53ea\u4fdd\u5b58\u754c\u9762\u8f93\u5165\uff0c\u4e0d\u8bfb\u53d6\u6216\u63d2\u503c\u5f39\u9053\u6587\u4ef6\u3002"), this);
        note->setWordWrap(true);
        layout->addWidget(note);
    }
    error_ = new QLabel(this);
    error_->setObjectName(QStringLiteral("taskInputError"));
    error_->setWordWrap(true);
    error_->setStyleSheet(QStringLiteral("color: #ba3434;"));
    layout->addWidget(error_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(uiText(u"\u786e\u5b9a"));
    buttons->button(QDialogButtonBox::Cancel)->setText(uiText(u"\u53d6\u6d88"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &CalculationTaskInputDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CalculationTaskInputDialog::setValues(const QVector<double> &values)
{
    editing_ = true;
    for (int i = 0; i < inputs_.size() && i < values.size(); ++i)
        inputs_[i]->setText(QString::number(values[i], 'g', 15));
}

void CalculationTaskInputDialog::validateAndAccept()
{
    QVector<QVector<double>> columns;
    const auto labels = calculationInputLabels(kind_);
    int count = -1;
    for (int col = 0; col < inputs_.size(); ++col) {
        const QString raw = inputs_[col]->text().trimmed();
        // Keep empty comma-separated fields visible as an error ("1,,2").
        if (raw.isEmpty() || raw.startsWith(',') || raw.endsWith(',')
            || raw.contains(QRegularExpression(QStringLiteral(",\\s*,")))) {
            error_->setText(labels[col] + uiText(u"\uff1a\u8bf7\u586b\u5199\u5b8c\u6574\u7684\u6570\u503c\u3002"));
            inputs_[col]->setFocus(); return;
        }
        const auto tokens = raw.split(QRegularExpression(QStringLiteral("[,\\s]+")), QString::SkipEmptyParts);
        if (tokens.size() > 1000 || (editing_ && tokens.size() != 1)) {
            error_->setText(editing_ ? uiText(u"\u7f16\u8f91\u4efb\u52a1\u65f6\uff0c\u6bcf\u680f\u53ea\u586b\u4e00\u4e2a\u6570\u503c\u3002") : uiText(u"\u6bcf\u6b21\u6700\u591a\u6dfb\u52a0 1000 \u4e2a\u4efb\u52a1\u3002"));
            return;
        }
        if (count >= 0 && tokens.size() != count) {
            error_->setText(uiText(u"\u5404\u680f\u6570\u503c\u6570\u91cf\u4e0d\u4e00\u81f4\uff0c\u8bf7\u6309\u4efb\u52a1\u4e00\u4e00\u5bf9\u5e94\u586b\u5199\u3002")); return;
        }
        count = tokens.size();
        QVector<double> values;
        for (const auto &token : tokens) {
            bool ok = false;
            const double value = QLocale::c().toDouble(token, &ok);
            const int machColumn = inputs_.size() - 3;
            const bool nonNegative = col == machColumn
                || ((kind_ == CalculationInputKind::Altitude || kind_ == CalculationInputKind::Trajectory) && col < machColumn);
            const bool positive = col < machColumn && !nonNegative;
            if (!ok || !std::isfinite(value) || (nonNegative && value < 0) || (positive && value <= 0)) {
                error_->setText(labels[col] + uiText(u"\uff1a\u6570\u503c\u65e0\u6548\uff0c\u8bf7\u68c0\u67e5\u683c\u5f0f\u4e0e\u53d6\u503c\u8303\u56f4\u3002"));
                inputs_[col]->setFocus(); return;
            }
            values.append(value);
        }
        columns.append(values);
    }
    rows_.clear();
    for (int row = 0; row < count; ++row) {
        QVector<double> values;
        for (const auto &column : columns) values.append(column[row]);
        rows_.append(values);
    }
    accept();
}

} // namespace aerosim
