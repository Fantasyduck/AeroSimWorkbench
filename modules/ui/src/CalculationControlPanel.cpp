#include "aerosim/ui/RadioChoice.h"
#include "aerosim/ui/CalculationControlPanel.h"
#include "CalculationControlBinding.h"

#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QLocale>
#include <QSpinBox>
#include <QVBoxLayout>

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}

class ResidualSpinBox final : public QDoubleSpinBox {
public:
    explicit ResidualSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {}

protected:
    QString textFromValue(double value) const override
    {
        return locale().toString(value, 'g', QLocale::FloatingPointShortest);
    }
};
}

CalculationControlPanel::CalculationControlPanel(const QVariantMap &draft, QWidget *parent)
    : QWidget(parent), draft_(draft)
{
    setObjectName(QStringLiteral("calculationControlPanel"));
    setProperty("parameterPanel", true);
    setAttribute(Qt::WA_StyledBackground, true);
    QVariantMap defaults = calculationControlDraft(defaultCalculationControl());
    defaults["gpuIndex"] = QString();
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
        if (!draft_.contains(it.key())) draft_.insert(it.key(), it.value());

    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    const auto section = [this, layout](const QString &title, int column) {
        auto *group = new QGroupBox(title, this);
        auto *form = new QFormLayout(group);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        form->setFormAlignment(Qt::AlignTop);
        form->setHorizontalSpacing(18);
        form->setVerticalSpacing(8);
        layout->addWidget(group, 0, column);
        return form;
    };
    const auto addRow = [](QFormLayout *form, const QString &text, QWidget *editor) {
        auto *label = new QLabel(text, form->parentWidget());
        label->setMinimumWidth(120);
        label->setBuddy(editor);
        form->addRow(label, editor);
    };
    const auto integer = [this](QWidget *owner, const QString &key, int minimum, int maximum) {
        auto *editor = new QSpinBox(owner);
        editor->setObjectName(key);
        editor->setRange(minimum, maximum);
        editor->setValue(draft_.value(key).toInt());
        editor->setKeyboardTracking(false);
        draft_[key] = editor->value();
        connect(editor, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, key](int value) {
            draft_[key] = value;
            emit draftChanged(draft_);
        });
        return editor;
    };
    auto *iterations = section(uiText(u"迭代控制"), 0);
    addRow(iterations, uiText(u"一阶迭代步数"), integer(iterations->parentWidget(), "firstOrderIterations", 0, 1000000000));
    addRow(iterations, uiText(u"最大迭代步数"), integer(iterations->parentWidget(), "maximumIterations", 1, 1000000000));

    auto *convergence = section(uiText(u"收敛准则"), 1);
    const auto residual = [this, convergence, addRow](const QString &label, const QString &key, double minimum) {
        auto *editor = new ResidualSpinBox(convergence->parentWidget());
        editor->setObjectName(key);
        editor->setDecimals(17);
        editor->setRange(minimum, 1e9);
        editor->setValue(draft_.value(key).toDouble());
        editor->setKeyboardTracking(false);
        connect(editor, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, key](double value) {
            draft_[key] = value;
            emit draftChanged(draft_);
        });
        addRow(convergence, label, editor);
    };
    residual(uiText(u"收敛残差"), "convergenceResidual", -1e9);
    residual(uiText(u"残差监控阈值"), "residualMonitorThreshold", 0.0);

    auto *resources = new QGroupBox(uiText(u"计算资源"), this);
    resources->setObjectName(QStringLiteral("calculationResourcesGroup"));
    auto *resourceLayout = new QGridLayout(resources);
    resourceLayout->setHorizontalSpacing(24);
    resourceLayout->setVerticalSpacing(8);
    resourceLayout->setColumnStretch(0, 1);
    resourceLayout->setColumnStretch(1, 1);
    layout->addWidget(resources, 1, 0, 1, 2);
    auto *gpu = new RadioChoice(resources);
    gpu->setObjectName(QStringLiteral("useGpu"));
    gpu->addItem(uiText(u"不使用GPU计算"), false);
    gpu->addItem(uiText(u"使用GPU计算"), true);
    gpu->setCurrentIndex(draft_.value("useGpu").toBool() ? 1 : 0);
    auto *gpuLabel = new QLabel(uiText(u"GPU 计算"), resources);
    gpuLabel->setBuddy(gpu);
    resourceLayout->addWidget(gpuLabel, 0, 0);
    resourceLayout->addWidget(gpu, 1, 0);

    auto *processes = integer(resources, "processCount", 1, 65536);
    auto *processLabel = new QLabel(uiText(u"计算进程数"), resources);
    processLabel->setBuddy(processes);
    resourceLayout->addWidget(processLabel, 0, 1);
    resourceLayout->addWidget(processes, 1, 1);

    auto *gpuSettings = new QGroupBox(uiText(u"GPU 计算设置"), resources);
    gpuSettings->setObjectName(QStringLiteral("gpuSettingsGroup"));
    auto *gpuForm = new QFormLayout(gpuSettings);
    gpuForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    gpuForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    gpuForm->setHorizontalSpacing(18);
    gpuForm->setVerticalSpacing(8);
    addRow(gpuForm, uiText(u"GPU 卡数"), integer(gpuSettings, "gpuCount", 1, 65536));
    auto *gpuIndex = new QComboBox(gpuSettings);
    gpuIndex->setObjectName(QStringLiteral("gpuIndex"));
    gpuIndex->setEditable(true);
    gpuIndex->setInsertPolicy(QComboBox::NoInsert);
    gpuIndex->setCurrentText(draft_.value("gpuIndex").toString());
    gpuIndex->lineEdit()->setPlaceholderText(uiText(u"填写 GPU 编号"));
    gpuIndex->setToolTip(uiText(u"当前未接入设备枚举，请填写指定 GPU 编号；文件编码待确认。"));
    auto *gpuHint = new QLabel(uiText(u"指定 GPU 编码待确认，此项暂不读取或写入 XCF。"), gpuSettings);
    gpuHint->setWordWrap(true);
    gpuHint->setProperty("parameterHint", true);
    addRow(gpuForm, uiText(u"指定计算 GPU"), gpuIndex);
    gpuForm->addRow(gpuHint);
    connect(gpuIndex, &QComboBox::currentTextChanged, this, [this](const QString &value) {
        draft_["gpuIndex"] = value;
        emit draftChanged(draft_);
    });
    resourceLayout->addWidget(gpuSettings, 2, 0, 1, 2);
    gpuSettings->setVisible(draft_.value("useGpu").toBool());
    connect(gpu, &RadioChoice::currentIndexChanged, this, [this, gpu, gpuSettings](int) {
        draft_["useGpu"] = gpu->currentData();
        gpuSettings->setVisible(gpu->currentData().toBool());
        emit draftChanged(draft_);
    });
}

} // namespace aerosim
