#include "aerosim/ui/intelligent/ResultForecastPanel.h"
#include "aerosim/machine_learning/TimeSeriesForecast.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <limits>

namespace aerosim {
namespace {

double numericValue(const QString &text)
{
    bool ok = false;
    const double value = text.toDouble(&ok);
    return ok && std::isfinite(value) ? value : std::numeric_limits<double>::quiet_NaN();
}

QTableWidget *resultTable(QWidget *parent, const QString &name)
{
    auto *table = new QTableWidget(parent);
    table->setObjectName(name);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    return table;
}

void setNumber(QTableWidget *table, int row, int column, double value)
{
    auto *item = new QTableWidgetItem(QString::number(value, 'g', 17));
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, column, item);
}

} // namespace

ResultForecastPanel::ResultForecastPanel(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("resultForecastPanel"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 0, 0, 0);
    settings_ = new QWidget(this);
    auto *layout = new QVBoxLayout(settings_);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    method_ = new QComboBox(settings_);
    method_->setObjectName(QStringLiteral("forecastMethod"));
    method_->addItem(QStringLiteral("RBF-KRR"), QStringLiteral("rbf-krr"));
    form->addRow(QStringLiteral(u"分析方法"), method_);
    dataset_ = new QComboBox(settings_);
    dataset_->setObjectName(QStringLiteral("forecastDataset"));
    form->addRow(QStringLiteral(u"数据文件"), dataset_);
    coordinate_ = new QComboBox(settings_);
    coordinate_->setObjectName(QStringLiteral("forecastTimeColumn"));
    form->addRow(QStringLiteral(u"时间 / 步号"), coordinate_);
    target_ = new QComboBox(settings_);
    target_->setObjectName(QStringLiteral("forecastTargetColumn"));
    form->addRow(QStringLiteral(u"预测目标"), target_);
    layout->addLayout(form);
    auto *inputLabel = new QLabel(QStringLiteral(u"输入变量（使用其历史窗口）"), settings_);
    layout->addWidget(inputLabel);
    inputs_ = new QListWidget(settings_);
    inputs_->setObjectName(QStringLiteral("forecastInputs"));
    inputs_->setMinimumHeight(100);
    inputs_->setMaximumHeight(180);
    inputLabel->setBuddy(inputs_);
    layout->addWidget(inputs_);
    auto *parameters = new QFormLayout;
    window_ = new QSpinBox(settings_);
    window_->setObjectName(QStringLiteral("forecastWindow"));
    window_->setRange(1, 10000);
    window_->setValue(2);
    parameters->addRow(QStringLiteral(u"历史窗口"), window_);
    horizon_ = new QSpinBox(settings_);
    horizon_->setObjectName(QStringLiteral("forecastHorizon"));
    horizon_->setRange(1, 1000);
    horizon_->setValue(5);
    parameters->addRow(QStringLiteral(u"预测步数"), horizon_);
    step_ = new QDoubleSpinBox(settings_);
    step_->setObjectName(QStringLiteral("forecastStep"));
    step_->setDecimals(12);
    step_->setRange(1e-12, 1e12);
    step_->setValue(1);
    step_->setToolTip(QStringLiteral(u"输入数据相邻记录在所选时间列上的间隔。预测步数决定输出数量。"));
    auto *stepControls = new QWidget(settings_);
    auto *stepLayout = new QHBoxLayout(stepControls);
    stepLayout->setContentsMargins(0, 0, 0, 0);
    auto *automaticStep = new QPushButton(QStringLiteral(u"自动"), stepControls);
    automaticStep->setObjectName(QStringLiteral("forecastAutomaticStep"));
    automaticStep->setToolTip(QStringLiteral(u"从所选时间列的末尾两条记录读取数据步长。"));
    stepLayout->addWidget(step_, 1);
    stepLayout->addWidget(automaticStep);
    parameters->addRow(QStringLiteral(u"采样步长"), stepControls);
    gamma_ = new QLineEdit(QStringLiteral("1e-4"), settings_);
    gamma_->setObjectName(QStringLiteral("forecastGamma"));
    regularization_ = new QLineEdit(QStringLiteral("1e-8"), settings_);
    regularization_->setObjectName(QStringLiteral("forecastRegularization"));
    for (auto *editor : {gamma_, regularization_}) {
        auto *validator = new QDoubleValidator(editor);
        validator->setLocale(QLocale::c());
        validator->setNotation(QDoubleValidator::ScientificNotation);
        editor->setValidator(validator);
    }
    parameters->addRow(QStringLiteral(u"RBF γ"), gamma_);
    parameters->addRow(QStringLiteral(u"正则项 λ"), regularization_);
    layout->addLayout(parameters);
    outer->addWidget(settings_);
    auto *buttons = new QHBoxLayout;
    run_ = new QPushButton(QStringLiteral(u"开始预测"), this);
    run_->setObjectName(QStringLiteral("forecastRunButton"));
    cancel_ = new QPushButton(QStringLiteral(u"取消"), this);
    cancel_->setObjectName(QStringLiteral("forecastCancelButton"));
    cancel_->hide();
    buttons->addWidget(run_, 1);
    buttons->addWidget(cancel_);
    outer->addLayout(buttons);
    status_ = new QLabel(QStringLiteral(u"导入数据后，选择时间列、预测目标和输入变量。"), this);
    status_->setObjectName(QStringLiteral("forecastStatus"));
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    outer->addWidget(status_);
    outer->addStretch();

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("forecastResultTabs"));
    future_ = resultTable(tabs, QStringLiteral("forecastResults"));
    validation_ = resultTable(tabs, QStringLiteral("forecastBacktest"));
    tabs->addTab(future_, QStringLiteral(u"预测结果"));
    tabs->addTab(validation_, QStringLiteral(u"留出回测"));
    results_ = tabs;
    tabs->hide();

    connect(dataset_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ResultForecastPanel::refreshDataset);
    connect(method_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ResultForecastPanel::invalidateResult);
    connect(coordinate_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ResultForecastPanel::refreshCoordinate);
    connect(target_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ResultForecastPanel::invalidateResult);
    connect(inputs_, &QListWidget::itemChanged, this, &ResultForecastPanel::invalidateResult);
    connect(window_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResultForecastPanel::invalidateResult);
    connect(horizon_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ResultForecastPanel::invalidateResult);
    connect(step_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ResultForecastPanel::invalidateResult);
    connect(automaticStep, &QPushButton::clicked, this, &ResultForecastPanel::refreshCoordinate);
    connect(gamma_, &QLineEdit::textChanged, this, &ResultForecastPanel::invalidateResult);
    connect(regularization_, &QLineEdit::textChanged, this, &ResultForecastPanel::invalidateResult);
    connect(run_, &QPushButton::clicked, this, &ResultForecastPanel::runForecast);
    connect(cancel_, &QPushButton::clicked, this, [this] {
        if (cancellation_)
            cancellation_->store(true);
        cancel_->setEnabled(false);
        status_->setText(QStringLiteral(u"正在取消预测…"));
    });
    settings_->setEnabled(false);
    updateRunButton();
}

