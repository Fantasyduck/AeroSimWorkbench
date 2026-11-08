#include "aerosim/ui/InitializationPanel.h"
#include <QComboBox>
#include <QDialog>
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
#include <functional>
namespace aerosim { namespace {
template <size_t N> QString text(const char16_t (&s)[N]) {
    return QString::fromUtf16(reinterpret_cast<const ushort *>(s), int(N)-1);
}
class Number final : public QDoubleSpinBox {
public:
    explicit Number(QWidget *parent) : QDoubleSpinBox(parent) {
        setDecimals(6); setRange(-1e12,1e12); setKeyboardTracking(false);
    }
    QString textFromValue(double v) const override {
        auto s=QDoubleSpinBox::textFromValue(v); const int p=s.indexOf(locale().decimalPoint());
        if(p>=0) while(s.size()>p+2 && s.endsWith(locale().zeroDigit())) s.chop(1);
        return s;
    }
};
QList<QPair<QString,QString>> parameterTypes() {
    return {{text(u"静压和静温"),"static-temperature-pressure"}, {text(u"单位雷诺数和静温"),"reynolds-temperature"},
            {text(u"总压和总温"),"total-temperature-pressure"}, {text(u"高度"),"altitude"}};
}
// Shared state form for global initialization and each local region.
class StateEditor final : public QWidget {
public:
    QVariantMap values;
    std::function<void(const QVariantMap &)> changed;
    explicit StateEditor(const QVariantMap &initial, QWidget *parent, bool fixed = false) : QWidget(parent), values(initial) {
        form_=new QFormLayout(this); form_->setContentsMargins(0,0,0,0); form_->setVerticalSpacing(8);
        form_->setRowWrapPolicy(QFormLayout::WrapLongRows);
        auto *type=new QComboBox(this); type->setObjectName("initialParameterType");
        for(const auto &p:parameterTypes()) type->addItem(p.first,p.second);
        type->setCurrentIndex(qMax(0,type->findData(values.value("parameterType","static-temperature-pressure"))));
        values["parameterType"]=type->currentData(); form_->addRow(text(u"选择参数"),type);
        auto *velocity=new QComboBox(this); velocity->setObjectName("initialVelocityDefinition");
        velocity->addItem(text(u"马赫数/攻角/侧滑角"),"angles"); velocity->addItem(text(u"马赫数/速度方向"),"direction");
        velocity->setCurrentIndex(values.value("velocityDefinition","angles")=="direction"?1:0);
        values["velocityDefinition"]=velocity->currentData(); form_->addRow(text(u"速度定义方式"),velocity);
        const auto number=[=](const QString &key,const QString &caption,double fallback,double minimum) {
            auto *v=new Number(this); v->setObjectName(key); v->setMinimum(minimum);
            v->setValue(values.value(key,fallback).toDouble()); values[key]=v->value();
            auto *label=new QLabel(caption,this); rows_[key]=qMakePair(label,static_cast<QWidget *>(v));
            connect(v,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[=](double d){values[key]=d; notify();});
        };
        number("machNumber",text(u"马赫数"),6.,0.);
        number("angleOfAttack",text(u"攻角 (°)"),0.,-180.); number("sideslipAngle",text(u"侧滑角 (°)"),0.,-180.);
        number("directionX",text(u"X方向速度方向"),1.,-1e12); number("directionY",text(u"Y方向速度方向"),0.,-1e12);
        number("directionZ",text(u"Z方向速度方向"),0.,-1e12);
        number("staticPressure",text(u"静压 (Pa)"),955.,0.000001); number("staticTemperature",text(u"静温 (K)"),60.,0.000001);
        number("unitReynoldsNumber",text(u"单位雷诺数 (1/m)"),1.,0.000001);
        number("totalPressure",text(u"总压 (Pa)"),955.,0.000001); number("totalTemperature",text(u"总温 (K)"),60.,0.000001);
        number("altitude",text(u"高度 (m)"),1.,-1e5);
        connect(type,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=](int){values["parameterType"]=type->currentData(); updateRows();notify();});
        connect(velocity,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=](int){values["velocityDefinition"]=velocity->currentData();updateRows();notify();});
        updateRows();
        if(fixed) {
            for(auto *choice : {type,velocity}) {
                auto row=form_->takeRow(choice);if(row.labelItem)delete row.labelItem->widget();delete row.labelItem;delete row.fieldItem;
                delete choice;
            }
        }
    }
private:
    QFormLayout *form_;
    QMap<QString,QPair<QWidget *,QWidget *>> rows_;
    void notify() { if(changed) changed(values); }
    void updateRows() {
        for(const auto &r:rows_) {
            if(form_->indexOf(r.second)>=0) { auto items=form_->takeRow(r.second); delete items.labelItem;delete items.fieldItem; }
            r.first->hide();r.second->hide();
        }
        QStringList keys{"machNumber"};
        keys += values.value("velocityDefinition")=="direction" ? QStringList{"directionX","directionY","directionZ"} : QStringList{"angleOfAttack","sideslipAngle"};
        const auto type=values.value("parameterType").toString();
        if(type=="static-temperature-pressure") keys += QStringList{"staticPressure","staticTemperature"};
        else if(type=="reynolds-temperature") keys += QStringList{"unitReynoldsNumber","staticTemperature"};
        else if(type=="total-temperature-pressure") keys += QStringList{"totalPressure","totalTemperature"};
        else keys << "altitude";
        for(const auto &key:keys) { const auto r=rows_[key]; form_->addRow(r.first,r.second);r.first->show();r.second->show(); }
        for(int row=0;row<form_->rowCount();++row) {
            auto *item=form_->itemAt(row,QFormLayout::LabelRole);
            if(item && item->widget())item->widget()->setMinimumWidth(190);
        }
    }
};
}}
namespace aerosim {
InitializationPanel::InitializationPanel(const QVariantMap &draft,QWidget *parent) : QWidget(parent),draft_(draft),regions_(draft.value("regions").toList()) {
    setObjectName("initializationPanel");setProperty("parameterPanel",true);setAttribute(Qt::WA_StyledBackground,true);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(16);
    auto *global=new QGroupBox(text(u"全局初始化"),this);auto *globalLayout=new QVBoxLayout(global);
    auto *form=new QFormLayout; globalForm_=form;globalLayout->addLayout(form);
    mode_=new QComboBox(global);mode_->setObjectName("initializationMode");
    mode_->addItem(text(u"自定义输入"),"custom");mode_->addItem(text(u"从边界条件初始化"),"boundary");mode_->addItem(text(u"从参考量初始化"),"reference");
    mode_->setCurrentIndex(qMax(0,mode_->findData(draft_.value("mode","custom"))));draft_["mode"]=mode_->currentData();
    form->addRow(text(u"初始化方式"),mode_);
    form->labelForField(mode_)->setMinimumWidth(190);
    source_=new QComboBox(global);source_->setObjectName("initializationBoundary");sourceLabel_=new QLabel(text(u"从边界条件获取"),global);form->addRow(sourceLabel_,source_);
    state_=new QWidget(global);state_->setObjectName("initializationStateHost");auto *stateLayout=new QVBoxLayout(state_);stateLayout->setContentsMargins(0,0,0,0);globalLayout->addWidget(state_);
    sourceHint_=new QLabel(global);sourceHint_->setWordWrap(true);sourceHint_->setProperty("parameterHint",true);globalLayout->addWidget(sourceHint_);layout->addWidget(global);
    connect(mode_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=](int){draft_["mode"]=mode_->currentData();updateGlobal();publish();});
    connect(source_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=](int){draft_["boundaryId"]=source_->currentData();updateGlobal();publish();});
    auto *local=new QGroupBox(text(u"局部初始化"),this);auto *localLayout=new QVBoxLayout(local);auto *countForm=new QFormLayout;
    auto *count=new QSpinBox(local);count->setObjectName("initializationRegionCount");count->setRange(0,100);count->setMaximumWidth(220);
    count->setValue(draft_.value("regionCount",0).toInt());draft_["regionCount"]=count->value();countForm->addRow(text(u"局部初始化个数"),count);localLayout->addLayout(countForm);
    table_=new QTableWidget(local);table_->setObjectName("initializationRegions");table_->setColumnCount(3);table_->setHorizontalHeaderLabels({text(u"形状"),text(u"参数类型"),text(u"设置")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->verticalHeader()->setDefaultSectionSize(40);table_->setMinimumHeight(200);localLayout->addWidget(table_);layout->addWidget(local);layout->addStretch();
    connect(count,QOverload<int>::of(&QSpinBox::valueChanged),this,[=](int n){draft_["regionCount"]=n;rebuildRegions(n);publish();});
    rebuildRegions(count->value());setSources({},{});
}
void InitializationPanel::setSources(const QVariantMap &boundary,const QVariantMap &reference) {
    boundaries_=boundary.value("boundaries").toList();reference_=reference;
    const QSignalBlocker block(source_);source_->clear();
    for(const auto &v:boundaries_) {const auto r=v.toMap();if(r.value("type")=="farfield" && !r.value("farfield").toMap().isEmpty()) source_->addItem(r.value("name").toString(),r.value("id"));}
    source_->setCurrentIndex(qMax(0,source_->findData(draft_.value("boundaryId"))));
    if(source_->count()) draft_["boundaryId"]=source_->currentData();
    updateGlobal();
}
void InitializationPanel::updateGlobal() {
    const auto mode=mode_->currentData().toString(); const bool boundary=mode=="boundary";
    if(globalForm_->indexOf(source_)>=0){auto row=globalForm_->takeRow(source_);delete row.labelItem;delete row.fieldItem;}
    if(boundary){globalForm_->addRow(sourceLabel_,source_);sourceLabel_->setMinimumWidth(190);}
    source_->setVisible(boundary);sourceLabel_->setVisible(boundary);
    QVariantMap state=draft_.value("customState").toMap();bool available=true;
    if(mode=="reference") {state=reference_;available=!reference_.isEmpty();}
    if(boundary) {state.clear();for(const auto &v:boundaries_){auto r=v.toMap();if(r.value("id")==source_->currentData()) state=r.value("farfield").toMap();}available=!state.isEmpty();}
    while(auto *item=state_->layout()->takeAt(0)){delete item->widget();delete item;}
    auto *editor=new StateEditor(state,state_);editor->setObjectName("globalInitializationState");state_->layout()->addWidget(editor);
    editor->setEnabled(mode=="custom");editor->setVisible(available);
    if(mode=="custom") {draft_["customState"]=editor->values; editor->changed=[this](const QVariantMap &v){draft_["customState"]=v;publish();};}
    sourceHint_->setText(!available ? text(u"尚无可用来源参数，请先编辑参考量或保存远场边界条件，或选择自定义输入。") : mode!="custom" ? text(u"显示来源页面已编辑的草稿参数；未执行物理换算或求解初始化。") : QString());
    sourceHint_->setVisible(!sourceHint_->text().isEmpty());
}
void InitializationPanel::rebuildRegions(int count) {
    while(regions_.size()<count) regions_.append(QVariantMap{{"shape","box"},{"parameterType","static-temperature-pressure"}});
    table_->setRowCount(0);table_->setRowCount(count);
    for(int i=0;i<count;++i) {
        auto region=regions_[i].toMap();auto *shape=new QComboBox(table_);
        shape->addItem(text(u"箱体"),"box");shape->addItem(text(u"圆球"),"sphere");
        shape->addItem(text(u"圆柱体"),"cylinder");shape->addItem(text(u"圆锥体"),"cone");
        shape->setCurrentIndex(qMax(0,shape->findData(region.value("shape"))));table_->setCellWidget(i,0,shape);
        connect(shape,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=](int){auto r=regions_[i].toMap();r["shape"]=shape->currentData();regions_[i]=r;publish();});
        auto *type=new QComboBox(table_);for(const auto &p:parameterTypes())type->addItem(p.first,p.second);type->setCurrentIndex(qMax(0,type->findData(region.value("parameterType"))));table_->setCellWidget(i,1,type);
        connect(type,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[=](int){auto r=regions_[i].toMap();r["parameterType"]=type->currentData();auto s=r.value("state").toMap();s["parameterType"]=type->currentData();r["state"]=s;regions_[i]=r;publish();});
        auto *settings=new QPushButton(text(u"设置"),table_);settings->setObjectName(QStringLiteral("initializationRegion%1Settings").arg(i));table_->setCellWidget(i,2,settings);
        connect(settings,&QPushButton::clicked,this,[=]{
            auto r=regions_[i].toMap();QDialog dialog(this);dialog.setObjectName("localInitializationDialog");dialog.setWindowTitle(text(u"局部初始化设置"));dialog.setProperty("parameterEditor",true);dialog.resize(680,680);
            dialog.setStyleSheet("QAbstractSpinBox { min-height: 22px; max-height: 22px; padding: 2px 6px; }");
            auto *layout=new QVBoxLayout(&dialog);auto *scroll=new QScrollArea(&dialog);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
            auto *content=new QWidget(scroll);content->setProperty("parameterSurface",true);auto *body=new QVBoxLayout(content);
            auto *box=new QGroupBox(shape->currentText(),content);auto *form=new QFormLayout(box);body->addWidget(box);
            const QString geometry=r.value("shape","box").toString();
            QList<Number *> bounds;
            const auto scalar=[&](QWidget *owner,const QString &key,double fallback,bool radius=false){
                auto *n=new Number(owner);n->setObjectName(key);n->setMinimumWidth(70);if(radius)n->setMinimum(0);
                n->setValue(r.value(key,fallback).toDouble());bounds.append(n);return n;
            };
            const auto vector=[&](const QString &label,const QStringList &keys,bool second=false){
                auto *row=new QWidget(box);auto *items=new QHBoxLayout(row);items->setContentsMargins(0,0,0,0);
                for(const auto &key:keys)items->addWidget(scalar(row,key,second && geometry=="box" ? 1.:0.));
                form->addRow(label,row);
            };
            if(geometry=="sphere") {
                vector(text(u"球心坐标 (m)"),{"centerX","centerY","centerZ"});
                form->addRow(text(u"半径 (m)"),scalar(box,"radius",0.,true));
            } else if(geometry=="box") {
                vector(text(u"顶点1 (m)"),{"xMin","yMin","zMin"});
                vector(text(u"顶点2 (m)"),{"xMax","yMax","zMax"},true);
            } else {
                if(geometry=="cylinder")form->addRow(text(u"半径 (m)"),scalar(box,"radius",0.,true));
                vector(text(u"顶心1坐标 (m)"),{"axis1X","axis1Y","axis1Z"});
                vector(text(u"顶心2坐标 (m)"),{"axis2X","axis2Y","axis2Z"});
                if(geometry=="cone") {
                    form->addRow(text(u"半径1 (m)"),scalar(box,"radius1",0.,true));
                    form->addRow(text(u"半径2 (m)"),scalar(box,"radius2",0.,true));
                }
            }
            auto s=r.value("state").toMap();s["parameterType"]=r.value("parameterType");
            s["velocityDefinition"]="angles";
            if(!s.contains("machNumber"))s["machNumber"]=3.;
            if(!s.contains("staticTemperature"))s["staticTemperature"]=1000.;
            auto *stateGroup=new QGroupBox(text(u"初始化参数"),content);auto *stateLayout=new QVBoxLayout(stateGroup);
            auto *editor=new StateEditor(s,stateGroup,true);stateLayout->addWidget(editor);body->addWidget(stateGroup);body->addStretch();scroll->setWidget(content);layout->addWidget(scroll);
            auto *error=new QLabel(&dialog);error->hide();layout->addWidget(error);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Ok)->setText(text(u"确定"));buttons->button(QDialogButtonBox::Cancel)->setText(text(u"取消"));layout->addWidget(buttons);
            connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
                const auto value=[&](const QString &key){return dialog.findChild<QDoubleSpinBox *>(key)->value();};
                bool valid=true;
                if(geometry=="box") {for(const auto &axis:QStringList{"x","y","z"})valid=valid && value(axis+"Min")!=value(axis+"Max");}
                else if(geometry=="sphere")valid=value("radius")>0.;
                else {
                    bool distinct=false;for(const auto &axis:QStringList{"X","Y","Z"})distinct=distinct || value("axis1"+axis)!=value("axis2"+axis);
                    valid=distinct && (geometry=="cylinder" ? value("radius")>0. : value("radius1")>0. || value("radius2")>0.);
                }
                if(!valid){error->setText(text(u"请设置有效区域：箱体各方向长度、球或柱半径须大于零；柱/锥的两顶心不能重合，锥体至少一个半径大于零。"));error->show();return;}
                dialog.accept();});
            if(dialog.exec()==QDialog::Accepted) {for(auto *n:bounds)r[n->objectName()]=n->value();r["state"]=editor->values;r["parameterType"]=editor->values.value("parameterType");regions_[i]=r;const QSignalBlocker block(type);type->setCurrentIndex(type->findData(r.value("parameterType")));publish();}
        });
    }
}
void InitializationPanel::publish() {draft_["regions"]=regions_;emit draftChanged(draft_);}
}
