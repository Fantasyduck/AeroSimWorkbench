#include "aerosim/ui/intelligent/ResultDataTablePage.h"

#include <QFileDialog>
#include <QComboBox>
#include <QSignalBlocker>
#include <limits>
#include <QFont>
#include <QPointer>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cmath>

namespace aerosim {
namespace {

// A result file can carry thousands of iterations. The grid is a reading aid,
// not a data store, so the sample columns stop here and the status line says so
// rather than silently hiding the tail.
constexpr int kMaxSampleColumns = 400;

QString numberText(double value)
{
    if (!std::isfinite(value))
        return QStringLiteral(u"—");
    return QString::number(value, 'g', 6);
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

ResultDataTablePage::ResultDataTablePage(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("resultDataTablePage"));
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    buildUi();
}

void ResultDataTablePage::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);

    auto *bar = new QHBoxLayout;
    openButton_ = new QPushButton(QStringLiteral(u"打开结果文件…"), this);
    openButton_->setObjectName(QStringLiteral("resultDataOpenButton"));
    openButton_->setEnabled(false);
    connect(openButton_, &QPushButton::clicked, this, &ResultDataTablePage::chooseFile);
    bar->addWidget(openButton_);

    sourceLabel_ = new QLabel(QStringLiteral(u"尚未打开结果文件。"), this);
    sourceLabel_->setObjectName(QStringLiteral("resultDataSourceLabel"));
    sourceLabel_->setTextFormat(Qt::PlainText);
    bar->addWidget(sourceLabel_, 1);

    axisLabelView_ = new QLabel(this);
    axisLabelView_->setObjectName(QStringLiteral("resultDataAxisLabel"));
    axisLabelView_->setTextFormat(Qt::PlainText);
    bar->addWidget(axisLabelView_);
    xAxis_ = new QComboBox(this);
    xAxis_->setObjectName(QStringLiteral("resultDataXAxis"));
    xAxis_->setEnabled(false);
    xAxis_->setAccessibleName(QStringLiteral(u"横轴变量"));
    xAxis_->setToolTip(QStringLiteral(u"选择横轴；在上方表格中选中需要绘制的纵轴变量。"));
    bar->addWidget(xAxis_);
    connect(xAxis_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        updateCurveView();
    });
    outer->addLayout(bar);

    auto *split = new QSplitter(Qt::Horizontal, this);
    split->setObjectName(QStringLiteral("resultDataSplit"));

    tree_ = new QTreeWidget(split);
    tree_->setObjectName(QStringLiteral("resultDataVariableTree"));
    tree_->setHeaderLabels({QStringLiteral(u"变量"), QStringLiteral(u"单位")});
    tree_->setColumnWidth(0, 220);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setUniformRowHeights(true);
    connect(tree_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *, int) {
        populateTable();
        updateCurveView();
        updateStatus();
    });
    split->addWidget(tree_);

    auto *right = new QSplitter(Qt::Vertical, split);
    right->setObjectName(QStringLiteral("resultDataRightSplit"));

    curveHost_ = new QWidget(right);
    curveLayout_ = new QVBoxLayout(curveHost_);
    curveLayout_->setContentsMargins(0, 0, 0, 0);
    curveHint_ = new QLabel(
        QStringLiteral(u"曲线视图未接入。当前构建未启用图表模块，表格仍可使用。"), curveHost_);
    curveHint_->setObjectName(QStringLiteral("resultDataCurveHint"));
    curveHint_->setTextFormat(Qt::PlainText);
    curveHint_->setWordWrap(true);
    curveHint_->setAlignment(Qt::AlignCenter);
    curveLayout_->addWidget(curveHint_);

    tableWidget_ = new QTableWidget(right);
    tableWidget_->setObjectName(QStringLiteral("resultDataTable"));
    tableWidget_->setMinimumHeight(96);
    tableWidget_->verticalHeader()->hide();
    tableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(tableWidget_, &QTableWidget::itemSelectionChanged, this, [this] {
        updateCurveView();
        updateStatus();
    });
    tableWidget_->setAlternatingRowColors(true);
    right->addWidget(tableWidget_);
    // Selecting rows above immediately draws the corresponding curves below.
    right->addWidget(curveHost_);
    right->setChildrenCollapsible(false);
    right->setStretchFactor(0, 0);
    right->setStretchFactor(1, 1);
    right->setSizes({160, 440});

    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({260, 900});
    contentLayout_ = new QHBoxLayout;
    contentLayout_->addWidget(split, 1);
    outer->addLayout(contentLayout_, 1);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("resultDataStatus"));
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    outer->addWidget(status_);

    updateStatus();
}

void ResultDataTablePage::setDataSource(ResultDataSource *source)
{
    source_ = source;
    // The filter comes from the reader, so the dialog offers exactly the
    // formats that can actually be read in this build.
    openButton_->setEnabled(source_ != nullptr);
    if (source_ == nullptr) {
        sourceLabel_->setText(QStringLiteral(u"当前构建未启用结果读取器，无法打开文件。"));
    } else if (table_.isEmpty()) {
        sourceLabel_->setText(QStringLiteral(u"尚未打开结果文件。"));
    }
}

