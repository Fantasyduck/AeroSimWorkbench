#include "aerosim/ui/CalculationTaskPage.h"
#include "aerosim/ui/UiLayoutConfig.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>
#include <cstddef>

namespace aerosim {

template <std::size_t N>
QString uiText(const char16_t (&value)[N])
{
    return QString::fromUtf16(reinterpret_cast<const ushort *>(value), int(N) - 1);
}
namespace {
// Empty axes only: no generated samples or curve algorithm. 邱's monitor
// widget replaces this canvas through the existing WidgetSlot::Monitor host.
class EmptyMonitorChart final : public QWidget {
public:
    explicit EmptyMonitorChart(QWidget *parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("calculationEmptyChart"));
        setMinimumSize(190, 145);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAccessibleName(uiText(u"\u76d1\u63a7\u56fe\u50cf\uff0c\u6682\u65e0\u6570\u636e"));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().brush(QPalette::Base));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(palette().color(QPalette::Text));
        const QRectF chart(42, 20, qMax(80, width() - 60), qMax(50, height() - 65));
        painter.drawLine(chart.bottomLeft(), chart.bottomRight());
        painter.drawLine(chart.bottomLeft(), chart.topLeft());
        for (int i = 0; i <= 4; ++i) {
            const qreal x = chart.left() + chart.width() * i / 4;
            const qreal y = chart.bottom() - chart.height() * i / 4;
            painter.drawLine(QPointF(x, chart.bottom()), QPointF(x, chart.bottom() - 4));
            painter.drawLine(QPointF(chart.left(), y), QPointF(chart.left() + 4, y));
            painter.drawText(QRectF(x - 15, chart.bottom() + 5, 30, 18), Qt::AlignCenter, QString::number(i));
            painter.drawText(QRectF(3, y - 10, 30, 20), Qt::AlignRight | Qt::AlignVCenter, QString::number(i));
        }
        painter.drawText(QRectF(chart.left(), height() - 23, chart.width(), 20), Qt::AlignCenter, QStringLiteral("Iteration Steps"));
        painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        painter.drawText(chart, Qt::AlignCenter, uiText(u"\u6682\u65e0\u76d1\u63a7\u6570\u636e"));
    }
};

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("calculationSection", true);
    label->setMinimumHeight(uimetrics::sectionTitleHeight());
    label->setMargin(6);
    return label;
}

double splitRatio(QSplitter *splitter)
{
    const auto sizes = splitter->sizes();
    const int total = sizes.value(0) + sizes.value(1);
    return total > 0 ? double(sizes[0]) / total : 0.5;
}

void restoreRatio(QSplitter *splitter, double ratio)
{
    if (!std::isfinite(ratio)) ratio = 0.5;
    ratio = qBound(0.15, ratio, 0.85);
    splitter->setSizes({qRound(1000 * ratio), qRound(1000 * (1 - ratio))});
}
}

