#include "aerosim/ui/intelligent/CalculationCurveMonitor.h"
#include "aerosim/ui/intelligent/MonitorResultDataSource.h"
#include "aerosim/ui/intelligent/QwtResultCurveView.h"
#include "MonitorFileTailSource.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

namespace aerosim {
CalculationCurveMonitor::CalculationCurveMonitor(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("calculationCurveMonitor"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *bar = new QHBoxLayout;
    auto *open = new QPushButton(QStringLiteral(u"监控解算器输出…"), this);
    open->setObjectName(QStringLiteral("calculationMonitorOpen"));
    bar->addWidget(open);
    pause_ = new QPushButton(QStringLiteral(u"暂停刷新"), this);
    pause_->setObjectName(QStringLiteral("calculationMonitorPause"));
    pause_->setCheckable(true);
    pause_->setEnabled(false);
    bar->addWidget(pause_);
    path_ = new QLabel(QStringLiteral(u"请选择解算器正在写入的结果文件"), this);
    path_->setTextFormat(Qt::PlainText);
    path_->setMinimumWidth(0);
    bar->addWidget(path_, 1);
    layout->addLayout(bar);
    auto *split = new QSplitter(this);
    variables_ = new QListWidget(split);
    variables_->setObjectName(QStringLiteral("calculationMonitorVariables"));
    variables_->setMinimumWidth(100);
    variables_->setMaximumWidth(240);
    variables_->setToolTip(QStringLiteral(u"勾选需要实时绘制的观测量"));
    curve_ = new QwtResultCurveView(split);
    split->setStretchFactor(1, 1);
    split->setSizes({150, 650});
    layout->addWidget(split, 1);
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("calculationMonitorStatus"));
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    status_->hide();
    layout->addWidget(status_);
    timer_ = new QTimer(this);
    timer_->setInterval(500);
    connect(timer_, &QTimer::timeout, this, [this] { poll(nullptr); });
    connect(pause_, &QPushButton::toggled, this, &CalculationCurveMonitor::setPaused);
    connect(variables_, &QListWidget::itemChanged, this, [this] { updateCurves(); });
    connect(open, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral(u"选择解算器输出文件"),
            QString(), MonitorResultDataSource().fileFilter());
        if (path.isEmpty()) return;
        QString error;
        if (!monitorFile(path, &error)) {
            status_->setText(error);
            status_->show();
        }
    });
}

CalculationCurveMonitor::~CalculationCurveMonitor() = default;

bool CalculationCurveMonitor::monitorFile(const QString &path, QString *error)
{
    std::unique_ptr<monitor::MonitorFileTailSource> source(new monitor::MonitorFileTailSource(path));
    if (!source->start(error)) return false;
    monitor::MonitorUpdate initial;
    monitor::MonitorLogAccumulator accumulator;
    accumulator.setDatasetId(source->sourceId());
    ResultTable table;
    if (!source->poll(&initial, error) || !accumulator.append(initial, error)) return false;
    if (accumulator.rowCount() > 0
        && !MonitorResultDataSource::fromLog(accumulator.log(), &table, error)) return false;
    timer_->stop();
    source_ = std::move(source);
    accumulator_ = std::move(accumulator);
    table_ = std::move(table);
    table_.sourcePath = path;
    variables_->clear();
    curve_->clearSeries();
    path_->setText(QFileInfo(path).fileName());
    path_->setToolTip(path);
    pause_->setEnabled(true);
    setPaused(false);
    refreshView();
    return true;
}

void CalculationCurveMonitor::setPaused(bool paused)
{
    const QSignalBlocker blocker(pause_);
    pause_->setChecked(paused);
    pause_->setText(paused ? QStringLiteral(u"继续刷新") : QStringLiteral(u"暂停刷新"));
    if (paused || !source_) timer_->stop();
    else timer_->start();
    status_->hide();
}

bool CalculationCurveMonitor::poll(QString *error)
{
    if (error) error->clear();
    if (!source_ || pause_->isChecked()) return true;
    monitor::MonitorUpdate update;
    QString message;
    if (!source_->poll(&update, &message) || !accumulator_.append(update, &message)) {
        status_->setText(message);
        status_->show();
        if (error) *error = message;
        return false;
    }
    if (update.isEmpty()) return true;
    if (accumulator_.rowCount() == 0) {
        table_.clear();
        curve_->clearSeries();
        status_->hide();
        return true;
    }
    if (!MonitorResultDataSource::fromLog(accumulator_.log(), &table_, &message)) {
        status_->setText(message);
        status_->show();
        if (error) *error = message;
        return false;
    }
    table_.sourcePath = path_->toolTip();
    refreshView();
    return true;
}

void CalculationCurveMonitor::refreshView()
{
    QStringList previousKeys;
    QStringList incomingKeys;
    for (const auto &variable : table_.variables)
        incomingKeys.append(variable.key);
    QStringList checkedKeys;
    for (int i = 0; i < variables_->count(); ++i) {
        auto *item = variables_->item(i);
        previousKeys.append(item->text());
        if (item->checkState() == Qt::Checked) checkedKeys.append(item->text());
    }
    if (previousKeys != incomingKeys) {
        const QSignalBlocker blocker(variables_);
        variables_->clear();
        for (const auto &variable : table_.variables) {
            auto *item = new QListWidgetItem(variable.key, variables_);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(!previousKeys.contains(variable.key) || checkedKeys.contains(variable.key)
                ? Qt::Checked : Qt::Unchecked);
        }
    }
    updateCurves();
    status_->hide();
}

void CalculationCurveMonitor::updateCurves()
{
    QStringList keys;
    for (int i = 0; i < variables_->count(); ++i)
        if (variables_->item(i)->checkState() == Qt::Checked) keys.append(variables_->item(i)->text());
    curve_->replaceSeries(table_, keys, table_.axisLabel);
}
} // namespace aerosim