bool ResultDataTablePage::setCurveView(ResultCurveView *view)
{
    // Same three refusals as ComputeMonitorPage::insertCurveWidget: one slot, a
    // real widget, and GUI-thread affinity.
    if (curve_ != nullptr || view == nullptr || view->thread() != thread())
        return false;
    if (view->isAncestorOf(this) || isAncestorOf(view))
        return false;

    curve_ = view;
    curveHint_->hide();
    curveLayout_->addWidget(curve_, 1);
    if (auto *panel = curve_->settingsPanel()) {
        contentLayout_->addWidget(panel);
        panel->show();
    }
    // The hint and the view are siblings under curveHost_, and Qt destroys them
    // in child order -- hint first, view second. Restoring the hint from the
    // view's destruction signal would therefore poke a label that is already
    // gone. A QPointer goes null with the label, so the callback stays safe
    // whether the view goes first or last.
    const QPointer<QLabel> hintGuard(curveHint_);
    connect(curve_, &QObject::destroyed, this, [this, hintGuard]() {
        curve_ = nullptr;
        if (hintGuard)
            hintGuard->show();
    });
    updateCurveView();
    return true;
}

bool ResultDataTablePage::loadFile(const QString &path, QString *error)
{
    if (source_ == nullptr) {
        if (error)
            *error = QStringLiteral(u"当前构建未启用结果读取器，无法打开文件。");
        return false;
    }

    ResultTable loaded;
    QString message;
    if (!source_->load(path, &loaded, &message)) {
        if (error)
            *error = message;
        return false;
    }
    if (loaded.isEmpty()) {
        if (error)
            *error = QStringLiteral(u"文件里没有可用的采样数据：%1").arg(path);
        return false;
    }
    if (error)
        error->clear();

    table_ = loaded;
    sourceLabel_->setText(path);
    axisLabelView_->setText(table_.axisLabel.isEmpty()
                                ? QString()
                                : QStringLiteral(u"横轴："));
    populateTree();
    populateAxes();
    {
        const QSignalBlocker blocker(tableWidget_);
        tableWidget_->clearSelection();
    }
    populateTable();
    updateCurveView();
    updateStatus();
    emit fileLoaded(path);
    return true;
}

void ResultDataTablePage::clearData()
{
    const QSignalBlocker tableBlocker(tableWidget_);
    table_.clear();
    {
        const QSignalBlocker blocker(tree_);
        tree_->clear();
    }
    tableWidget_->clear();
    tableWidget_->setRowCount(0);
    tableWidget_->setColumnCount(0);
    sourceLabel_->setText(QStringLiteral(u"尚未打开结果文件。"));
    axisLabelView_->clear();
    populateAxes();
    if (curve_ != nullptr)
        curve_->clearSeries();
    updateStatus();
}

QStringList ResultDataTablePage::selectedKeys() const
{
    QStringList keys;
    for (int group = 0; group < tree_->topLevelItemCount(); ++group) {
        QTreeWidgetItem *parent = tree_->topLevelItem(group);
        for (int row = 0; row < parent->childCount(); ++row) {
            QTreeWidgetItem *child = parent->child(row);
            if (child->checkState(0) == Qt::Checked)
                keys.append(child->data(0, Qt::UserRole).toString());
        }
    }
    return keys;
}

void ResultDataTablePage::populateTree()
{
    const QSignalBlocker blocker(tree_);
    tree_->clear();
    if (table_.isEmpty())
        return;

    // Two groups only: residuals and everything else. Splitting the coefficients
    // further would encode a judgement about which numbers belong together that
    // the file itself does not make.
    QTreeWidgetItem *residuals = nullptr;
    QTreeWidgetItem *others = nullptr;
    for (const ResultVariable &variable : table_.variables) {
        QTreeWidgetItem *&group = variable.isResidual ? residuals : others;
        if (group == nullptr) {
            group = new QTreeWidgetItem(tree_);
            group->setText(0, variable.isResidual ? QStringLiteral(u"残差")
                                                  : QStringLiteral(u"变量"));
            group->setFlags(group->flags() & ~Qt::ItemIsUserCheckable);
            group->setExpanded(true);
        }
        auto *item = new QTreeWidgetItem(group);
        item->setText(0, variable.key);
        item->setText(1, variable.unit);
        item->setData(0, Qt::UserRole, variable.key);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        // Everything is ticked on open: a reader that starts with nothing shown
        // looks broken. Residuals stay off only when the file mixes scales --
        // they are handled by their own axis anyway, so tick them too.
        item->setCheckState(0, Qt::Checked);
    }
}

