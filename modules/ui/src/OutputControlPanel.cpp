#include "aerosim/ui/OutputControlPanel.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
namespace aerosim { namespace {
template <size_t N> QString text(const char16_t (&s)[N]) { return QString::fromUtf16(reinterpret_cast<const ushort *>(s), int(N)-1); }
QStringList checkedNames(QListWidget *list) { QStringList result; for (int i=0;i<list->count();++i) if (list->item(i)->checkState()==Qt::Checked) result << list->item(i)->data(Qt::UserRole).toString(); return result; }
}}
namespace aerosim {
OutputControlPanel::OutputControlPanel(const QVariantMap &draft, QWidget *parent) : QWidget(parent), draft_(draft) {
    setObjectName("outputControlPanel"); setProperty("parameterPanel", true); setAttribute(Qt::WA_StyledBackground, true);
    const QStringList flowNames{"压力","密度","速度u","速度v","速度w","温度","湍流动能","湍流耗散率","组分"};
    const QStringList flowKeys{"pressure","density","velocity-u","velocity-v","velocity-w","temperature","turbulence-k","turbulence-dissipation","species"};
    const QStringList wallNames{"马赫数","节点状态","部件编号","初始化状态","热流","x方向摩擦力","y方向摩擦力","z方向摩擦力","摩擦力值","总温","总压"};
    const QStringList wallKeys{"mach","node-status","component-id","initialization-status","heat-flux","friction-x","friction-y","friction-z","friction-magnitude","total-temperature","total-pressure"};
    const QStringList variableNames = flowNames + wallNames;
    const QStringList variableKeys = flowKeys + wallKeys;
    const QStringList boundaryNames{"farfield","symmetry","wing_down","wing_edge","wing_up"};
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(14);
    auto *file = new QGroupBox(text(u"输出文件控制"), this); auto *ff = new QFormLayout(file);
    const auto integer = [this,file](const QString &name,const QString &key,int fallback,int min) { auto *s=new QSpinBox(file); s->setObjectName(key); s->setRange(min,1000000000); s->setValue(draft_.value(key,fallback).toInt()); s->setMaximumWidth(280); draft_[key]=s->value(); connect(s,QOverload<int>::of(&QSpinBox::valueChanged),this,[this,key](int v){draft_[key]=v;emit draftChanged(draft_);}); return s; };
    ff->addRow(text(u"续算文件输出间隔"), integer("续算文件输出间隔","restartInterval",100,0));
    ff->addRow(text(u"壁面结果输出间隔"), integer("壁面结果输出间隔","wallInterval",-1,-1));
    ff->addRow(text(u"流场结果输出间隔"), integer("流场结果输出间隔","flowInterval",-1,-1)); layout->addWidget(file);
    auto addList = [this,layout](const QString &title,const QStringList &names,const QStringList &keys,const QString &key,QListWidget **out) {
        auto *group=new QGroupBox(title,this); auto *row=new QHBoxLayout(group); auto *list=new QListWidget(group); list->setObjectName(key+"List"); list->setAlternatingRowColors(true); list->setMinimumHeight(180); row->addWidget(list,1); auto *buttons=new QVBoxLayout; auto *all=new QPushButton(text(u"全选"),group); auto *invert=new QPushButton(text(u"反选"),group); buttons->addWidget(all); buttons->addWidget(invert); buttons->addStretch(); row->addLayout(buttons); layout->addWidget(group); *out=list;
        QStringList selected=draft_.value(key).toStringList();
        if (!draft_.contains(key) && key == "flowVariables") selected = keys.mid(0, 5);
        if (!draft_.contains(key) && key == "wallVariables") selected = QStringList{"mach"};
        for(int i=0;i<names.size();++i){auto *item=new QListWidgetItem(names[i],list);item->setData(Qt::UserRole,keys.value(i));item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(selected.contains(keys.value(i))?Qt::Checked:Qt::Unchecked);}
        draft_[key] = selected;
        connect(all,&QPushButton::clicked,this,[=]{for(int i=0;i<list->count();++i)list->item(i)->setCheckState(Qt::Checked);}); connect(invert,&QPushButton::clicked,this,[=]{for(int i=0;i<list->count();++i)list->item(i)->setCheckState(list->item(i)->checkState()==Qt::Checked?Qt::Unchecked:Qt::Checked);}); connect(list,&QListWidget::itemChanged,this,[=]{draft_[key]=checkedNames(list);emit draftChanged(draft_);});
    };
    addList(text(u"流场输出变量"),variableNames,variableKeys,"flowVariables",&flow_); addList(text(u"壁面输出变量"),variableNames,variableKeys,"wallVariables",&wall_); addList(text(u"边界条件收敛历史"),boundaryNames,boundaryNames,"boundaryHistory",&history_); layout->addStretch();
}
void OutputControlPanel::loadList(QListWidget *, const QString &, const QStringList &, const QStringList &) {}
void OutputControlPanel::saveList(QListWidget *, const QString &) {}
}