CalculationTaskPage::CalculationTaskPage(bool multiple, QWidget *parent)
    : QWidget(parent), multiple_(multiple)
{
    setObjectName(multiple ? QStringLiteral("multipleTaskPage") : QStringLiteral("singleTaskPage"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral(
        "QLabel[calculationSection=\"true\"] { background: palette(highlight); color: palette(highlighted-text); }"
        "QGroupBox { border: 1px solid palette(mid); border-radius: 3px; margin-top: 12px; padding: 10px 7px 7px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; padding: 0 5px; }"
        "QFrame#singleExecutionCard { background: palette(base); border: 1px solid palette(midlight); border-radius: 6px; }"
        "QLabel#executionHeading { font-size: 16px; font-weight: bold; background: transparent; }"
        "QLabel[calculationHint=\"true\"] { color: palette(disabled, text); background: transparent; }"
        "QPushButton#submitCalculation { background: #2563a6; color: white; border: 1px solid #2563a6; border-radius: 4px; padding: 6px 10px; }"
        "QPushButton#submitCalculation:hover { background: #3475b8; }"
        "QComboBox#submissionModeCombo { min-width: 112px; max-width: 130px; }"
        "QSplitter::handle { background: palette(midlight); }"
        "QSplitter::handle:horizontal { width: 5px; }"
        "QSplitter::handle:vertical { height: 5px; }"
        "QSplitter::handle:hover { background: palette(highlight); }"
        "QPushButton[taskCategory=\"true\"] { padding: 5px 3px; border-radius: 3px; }"
        "QLabel#calculationUiNotice { color: palette(mid); padding: 2px 6px; }"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    verticalSplitter_ = new QSplitter(Qt::Vertical, this);
    verticalSplitter_->setObjectName(QStringLiteral("calculationVerticalSplitter"));
    verticalSplitter_->setChildrenCollapsible(false);
    mainSplitter_ = new QSplitter(Qt::Horizontal, verticalSplitter_);
    mainSplitter_->setObjectName(QStringLiteral("calculationMainSplitter"));
    mainSplitter_->setChildrenCollapsible(false);
    auto *settings = new QWidget(mainSplitter_);
    settings->setObjectName(QStringLiteral("calculationSettingsPanel"));
    settings->setMinimumWidth(multiple ? 340 : 280);
    auto *settingsLayout = new QVBoxLayout(settings);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(0);
    settingsLayout->addWidget(sectionTitle(uiText(u"\u8bbe\u7f6e\u9762\u677f"), settings));
    auto *settingsScroll = new QScrollArea(settings);
    settingsScroll->setObjectName(QStringLiteral("calculationSettingsScroll"));
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setFrameShape(QFrame::NoFrame);
    settingsScroll->setWidget(multiple ? createMultipleSettings() : createSingleSettings());
    settingsLayout->addWidget(settingsScroll);

    auto *monitor = new QWidget(mainSplitter_);
    monitor->setMinimumWidth(330);
    auto *monitorLayout = new QVBoxLayout(monitor);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->setSpacing(0);
    auto *tabs = new QWidget(monitor);
    tabs->setObjectName(QStringLiteral("calculationViewTabs"));
    auto *tabLayout = new QHBoxLayout(tabs);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    auto *tab = new QPushButton(multiple ? uiText(u"\u591a\u5de5\u51b5") : uiText(u"\u5355\u5de5\u51b5"), tabs);
    tab->setObjectName(QStringLiteral("calculationConditionTab"));
    tab->setCheckable(true); tab->setChecked(true);
    connect(tab, &QPushButton::clicked, this, [tab] { tab->setChecked(true); });
    tabLayout->addWidget(tab);
    tabLayout->addStretch();
    monitorLayout->addWidget(tabs);
    monitorSplitter_ = new QSplitter(Qt::Horizontal, monitor);
    monitorSplitter_->setObjectName(QStringLiteral("calculationMonitorSplitter"));
    monitorSplitter_->setChildrenCollapsible(false);
    auto *observations = new QWidget(monitorSplitter_);
    observations->setMinimumWidth(130);
    auto *observationLayout = new QVBoxLayout(observations);
    observationLayout->setContentsMargins(8, 6, 3, 5);
    observationLayout->addWidget(createObservables(), 1);
    if (multiple) {
        auto *samples = new QCheckBox(uiText(u"\u4ec5\u663e\u793a\u6837\u672c\u70b9"), observations);
        samples->setObjectName(QStringLiteral("calculationSamplesOnly"));
        samples->setToolTip(uiText(u"\u9009\u62e9\u4fdd\u7559\u5728\u672c\u9875\uff0c\u63a5\u5165\u66f2\u7ebf\u6570\u636e\u540e\u751f\u6548"));
        observationLayout->addWidget(samples);
    }
    auto *chartPanel = new QWidget(monitorSplitter_);
    chartPanel->setObjectName(QStringLiteral("calculationChartPanel"));
    curveLayout_ = new QVBoxLayout(chartPanel);
    curveLayout_->setContentsMargins(0, 0, 0, 0);
    emptyChart_ = new EmptyMonitorChart(chartPanel);
    curveLayout_->addWidget(emptyChart_);
    monitorLayout->addWidget(monitorSplitter_, 1);

    outputSplitter_ = new QSplitter(Qt::Horizontal, verticalSplitter_);
    outputSplitter_->setObjectName(QStringLiteral("calculationOutputSplitter"));
    outputSplitter_->setChildrenCollapsible(false);
    const auto createOutput = [this](const QString &name, const QString &title,
                                      const QString &placeholder) {
        auto *panel = new QWidget(outputSplitter_);
        panel->setObjectName(name + QStringLiteral("Panel"));
        panel->setMinimumSize(200, 115);
        auto *panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(0, 0, 0, 0);
        panelLayout->setSpacing(0);
        panelLayout->addWidget(sectionTitle(title, panel));
        auto *editor = new QPlainTextEdit(panel);
        editor->setObjectName(name);
        editor->setReadOnly(true);
        editor->setMaximumBlockCount(2000);
        editor->setPlaceholderText(placeholder);
        panelLayout->addWidget(editor);
        return editor;
    };
    history_ = createOutput(QStringLiteral("calculationRunHistory"), uiText(u"运行历史"),
        uiText(u"暂无操作记录"));
    solverOutput_ = createOutput(QStringLiteral("calculationSolverOutput"), uiText(u"解算器输出"),
        uiText(u"暂无解算器输出"));
    solverOutput_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    mainSplitter_->setStretchFactor(0, 0);
    mainSplitter_->setStretchFactor(1, 1);
    outputSplitter_->setStretchFactor(0, 0);
    outputSplitter_->setStretchFactor(1, 1);
    monitorSplitter_->setStretchFactor(0, 0);
    monitorSplitter_->setStretchFactor(1, 1);
    layout->addWidget(verticalSplitter_, 1);
    auto *notice = new QLabel(uiText(u"\u754c\u9762\u9884\u89c8 \u00b7 \u5c1a\u672a\u63a5\u5165\u8ba1\u7b97\u4e0e\u6587\u4ef6\u751f\u6210"), this);
    notice->setObjectName(QStringLiteral("calculationUiNotice"));
    layout->addWidget(notice);
    restoreDefaultLayout();
}

QWidget *CalculationTaskPage::createSingleSettings()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(uimetrics::panelSpacing(), uimetrics::panelSpacing(), uimetrics::panelSpacing(), uimetrics::panelSpacing());
    layout->setSpacing(uimetrics::panelSpacing());
    auto *execution = new QGroupBox(uiText(u"\u6267\u884c\u65b9\u5f0f"), panel);
    auto *executionLayout = new QHBoxLayout(execution);
    executionLayout->addWidget(new QLabel(uiText(u"\u8fd0\u884c\u6a21\u5f0f\uff1a"), execution));
    auto *window = new QRadioButton(QStringLiteral("Window"), execution);
    window->setObjectName(QStringLiteral("singleTaskWindowMode")); window->setChecked(true);
    auto *console = new QRadioButton(QStringLiteral("Console"), execution);
    console->setObjectName(QStringLiteral("singleTaskConsoleMode"));
    executionLayout->addWidget(window); executionLayout->addWidget(console); executionLayout->addStretch();
    layout->addWidget(execution);
    connect(window, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) appendLog(uiText(u"已选择 Window 执行模式。"));
    });
    connect(console, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) appendLog(uiText(u"已选择 Console 执行模式。"));
    });
    auto *hint = new QLabel(uiText(u"选择运行窗口和提交方式后，可直接提交当前工况。"), panel);
    hint->setProperty("calculationHint", true);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addWidget(createSubmissionControls());
    layout->addSpacing(10);
    layout->addStretch();
    return panel;
}