void ResultDataTablePage::populateTable()
{
    const QStringList previous = curveKeys();
    const QSignalBlocker blocker(tableWidget_);
    const QStringList keys = selectedKeys();
    if (keys.isEmpty() || table_.isEmpty()) {
        tableWidget_->clear();
        tableWidget_->setRowCount(0);
        tableWidget_->setColumnCount(0);
        return;
    }

    // One row per variable, one column per sample. The file is the transpose of
    // this, and that transpose is the whole point of the page: a variable's
    // entire trace reads left to right on a single line.
    const int samples = qMin(table_.sampleCount(), kMaxSampleColumns);
    QStringList headers;
    headers.reserve(samples + 1);
    headers << QStringLiteral(u"变量");
    for (int i = 0; i < samples; ++i)
        headers << QString::number(table_.axisValues.at(i), 'g', 6);

    tableWidget_->clear();
    tableWidget_->setColumnCount(headers.size());
    tableWidget_->setRowCount(keys.size());
    tableWidget_->setHorizontalHeaderLabels(headers);
    tableWidget_->verticalHeader()->setVisible(true);
    tableWidget_->verticalHeader()->setDefaultSectionSize(22);

    const int dataColumns = headers.size() - 1;
    for (int row = 0; row < keys.size(); ++row) {
        const QString &key = keys.at(row);
        tableWidget_->setItem(row, 0, readOnlyItem(key));
        const QVector<double> values = table_.samples.value(key);
        for (int column = 0; column < dataColumns; ++column) {
            tableWidget_->setItem(row, column + 1,
                                  readOnlyItem(numberText(values.value(column, std::numeric_limits<double>::quiet_NaN()))));
        }
    }
    for (int row = 0; row < keys.size(); ++row) {
        if (previous.contains(keys.at(row)))
            tableWidget_->setRangeSelected(
                QTableWidgetSelectionRange(row, 0, row, headers.size() - 1), true);
    }
    tableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    tableWidget_->horizontalHeader()->resizeSection(0, 180);
}

void ResultDataTablePage::populateAxes()
{
    const QSignalBlocker blocker(xAxis_);
    xAxis_->clear();
    xAxis_->setEnabled(!table_.isEmpty());
    if (table_.isEmpty())
        return;
    xAxis_->addItem(table_.axisLabel);
    for (const ResultAxis &axis : table_.alternativeAxes)
        xAxis_->addItem(axis.label);
    for (const ResultVariable &variable : table_.variables)
        xAxis_->addItem(variable.unit.isEmpty() ? variable.key
            : QStringLiteral("%1 (%2)").arg(variable.key, variable.unit));
}

QStringList ResultDataTablePage::curveKeys() const
{
    QStringList keys;
    for (int row = 0; row < tableWidget_->rowCount(); ++row) {
        const auto *item = tableWidget_->item(row, 0);
        if (item && item->isSelected())
            keys.append(item->text());
    }
    return keys;
}

void ResultDataTablePage::updateCurveView()
{
    if (curve_ == nullptr)
        return;
    const QStringList keys = curveKeys();
    if (keys.isEmpty() || table_.isEmpty()) {
        curve_->clearSeries();
        return;
    }
    // Qt containers share storage; changing X does not mutate the source table.
    ResultTable projected = table_;
    const int index = xAxis_->currentIndex();
    if (index > 0 && index <= table_.alternativeAxes.size()) {
        projected.axisValues = table_.alternativeAxes.at(index - 1).values;
    } else if (index > table_.alternativeAxes.size()) {
        const int variable = index - table_.alternativeAxes.size() - 1;
        if (variable < table_.variables.size())
            projected.axisValues = table_.samples.value(table_.variables.at(variable).key);
    }
    projected.axisLabel = xAxis_->currentText();
    curve_->replaceSeries(projected, keys, projected.axisLabel);
}

void ResultDataTablePage::updateStatus()
{
    if (table_.isEmpty()) {
        status_->setText(QStringLiteral(u"打开一个结果文件后，这里按「每行一个变量」列出采样值。"));
        return;
    }

    QString text = QStringLiteral(u"%1 个变量 · %2 个采样点 · 当前勾选 %3 个 · 选中 %4 行绘图")
                       .arg(table_.variables.size())
                       .arg(table_.sampleCount())
                       .arg(selectedKeys().size())
                       .arg(curveKeys().size());
    if (curveKeys().isEmpty())
        text += QStringLiteral(u" · 请选择表格行以绘图，Ctrl/Shift 可多选");
    if (table_.sampleCount() > kMaxSampleColumns) {
        text += QStringLiteral(u" · 表格只显示前 %1 个采样点")
                    .arg(kMaxSampleColumns);
    }
    if (!table_.warnings.isEmpty())
        text += QStringLiteral(u" · %1").arg(table_.warnings.join(QStringLiteral("；")));
    status_->setText(text);
}

void ResultDataTablePage::chooseFile()
{
    if (source_ == nullptr)
        return;
    const QString filter = source_->fileFilter();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral(u"打开结果文件"), QString(), filter);
    if (path.isEmpty())
        return;
    QString error;
    if (!loadFile(path, &error))
        sourceLabel_->setText(error);
}

} // namespace aerosim