ResultForecastPanel::~ResultForecastPanel()
{
    if (cancellation_)
        cancellation_->store(true);
}

void ResultForecastPanel::setDatasets(const QVector<CsvResultTable> &datasets)
{
    invalidateResult();
    datasets_ = datasets;
    {
        const QSignalBlocker blocker(dataset_);
        dataset_->clear();
        for (const auto &data : datasets_) {
            dataset_->addItem(QFileInfo(data.sourcePath).fileName());
            dataset_->setItemData(dataset_->count() - 1, data.sourcePath, Qt::ToolTipRole);
        }
    }
    refreshDataset();
}

void ResultForecastPanel::refreshDataset()
{
    const QSignalBlocker coordinateBlocker(coordinate_);
    const QSignalBlocker targetBlocker(target_);
    const QSignalBlocker inputBlocker(inputs_);
    coordinate_->clear();
    coordinate_->addItem(QStringLiteral(u"记录序号"), -1);
    target_->clear();
    inputs_->clear();
    if (dataset_->currentIndex() < 0) {
        invalidateResult();
        status_->setText(QStringLiteral(u"导入数据后，选择时间列、预测目标和输入变量。"));
        return;
    }
    const auto &data = datasets_.at(dataset_->currentIndex());
    for (int column = 0; column < data.columns.size(); ++column) {
        bool numeric = false;
        for (const auto &row : data.rows) {
            if (std::isfinite(numericValue(row.value(column)))) {
                numeric = true;
                break;
            }
        }
        if (numeric) {
            coordinate_->addItem(data.columns.at(column), column);
            target_->addItem(data.columns.at(column), column);
        }
    }
    const QStringList preferred{QStringLiteral("time"), QStringLiteral("physical_time"),
        QStringLiteral("flow_time"), QStringLiteral("inner_iter"), QStringLiteral("outer_iter"),
        QStringLiteral("time_iter"), QStringLiteral("iteration"), QStringLiteral("step")};
    for (const QString &name : preferred) {
        const int index = coordinate_->findText(name, Qt::MatchFixedString);
        if (index < 1 || data.rows.size() < 2)
            continue;
        const int column = coordinate_->itemData(index).toInt();
        const double last = numericValue(data.rows.last().value(column));
        const double previous = numericValue(data.rows.at(data.rows.size() - 2).value(column));
        if (std::isfinite(last) && std::isfinite(previous) && last > previous) {
            coordinate_->setCurrentIndex(index);
            break;
        }
    }
    int targetIndex = target_->findText(QStringLiteral("CD"), Qt::MatchFixedString);
    if (targetIndex < 0) {
        targetIndex = 0;
        while (targetIndex + 1 < target_->count()
               && target_->itemData(targetIndex) == coordinate_->currentData())
            ++targetIndex;
    }
    target_->setCurrentIndex(targetIndex);
    for (int index = 0; index < target_->count(); ++index) {
        auto *item = new QListWidgetItem(target_->itemText(index), inputs_);
        item->setData(Qt::UserRole, target_->itemData(index));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(index == targetIndex ? Qt::Checked : Qt::Unchecked);
    }
    refreshCoordinate();
    if (target_->count() == 0)
        status_->setText(QStringLiteral(u"当前文件没有可用于预测的数值列。"));
}

