#include "aerosim/ui/GoverningEquationsPanel.h"

#include <QCheckBox>
#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPair>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVector>

namespace aerosim {
namespace {
template <size_t N> QString uiText(const char16_t (&text)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(text), int(N) - 1);
}
QFormLayout *createForm(QWidget *parent)
{
    auto *form = new QFormLayout(parent);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setVerticalSpacing(8);
    return form;
}
}

GoverningEquationsPanel::GoverningEquationsPanel(const QVariantMap &draft, QWidget *parent)
    : QWidget(parent), draft_(draft)
{
    setObjectName(QStringLiteral("governingEquationsPanel"));
    setProperty("parameterPanel", true);
    setAttribute(Qt::WA_StyledBackground, true);
    const QVariantMap defaults = {{"calculationType", "steady"}, {"equation", "navier-stokes"},
        {"flowRegime", "turbulent"}, {"turbulenceModel", "sa"}, {"rarefiedGas", true},
        {"collisionEquation", "bgk"}, {"collisionSubmodel", "standard-bgk"},
        {"knudsenNumber", 1.0}, {"freestreamParticleCount", 100}, {"gridParticleCount", 2},
        {"collisionModel", "vss"}};
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
        if (!draft_.contains(it.key())) draft_.insert(it.key(), it.value());
    const QString viscousFlow = draft_.value("flowRegime").toString() == "laminar" ? "laminar" : "turbulent";
    draft_.insert("viscousFlowRegime", draft_.value("viscousFlowRegime", viscousFlow));
    draft_.insert("flowRegime", draft_.value("equation") == "euler" ? "inviscid" : draft_.value("viscousFlowRegime"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *basic = new QGroupBox(uiText(u"基础设置"), this);
    auto *basicLayout = new QVBoxLayout(basic);
    basicLayout->setSpacing(10);
    auto *baseFields = new QWidget(basic);
    auto *baseForm = createForm(baseFields);
    baseForm->setContentsMargins(0, 0, 0, 0);
    baseForm->setHorizontalSpacing(24);
    basicLayout->addWidget(baseFields);
    layout->addWidget(basic);

    const auto addChoice = [this](QFormLayout *form, QWidget *owner, const QString &label,
                                 const QString &key, const QVector<QPair<QString, QString>> &choices) {
        auto *options = new QWidget(owner);
        options->setObjectName(key);
        auto *row = new QGridLayout(options);
        row->setContentsMargins(0, 0, 0, 0);
        row->setHorizontalSpacing(16);
        row->setVerticalSpacing(6);
        auto *buttons = new QButtonGroup(options);
        buttons->setExclusive(true);
        QString selected = draft_.value(key).toString();
        bool valid = false;
        for (const auto &choice : choices) valid = valid || choice.second == selected;
        if (!valid) selected = choices.first().second;
        draft_.insert(key, selected);
        const int columns = qMin(3, choices.size());
        int index = 0;
        for (const auto &choice : choices) {
            auto *button = new QRadioButton(choice.first, options);
            button->setObjectName(key + QLatin1Char('_') + choice.second);
            buttons->addButton(button);
            button->setChecked(choice.second == selected);
            row->addWidget(button, index / columns, index % columns, Qt::AlignLeft);
            ++index;
            connect(button, &QRadioButton::toggled, this, [this, key, choice](bool checked) {
                if (checked) updateDraft(key, choice.second);
            });
        }
        row->setColumnStretch(columns, 1);
        form->addRow(label, options);
    };
    addChoice(baseForm, baseFields, uiText(u"计算类型"), "calculationType",
              {{uiText(u"定常计算"), "steady"}, {uiText(u"非定常计算"), "unsteady"}});

    auto *equationChoices = new QWidget(baseFields);
    auto *equationChoicesLayout = new QHBoxLayout(equationChoices);
    equationChoicesLayout->setContentsMargins(0, 0, 0, 0);
    auto *euler = new QRadioButton(uiText(u"欧拉方程"), equationChoices);
    euler->setObjectName(QStringLiteral("eulerEquation"));
    auto *navierStokes = new QRadioButton(uiText(u"N-S 方程"), equationChoices);
    navierStokes->setObjectName(QStringLiteral("navierStokesEquation"));
    euler->setChecked(draft_.value("equation") == "euler");
    navierStokes->setChecked(!euler->isChecked());
    equationChoicesLayout->addWidget(euler);
    equationChoicesLayout->addWidget(navierStokes);
    equationChoicesLayout->addStretch();
    baseForm->addRow(uiText(u"求解方程"), equationChoices);
    connect(euler, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) updateDraft("equation", "euler");
    });
    connect(navierStokes, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) updateDraft("equation", "navier-stokes");
    });

    auto *flow = new QWidget(baseFields);
    auto *flowLayout = new QVBoxLayout(flow);
    flowLayout->setContentsMargins(0, 0, 0, 0);
    viscousFlowChoices_ = new QWidget(flow);
    auto *flowChoices = new QHBoxLayout(viscousFlowChoices_);
    flowChoices->setContentsMargins(0, 0, 0, 0);
    auto *laminar = new QRadioButton(uiText(u"层流"), viscousFlowChoices_);
    laminar->setObjectName(QStringLiteral("laminarFlow"));
    auto *turbulent = new QRadioButton(uiText(u"湍流"), viscousFlowChoices_);
    turbulent->setObjectName(QStringLiteral("turbulentFlow"));
    laminar->setChecked(draft_.value("viscousFlowRegime") == "laminar");
    turbulent->setChecked(!laminar->isChecked());
    flowChoices->addWidget(laminar);
    flowChoices->addWidget(turbulent);
    flowChoices->addStretch();
    flowLayout->addWidget(viscousFlowChoices_);
    inviscidNote_ = new QLabel(uiText(u"欧拉方程采用无粘流。"), flow);
    flowLayout->addWidget(inviscidNote_);
    connect(laminar, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) updateDraft("viscousFlowRegime", "laminar");
    });
    connect(turbulent, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) updateDraft("viscousFlowRegime", "turbulent");
    });
    baseForm->addRow(uiText(u"流态设置"), flow);

    auto *turbulence = new QWidget(basic);
    turbulence->setObjectName(QStringLiteral("turbulenceSettings"));
    turbulenceSettings_ = turbulence;
    auto *turbulenceForm = createForm(turbulence);
    turbulenceForm->setContentsMargins(0, 0, 0, 0);
    turbulenceForm->setHorizontalSpacing(24);
    addChoice(turbulenceForm, turbulence, uiText(u"湍流模型"), "turbulenceModel",
              {{"S-A", "sa"}, {"k_w Menter", "kw-menter"}, {"des", "des"}, {"ddes", "ddes"},
               {"wilcox k_w1998", "wilcox-kw1998"}, {"wilcox k_w2006", "wilcox-kw2006"}});
    basicLayout->addWidget(turbulence);

    auto *rarefied = new QGroupBox(uiText(u"稀薄气体效应"), this);
    auto *rarefiedLayout = new QVBoxLayout(rarefied);
    rarefiedLayout->setSpacing(12);
    auto *enableRarefied = new QCheckBox(uiText(u"考虑稀薄气体效应"), rarefied);
    enableRarefied->setObjectName(QStringLiteral("rarefiedGasEffect"));
    enableRarefied->setChecked(draft_.value("rarefiedGas").toBool());
    rarefiedLayout->addWidget(enableRarefied);
    rarefiedSettings_ = new QWidget(rarefied);
    rarefiedSettings_->setObjectName(QStringLiteral("rarefiedGasSettings"));
    auto *rarefiedColumns = new QGridLayout(rarefiedSettings_);
    rarefiedColumns->setContentsMargins(0, 0, 0, 0);
    rarefiedColumns->setHorizontalSpacing(24);
    rarefiedColumns->setColumnStretch(0, 1);
    rarefiedColumns->setColumnStretch(1, 1);
    auto *collisionInputs = new QWidget(rarefiedSettings_);
    auto *particleInputs = new QWidget(rarefiedSettings_);
    auto *collisionForm = createForm(collisionInputs);
    auto *rarefiedForm = createForm(particleInputs);
    collisionForm->setContentsMargins(0, 0, 0, 0);
    rarefiedForm->setContentsMargins(0, 0, 0, 0);
    rarefiedColumns->addWidget(collisionInputs, 0, 0);
    rarefiedColumns->addWidget(particleInputs, 0, 1);
    addChoice(collisionForm, collisionInputs, uiText(u"碰撞方程"), "collisionEquation", {{uiText(u"BGK 方程"), "bgk"}});
    auto *submodels = new QWidget(collisionInputs);
    auto *submodelLayout = new QHBoxLayout(submodels);
    submodelLayout->setContentsMargins(0, 0, 0, 0);
    auto *standard = new QRadioButton(uiText(u"标准 BGK"), submodels);
    standard->setObjectName(QStringLiteral("standardBgk"));
    auto *es = new QRadioButton(QStringLiteral("ES-BGK"), submodels);
    es->setObjectName(QStringLiteral("esBgk"));
    es->setChecked(draft_.value("collisionSubmodel") == "es-bgk");
    standard->setChecked(!es->isChecked());
    submodelLayout->addWidget(standard);
    submodelLayout->addWidget(es);
    submodelLayout->addStretch();
    collisionForm->addRow(uiText(u"子模型类型"), submodels);
    connect(standard, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) updateDraft("collisionSubmodel", "standard-bgk");
    });
    connect(es, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) updateDraft("collisionSubmodel", "es-bgk");
    });
    auto *knudsen = new QDoubleSpinBox(rarefiedSettings_);
    knudsen->setObjectName(QStringLiteral("knudsenNumber"));
    knudsen->setDecimals(6);
    knudsen->setRange(0.000001, 1e9);
    knudsen->setMaximumWidth(160);
    knudsen->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    knudsen->setValue(draft_.value("knudsenNumber").toDouble());
    draft_.insert("knudsenNumber", knudsen->value());
    rarefiedForm->addRow(uiText(u"克努森数（Kn）"), knudsen);
    connect(knudsen, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { updateDraft("knudsenNumber", value); });
    const auto addParticleCount = [this, rarefiedForm](const QString &key, const QString &label) {
        auto *count = new QSpinBox(rarefiedSettings_);
        count->setObjectName(key);
        count->setRange(1, 1000000000);
        count->setMaximumWidth(160);
        count->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        count->setValue(draft_.value(key).toInt());
        draft_.insert(key, count->value());
        rarefiedForm->addRow(label, count);
        connect(count, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, key](int value) { updateDraft(key, value); });
    };
    addParticleCount("freestreamParticleCount", uiText(u"来流粒子参考数"));
    addParticleCount("gridParticleCount", uiText(u"网格粒子参考数"));
    addChoice(collisionForm, collisionInputs, uiText(u"碰撞模型"), "collisionModel",
              {{"VSS", "vss"}, {"HS", "hs"}, {"VHS", "vhs"}});
    rarefiedLayout->addWidget(rarefiedSettings_);
    connect(enableRarefied, &QCheckBox::toggled, this, [this](bool checked) {
        updateDraft("rarefiedGas", checked);
    });
    layout->addWidget(rarefied);
    refreshControls();
}

void GoverningEquationsPanel::refreshControls()
{
    const bool viscous = draft_.value("equation") == "navier-stokes";
    viscousFlowChoices_->setVisible(viscous);
    inviscidNote_->setVisible(!viscous);
    turbulenceSettings_->setVisible(viscous && draft_.value("flowRegime") == "turbulent");
    rarefiedSettings_->setVisible(draft_.value("rarefiedGas").toBool());
}

void GoverningEquationsPanel::updateDraft(const QString &key, const QVariant &value)
{
    draft_.insert(key, value);
    if (key == "equation" || key == "viscousFlowRegime")
        draft_.insert("flowRegime", draft_.value("equation") == "euler" ? "inviscid" : draft_.value("viscousFlowRegime"));
    refreshControls();
    emit draftChanged(draft_);
}

} // namespace aerosim