QWidget *CalculationTaskPage::createMultipleSettings()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 10);
    auto *group = new QGroupBox(uiText(u"\u591a\u72b6\u6001"), panel);
    auto *groupLayout = new QVBoxLayout(group);
    table_ = new QTableWidget(0, 8, group);
    table_->setObjectName(QStringLiteral("multipleTaskTable"));
    table_->setHorizontalHeaderLabels({uiText(u"\u5e8f\u53f7"), uiText(u"\u8ba1\u7b97\u72b6\u6001"), uiText(u"\u7eed\u7b97"),
        uiText(u"\u8fed\u4ee3\u6b65\u6570"), uiText(u"\u53c2\u8003\u4fe1\u606f"), uiText(u"\u9a6c\u8d6b\u6570"), uiText(u"\u653b\u89d2"), uiText(u"\u4fa7\u6ed1\u89d2")});
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setMinimumHeight(185);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setMinimumSectionSize(40);
    const int widths[] = {42, 62, 38, 58, 95, 52, 48, 52};
    for (int col = 0; col < 8; ++col) table_->setColumnWidth(col, widths[col]);
    groupLayout->addWidget(table_, 1);
    auto *types = new QGroupBox(uiText(u"\u53ef\u9009\u7c7b\u522b"), group);
    types->setObjectName(QStringLiteral("taskCategoryGroup"));
    auto *typeLayout = new QGridLayout(types);
    typeLayout->setContentsMargins(0, 4, 0, 4);
    const CalculationInputKind kinds[] = {CalculationInputKind::ReynoldsTemperature,
        CalculationInputKind::PressureTemperature, CalculationInputKind::TotalPressureTemperature,
        CalculationInputKind::Altitude, CalculationInputKind::Trajectory};
    for (int i = 0; i < 5; ++i) {
        const auto kind = kinds[i];
        auto *button = new QPushButton(calculationInputTitle(kind), types);
        button->setObjectName(QStringLiteral("addTaskCategory_%1").arg(i));
        button->setProperty("taskCategory", true);
        button->setToolTip(uiText(u"\u6dfb\u52a0%1\u4efb\u52a1").arg(calculationInputTitle(kind)));
        connect(button, &QPushButton::clicked, this, [this, kind] { addTasks(kind); });
        // Two rows keep the full category names legible on laptop displays.
        typeLayout->addWidget(button, i / 3, i % 3);
    }
    groupLayout->addWidget(types);
    auto *commands = new QHBoxLayout;
    selectAll_ = new QCheckBox(uiText(u"\u5168\u9009"), group);
    selectAll_->setObjectName(QStringLiteral("selectAllTasks"));
    commands->addWidget(selectAll_);
    commands->addStretch();
    remove_ = new QPushButton(uiText(u"\u2212 \u5220\u9664\u9009\u4e2d"), group);
    remove_->setObjectName(QStringLiteral("removeSelectedTasks"));
    remove_->setToolTip(uiText(u"仅删除序号列复选框已勾选的任务"));
    commands->addWidget(remove_);
    groupLayout->addLayout(commands);
    count_ = new QLabel(group);
    count_->setObjectName(QStringLiteral("multipleTaskCount"));
    groupLayout->addWidget(count_);
    layout->addWidget(group, 1);
    layout->addWidget(createSubmissionControls());
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (col != 0 && col != 2 && col != 3) editTask(row);
    });
    connect(table_, &QTableWidget::itemChanged, this, [this] { updateSelection(); });
    connect(table_, &QTableWidget::itemSelectionChanged, this, &CalculationTaskPage::updateSelection);
    connect(selectAll_, &QCheckBox::clicked, this, [this](bool checked) {
        const QSignalBlocker blocker(table_);
        for (int row = 0; row < table_->rowCount(); ++row)
            table_->item(row, 0)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        updateSelection();
        appendLog(checked ? uiText(u"已全选任务。") : uiText(u"已取消全选任务。"));
    });
    connect(remove_, &QPushButton::clicked, this, &CalculationTaskPage::removeSelected);
    updateSelection();
    return panel;
}