void ResultForecastPanel::refreshCoordinate()
{
    const int column = coordinate_->currentData().toInt();
    const QSignalBlocker blocker(step_);
    step_->setEnabled(column >= 0);
    step_->setValue(1);
    if (dataset_->currentIndex() >= 0 && column >= 0) {
        const auto &rows = datasets_.at(dataset_->currentIndex()).rows;
        if (rows.size() > 1) {
            const double delta = numericValue(rows.last().value(column))
                - numericValue(rows.at(rows.size() - 2).value(column));
            if (std::isfinite(delta) && delta > 0)
                step_->setValue(delta);
        }
    }
    invalidateResult();
}

void ResultForecastPanel::invalidateResult()
{
    if (cancellation_)
        cancellation_->store(true);
    watcher_ = nullptr;
    cancellation_.reset();
    settings_->setEnabled(dataset_->currentIndex() >= 0);
    cancel_->hide();
    future_->setRowCount(0);
    validation_->setRowCount(0);
    if (results_)
        results_->hide();
    status_->setText(QStringLiteral(u"选择输入变量并设置参数，数据充足时会同时进行末尾留出回测。"));
    updateRunButton();
}

void ResultForecastPanel::updateRunButton()
{
    bool selected = false;
    for (int index = 0; index < inputs_->count(); ++index)
        selected = selected || inputs_->item(index)->checkState() == Qt::Checked;
    const bool valid = method_->currentData() == QStringLiteral("rbf-krr")
        && dataset_->currentIndex() >= 0 && target_->currentIndex() >= 0
        && selected && numericValue(gamma_->text()) > 0 && numericValue(regularization_->text()) > 0;
    const QString error = samplingError();
    run_->setEnabled(valid && error.isEmpty() && !watcher_);
    QString hint = valid ? QString() : QStringLiteral(u"请选择数值目标、至少一个输入变量，并填写正数 γ 和 λ。");
    if (!error.isEmpty()) {
        hint = error;
        status_->setText(error);
    }
    run_->setToolTip(hint);
}

QString ResultForecastPanel::samplingError() const
{
    const int datasetIndex = dataset_->currentIndex();
    const int column = coordinate_->currentData().toInt();
    if (datasetIndex < 0 || column < 0)
        return QString();
    const auto &rows = datasets_.at(datasetIndex).rows;
    if (rows.size() < 2)
        return QString();
    const double last = numericValue(rows.last().value(column));
    const double previous = numericValue(rows.at(rows.size() - 2).value(column));
    const double delta = last - previous;
    if (!std::isfinite(delta) || delta <= 0)
        return QStringLiteral(u"时间列 %1 的末尾记录没有有效的递增间隔，请选择递增的时间/步号列，或使用记录序号。")
            .arg(coordinate_->currentText());
    const double tolerance = step_->value() * 1e-6 + 8 * std::numeric_limits<double>::epsilon()
        * std::max(std::abs(last), std::abs(previous));
    if (std::abs(delta - step_->value()) > tolerance)
        return QStringLiteral(u"时间列 %1 的末尾间隔为 %2，当前采样步长为 %3。点击“自动”恢复数据步长。")
            .arg(coordinate_->currentText(), QString::number(delta, 'g', 12),
                 QString::number(step_->value(), 'g', 12));
    return QString();
}

void ResultForecastPanel::runForecast()
{
    if (!run_->isEnabled())
        return;
    invalidateResult();
    machine_learning::TimeSeriesData data;
    const auto &csv = datasets_.at(dataset_->currentIndex());
    const int coordinateColumn = coordinate_->currentData().toInt();
    const int targetColumn = target_->currentData().toInt();
    QVector<int> columns;
    for (int index = 0; index < inputs_->count(); ++index) {
        if (inputs_->item(index)->checkState() == Qt::Checked)
            columns.append(inputs_->item(index)->data(Qt::UserRole).toInt());
    }
    for (int row = 0; row < csv.rows.size(); ++row) {
        data.coordinates.push_back(coordinateColumn < 0 ? row + 1.0
            : numericValue(csv.rows.at(row).value(coordinateColumn)));
        data.targets.push_back(numericValue(csv.rows.at(row).value(targetColumn)));
        machine_learning::RbfKrr::Sample sample;
        for (int column : columns)
            sample.push_back(numericValue(csv.rows.at(row).value(column)));
        data.inputs.push_back(std::move(sample));
    }
    machine_learning::ForecastOptions options;
    options.window = window_->value();
    options.horizon = horizon_->value();
    options.step = step_->value();
    options.model.gamma = numericValue(gamma_->text());
    options.model.regularization = numericValue(regularization_->text());
    const QString coordinateName = coordinate_->currentText();
    const QString targetName = target_->currentText();
    auto cancellation = std::make_shared<std::atomic_bool>(false);
    cancellation_ = cancellation;
    auto *watcher = new QFutureWatcher<machine_learning::ForecastResult>(this);
    watcher_ = watcher;
    connect(watcher, &QFutureWatcher<machine_learning::ForecastResult>::finished, this,
        [this, watcher, coordinateName, targetName, cancellation] {
            auto result = watcher->result();
            watcher->deleteLater();
            if (watcher != watcher_)
                return;
            if (cancellation->load()) {
                result.cancelled = true;
                result.error = "Prediction cancelled.";
            }
            watcher_ = nullptr;
            cancellation_.reset();
            settings_->setEnabled(true);
            cancel_->hide();
            showResult(result, coordinateName, targetName);
            updateRunButton();
            emit forecastFinished(result.error.empty());
        }, Qt::QueuedConnection);
    settings_->setEnabled(false);
    run_->setEnabled(false);
    cancel_->setEnabled(true);
    cancel_->show();
    status_->setText(QStringLiteral(u"正在训练 RBF-KRR 并执行留出回测…"));
    watcher->setFuture(QtConcurrent::run([data, options, cancellation] {
        return machine_learning::forecastRbfKrr(data, options, cancellation.get());
    }));
}

void ResultForecastPanel::showResult(const machine_learning::ForecastResult &result,
                                    const QString &coordinate, const QString &target)
{
    if (!result.error.empty()) {
        status_->setText(result.cancelled ? QStringLiteral(u"已取消预测。")
            : QStringLiteral(u"预测失败：%1").arg(QString::fromStdString(result.error)));
        return;
    }
    future_->setColumnCount(2);
    future_->setHorizontalHeaderLabels({coordinate, target + QStringLiteral(u"（预测）")});
    future_->setRowCount(int(result.future.size()));
    for (int row = 0; row < future_->rowCount(); ++row) {
        setNumber(future_, row, 0, result.future[row].coordinate);
        setNumber(future_, row, 1, result.future[row].prediction);
    }
    validation_->setColumnCount(4);
    validation_->setHorizontalHeaderLabels({coordinate, target + QStringLiteral(u"（实际）"),
        QStringLiteral(u"预测值"), QStringLiteral(u"绝对误差")});
    validation_->setRowCount(int(result.validation.size()));
    for (int row = 0; row < validation_->rowCount(); ++row) {
        setNumber(validation_, row, 0, result.validation[row].coordinate);
        setNumber(validation_, row, 1, result.actual[row]);
        setNumber(validation_, row, 2, result.validation[row].prediction);
        setNumber(validation_, row, 3, std::abs(result.validation[row].prediction - result.actual[row]));
    }
    results_->show();
    if (result.validation.empty()) {
        status_->setText(QStringLiteral(u"最新连续片段 %1 条记录\n已生成预测；当前记录数不足以执行末尾留出回测。")
            .arg(qulonglong(result.segmentSamples)));
        return;
    }
    status_->setText(QStringLiteral(u"最新连续片段 %1 条记录\n末尾 %2 步回测 MAE %3\n末值基准 MAE %4\n已用完整连续片段重新训练并预测。")
        .arg(qulonglong(result.segmentSamples)).arg(result.validation.size())
        .arg(QString::number(result.meanAbsoluteError, 'g', 6))
        .arg(QString::number(result.baselineMeanAbsoluteError, 'g', 6)));
}

} // namespace aerosim