QWidget *CalculationTaskPage::createSubmissionControls()
{
    auto *widget = new QWidget(this);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 5, 0, 10);
    if (!multiple_) {
        resume_ = new QCheckBox(uiText(u"\u7eed\u7b97"), widget);
        resume_->setObjectName(QStringLiteral("singleTaskResume"));
        resume_->setAccessibleName(uiText(u"\u7eed\u7b97"));
        resume_->setToolTip(uiText(u"在已有结果基础上继续计算"));
        layout->addWidget(resume_);
        connect(resume_, &QCheckBox::toggled, this, [this](bool checked) {
            appendLog(checked ? uiText(u"已开启续算。") : uiText(u"已关闭续算。"));
        });
    }
    auto *modeLabel = new QLabel(uiText(u"输出："), widget);
    layout->addWidget(modeLabel);
    auto *mode = new QComboBox(widget);
    mode->setObjectName(QStringLiteral("submissionModeCombo"));
    mode->addItems({uiText(u"提交本地计算"), uiText(u"仅生成 xdb 文件"), uiText(u"仅生成计算压缩包")});
    mode->setToolTip(uiText(u"选择提交计算时的输出方式"));
    layout->addWidget(mode);
    auto *submit = new QPushButton(uiText(u"提交计算"), widget);
    submit->setObjectName(QStringLiteral("submitCalculation"));
    submit->setToolTip(uiText(u"当前仅为界面预览，尚未接入真实计算"));
    layout->addWidget(submit, 0, Qt::AlignLeft);
    layout->addStretch();
    connect(mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, mode](int) {
        appendLog(uiText(u"已选择提交方式：%1。 ").arg(mode->currentText()));
    });
    connect(submit, &QPushButton::clicked, this, [this, mode] {
        if (multiple_) {
            int checked = 0;
            for (int row = 0; row < table_->rowCount(); ++row)
                checked += table_->item(row, 0)->checkState() == Qt::Checked;
            if (!checked) { appendLog(uiText(u"\u8bf7\u5148\u6dfb\u52a0\u5e76\u52fe\u9009\u9700\u8981\u63d0\u4ea4\u7684\u4efb\u52a1\u3002")); return; }
        }
        appendLog(uiText(u"已提交计算（%1）。 ").arg(mode->currentText()));
    });
    return widget;
}

QWidget *CalculationTaskPage::createObservables()
{
    auto *group = new QGroupBox(uiText(u"\u89c2\u6d4b\u91cf"), this);
    group->setObjectName(QStringLiteral("calculationObservables"));
    group->setMinimumSize(100, 70);
    auto *layout = new QVBoxLayout(group);
    auto *placeholder = new QLabel(uiText(u"\u6682\u65e0\u89c2\u6d4b\u91cf"), group);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setEnabled(false);
    layout->addWidget(placeholder);
    return group;
}

void CalculationTaskPage::addTasks(CalculationInputKind kind)
{
    CalculationTaskInputDialog dialog(kind, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto newRows = dialog.rows();
    if (rows_.size() + newRows.size() > 1000) {
        appendLog(uiText(u"\u672c\u9875\u6700\u591a\u4fdd\u7559 1000 \u4e2a\u4efb\u52a1\uff0c\u8bf7\u5220\u9664\u90e8\u5206\u4efb\u52a1\u540e\u518d\u6dfb\u52a0\u3002")); return;
    }
    const int first = rows_.size();
    for (const auto &values : newRows) {
        rows_.append({kind, values, nextId_++});
        table_->insertRow(table_->rowCount());
        fillRow(table_->rowCount() - 1);
    }
    table_->selectRow(first);
    updateSelection();
    appendLog(uiText(u"\u5df2\u6dfb\u52a0 %1 \u4e2a\u201c%2\u201d\u4efb\u52a1\uff0c\u5c1a\u672a\u63d0\u4ea4\u3002 ").arg(newRows.size()).arg(calculationInputTitle(kind)));
}

void CalculationTaskPage::editTask(int row)
{
    if (row < 0 || row >= rows_.size()) return;
    CalculationTaskInputDialog dialog(rows_[row].kind, this);
    dialog.setValues(rows_[row].values);
    if (dialog.exec() != QDialog::Accepted) return;
    rows_[row].values = dialog.rows().first();
    fillRow(row); updateSelection();
    appendLog(uiText(u"\u5df2\u66f4\u65b0\u4efb\u52a1 %1\u3002 ").arg(rows_[row].id));
}

void CalculationTaskPage::fillRow(int row)
{
    const QSignalBlocker blocker(table_);
    const auto &entry = rows_[row];
    const auto labels = calculationInputLabels(entry.kind);
    const int references = entry.values.size() - 3;
    QStringList info;
    for (int i = 0; i < references; ++i)
        info << labels[i] + QStringLiteral(": ") + QString::number(entry.values[i], 'g', 10);
    const QStringList texts = {QString::number(entry.id), uiText(u"\u5f85\u8ba1\u7b97"), QString(), QString(), info.join('\n'),
        QString::number(entry.values[references], 'g', 10), QString::number(entry.values[references + 1], 'g', 10),
        QString::number(entry.values[references + 2], 'g', 10)};
    for (int col = 0; col < texts.size(); ++col) {
        auto *item = table_->item(row, col);
        if (!item) {
            item = new QTableWidgetItem;
            table_->setItem(row, col, item);
            if (col == 0 || col == 2) item->setCheckState(Qt::Unchecked);
        }
        item->setText(texts[col]);
        item->setToolTip(calculationInputTitle(entry.kind) + QStringLiteral("\n") + info.join('\n'));
    }
    if (!table_->cellWidget(row, 3)) {
        auto *iterations = new QSpinBox(table_);
        iterations->setRange(1, 100000000); iterations->setValue(100);
        iterations->setFrame(false);
        table_->setCellWidget(row, 3, iterations);
    }
    table_->setRowHeight(row, references > 1 ? 52 : 36);
}

void CalculationTaskPage::updateSelection()
{
    int checked = 0;
    for (int row = 0; row < table_->rowCount(); ++row)
        if (table_->item(row, 0) && table_->item(row, 0)->checkState() == Qt::Checked) ++checked;
    const QSignalBlocker blocker(selectAll_);
    // Keep this binary: a partially checked square is easy to mistake for a
    // full selection.  Only every sequence checkbox makes “全选” checked.
    selectAll_->setTristate(false);
    selectAll_->setCheckState(checked > 0 && checked == table_->rowCount()
        ? Qt::Checked : Qt::Unchecked);
    selectAll_->setEnabled(table_->rowCount() > 0);
    // Row highlighting is only for browsing/editing.  Deletion is driven
    // exclusively by the checkbox in the sequence column.
    remove_->setEnabled(checked > 0);
    count_->setText(uiText(u"\u5171 %1 \u4e2a\u4efb\u52a1 \u00b7 \u5df2\u52fe\u9009 %2 \u4e2a").arg(table_->rowCount()).arg(checked));
}

void CalculationTaskPage::removeSelected()
{
    QVector<int> selected;
    for (int row = 0; row < table_->rowCount(); ++row)
        if (table_->item(row, 0)->checkState() == Qt::Checked) selected.append(row);
    for (int i = selected.size() - 1; i >= 0; --i) {
        rows_.removeAt(selected[i]); table_->removeRow(selected[i]);
    }
    updateSelection();
    appendLog(selected.isEmpty() ? uiText(u"未勾选任务，未执行删除。")
                                : uiText(u"已删除 %1 个任务。 ").arg(selected.size()));
}

void CalculationTaskPage::attachMonitor(QWidget *widget)
{
    if (!widget) return;
    emptyChart_->hide();
    curveLayout_->addWidget(widget);
    widget->show();
}

void CalculationTaskPage::appendLog(const QString &message)
{
    if (!history_) return;
    history_->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("[yyyy-MM-dd HH:mm:ss] ")) + message);
}

void CalculationTaskPage::appendSolverOutput(const QString &message)
{
    if (!solverOutput_) return;
    solverOutput_->appendPlainText(message);
}

QVariantMap CalculationTaskPage::layoutState() const
{
    return {{QStringLiteral("settingsRatio"), splitRatio(mainSplitter_)},
        {QStringLiteral("monitorRatio"), splitRatio(monitorSplitter_)},
        {QStringLiteral("outputRatio"), splitRatio(outputSplitter_)},
        {QStringLiteral("verticalRatio"), splitRatio(verticalSplitter_)}};
}

void CalculationTaskPage::restoreLayoutState(const QVariantMap &state)
{
    restoreRatio(mainSplitter_, state.value(QStringLiteral("settingsRatio"), 0.46).toDouble());
    restoreRatio(monitorSplitter_, state.value(QStringLiteral("monitorRatio"), 0.32).toDouble());
    restoreRatio(outputSplitter_, state.value(QStringLiteral("outputRatio"), 0.50).toDouble());
    restoreRatio(verticalSplitter_, state.value(QStringLiteral("verticalRatio"), 0.67).toDouble());
}

void CalculationTaskPage::restoreDefaultLayout()
{
    verticalSplitter_->setSizes({520, 270});
    mainSplitter_->setSizes({multiple_ ? 560 : 390, multiple_ ? 760 : 930});
    monitorSplitter_->setSizes({multiple_ ? 250 : 230, 600});
    outputSplitter_->setSizes({600, 600});
}

} // namespace aerosim
