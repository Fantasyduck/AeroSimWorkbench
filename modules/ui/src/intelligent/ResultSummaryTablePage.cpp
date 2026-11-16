#include "aerosim/ui/intelligent/ResultSummaryTablePage.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

namespace aerosim {
namespace {

const QString kAllWall = QStringLiteral("ALL_WALL");
const QString kMissing = QStringLiteral(u"—");

// YiHuo case names encode the freestream condition and the flow angles.
const QString kCaseNumber = QStringLiteral(R"([+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?)");
const QRegularExpression kCaseCondition(QStringLiteral(
    R"((?:^|_)Ma(%1)(h%1|Re%1t%1|tt%1pt%1|t%1p%1)?a(%1)b(%1)$)").arg(kCaseNumber),
    QRegularExpression::CaseInsensitiveOption);
const QRegularExpression kReferenceField(QStringLiteral("(h|Re|tt|pt|t|p)(%1)").arg(kCaseNumber),
    QRegularExpression::CaseInsensitiveOption);

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

ResultSummaryTablePage::ResultSummaryTablePage(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("resultSummaryTablePage"));
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    buildUi();
}

void ResultSummaryTablePage::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);

    auto *bar = new QHBoxLayout;
    auto *forceButton = new QPushButton(QStringLiteral(u"打开 CSV…"), this);
    forceButton->setObjectName(QStringLiteral("resultSummaryCsvOpenButton"));
    connect(forceButton, &QPushButton::clicked, this, [this] {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this, QStringLiteral(u"打开结果 CSV"), QString(),
            QStringLiteral("CSV (*.csv);;所有文件 (*)"));
        if (paths.isEmpty())
            return;
        QString error;
        if (!loadCsvFiles(paths, &error))
            status_->setText(error);
    });
    bar->addWidget(forceButton);

    openButton_ = new QPushButton(QStringLiteral(u"打开历史文件…"), this);
    openButton_->setObjectName(QStringLiteral("resultSummaryOpenButton"));
    openButton_->setEnabled(false);
    connect(openButton_, &QPushButton::clicked, this, &ResultSummaryTablePage::chooseFile);
    bar->addWidget(openButton_);

    sourceLabel_ = new QLabel(QStringLiteral(u"尚未打开结果文件。"), this);
    sourceLabel_->setObjectName(QStringLiteral("resultSummarySourceLabel"));
    sourceLabel_->setTextFormat(Qt::PlainText);
    bar->addWidget(sourceLabel_, 1);
    forceControls_ = new QWidget(this);
    auto *forceOptions = new QHBoxLayout(forceControls_);
    forceOptions->setContentsMargins(0, 0, 0, 0);
    csvView_ = new QComboBox(this);
    csvView_->setObjectName(QStringLiteral("resultSummaryCsvView"));
    csvView_->addItems({QStringLiteral(u"数据明细"), QStringLiteral(u"按壁面对比")});
    csvView_->hide();
    bar->addWidget(csvView_);
    connect(csvView_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ResultSummaryTablePage::showCsvView);
    auto *coefficientLabel = new QLabel(QStringLiteral(u"字段"), forceControls_);
    coefficient_ = new QComboBox(forceControls_);
    coefficient_->setObjectName(QStringLiteral("forceSummaryCoefficient"));
    coefficient_->setAccessibleName(coefficientLabel->text());
    coefficientLabel->setBuddy(coefficient_);
    forceOptions->addWidget(coefficientLabel);
    forceOptions->addWidget(coefficient_);
    bar->addWidget(forceControls_);
    forceControls_->hide();
    connect(coefficient_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ResultSummaryTablePage::populateForceTable);
    outer->addLayout(bar);

    tableWidget_ = new QTableWidget(this);
    tableWidget_->setObjectName(QStringLiteral("resultSummaryTable"));
    tableWidget_->verticalHeader()->hide();
    tableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableWidget_->setAlternatingRowColors(true);
    outer->addWidget(tableWidget_, 1);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("resultSummaryStatus"));
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    outer->addWidget(status_);

    updateStatus();
}

void ResultSummaryTablePage::setDataSource(ResultDataSource *source)
{
    source_ = source;
    openButton_->setEnabled(source_ != nullptr);
}

bool ResultSummaryTablePage::loadFile(const QString &path, QString *error)
{
    if (path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        return loadCsvFiles({path}, error);
    return loadHistoryFile(path, error);
}

bool ResultSummaryTablePage::loadHistoryFile(const QString &path, QString *error)
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

    forceCases_.clear();
    forceWalls_.clear();
    csvTables_.clear();
    csvColumns_.clear();
    comparisonFields_.clear();
    csvView_->hide();
    forceControls_->hide();
    table_ = loaded;
    sourceLabel_->setText(path);
    sourceLabel_->setToolTip(QString());
    populateTable();
    updateStatus();
    emit fileLoaded(path);
    return true;
}

bool ResultSummaryTablePage::loadForceFiles(const QStringList &paths, QString *error)
{
    return loadCsvFiles(paths, error);
}

bool ResultSummaryTablePage::loadCsvFiles(const QStringList &paths, QString *error)
{
    if (paths.isEmpty()) {
        if (error)
            *error = QStringLiteral(u"请至少选择一个 CSV 文件。");
        return false;
    }
    QVector<CsvResultTable> tables;
    QStringList columns;
    QStringList columnKeys;
    for (const QString &path : paths) {
        CsvResultTable loaded;
        if (!CsvResultReader::read(path, &loaded, error))
            return false;
        for (const QString &column : loaded.columns) {
            const QString key = column.toCaseFolded();
            if (!columnKeys.contains(key)) {
                columns.append(column);
                columnKeys.append(key);
            }
        }
        tables.append(loaded);
    }

    // A wall comparison is available only when each record has a unique wall
    // within its file. Repeated walls (e.g. time histories) remain in detail view.
    QVector<ForceCase> cases;
    QStringList walls;
    QStringList fields;
    bool comparable = true;
    for (const CsvResultTable &csv : tables) {
        QStringList keys;
        for (const QString &column : csv.columns)
            keys.append(column.toCaseFolded());
        const int wallIndex = keys.indexOf(QStringLiteral("wallname"));
        if (wallIndex < 0) {
            comparable = false;
            break;
        }
        ForceCase result;
        result.name = QFileInfo(csv.sourcePath).completeBaseName();
        if (result.name.endsWith(QStringLiteral("_force"), Qt::CaseInsensitive))
            result.name.chop(6);
        const auto condition = kCaseCondition.match(result.name);
        if (condition.hasMatch()) {
            result.mach = condition.captured(1);
            result.alpha = condition.captured(3);
            result.beta = condition.captured(4);
            auto references = kReferenceField.globalMatch(condition.captured(2));
            QStringList reference;
            while (references.hasNext()) {
                const auto field = references.next();
                reference.append(QStringLiteral("%1:%2").arg(field.captured(1), field.captured(2)));
            }
            result.reference = reference.join(QStringLiteral(", "));
        }
        for (const QStringList &row : csv.rows) {
            QString wall = row.at(wallIndex).trimmed();
            if (wall.compare(kAllWall, Qt::CaseInsensitive) == 0)
                wall = kAllWall;
            if (wall.isEmpty() || result.valuesByWall.contains(wall)) {
                comparable = false;
                break;
            }
            QHash<QString, QString> values;
            for (int column = 0; column < keys.size(); ++column)
                values.insert(keys.at(column), row.at(column));
            result.valuesByWall.insert(wall, values);
            if (!walls.contains(wall))
                walls.append(wall);
        }
        if (!comparable)
            break;
        cases.append(result);
    }
    for (int column = 0; column < columns.size(); ++column) {
        if (columnKeys.at(column) != QStringLiteral("wallname")
            && columnKeys.at(column) != QStringLiteral("bc index"))
            fields.append(columns.at(column));
    }
    comparable = comparable && !fields.isEmpty();
    if (!comparable) {
        cases.clear();
        walls.clear();
        fields.clear();
    }
    if (walls.removeOne(kAllWall))
        walls.append(kAllWall);

    // Commit all parsed files together so a failed import preserves the view.
    const QString selectedField = coefficient_->currentText();
    table_.clear();
    csvTables_ = tables;
    csvColumns_ = columns;
    forceCases_ = cases;
    forceWalls_ = walls;
    comparisonFields_ = fields;
    {
        const QSignalBlocker blocker(coefficient_);
        coefficient_->clear();
        coefficient_->addItems(fields);
        const int previous = coefficient_->findText(selectedField, Qt::MatchFixedString);
        if (previous >= 0)
            coefficient_->setCurrentIndex(previous);
    }
    {
        const QSignalBlocker blocker(csvView_);
        csvView_->setCurrentIndex(comparable ? 1 : 0);
    }
    csvView_->setVisible(comparable);
    sourceLabel_->setText(tables.size() == 1 ? QFileInfo(paths.first()).fileName()
        : QStringLiteral(u"已导入 %1 个 CSV 文件").arg(tables.size()));
    sourceLabel_->setToolTip(paths.join(QLatin1Char('\n')));
    showCsvView();
    if (error)
        error->clear();
    for (const QString &path : paths)
        emit fileLoaded(path);
    return true;
}

void ResultSummaryTablePage::showCsvView()
{
    const bool comparison = !forceCases_.isEmpty() && csvView_->currentIndex() == 1;
    forceControls_->setVisible(comparison);
    if (comparison)
        populateForceTable();
    else
        populateCsvTable();
}

void ResultSummaryTablePage::populateCsvTable()
{
    int count = 0;
    for (const CsvResultTable &csv : csvTables_)
        count += csv.rows.size();
    tableWidget_->clear();
    tableWidget_->setRowCount(count);
    tableWidget_->setColumnCount(csvColumns_.size() + 1);
    tableWidget_->setHorizontalHeaderLabels(QStringList{QStringLiteral(u"来源文件")} + csvColumns_);
    int tableRow = 0;
    for (const CsvResultTable &csv : csvTables_) {
        QStringList keys;
        for (const QString &column : csv.columns)
            keys.append(column.toCaseFolded());
        QVector<int> indices;
        for (const QString &column : csvColumns_)
            indices.append(keys.indexOf(column.toCaseFolded()));
        for (const QStringList &row : csv.rows) {
            auto *source = readOnlyItem(QFileInfo(csv.sourcePath).fileName());
            source->setToolTip(csv.sourcePath);
            tableWidget_->setItem(tableRow, 0, source);
            for (int column = 0; column < indices.size(); ++column) {
                const int index = indices.at(column);
                const QString value = index < 0 ? QString() : row.at(index);
                auto *item = readOnlyItem(value.isEmpty() ? kMissing : value);
                // Preserve the source text as well as its display value.
                item->setData(Qt::UserRole, value);
                tableWidget_->setItem(tableRow, column + 1, item);
            }
            ++tableRow;
        }
    }
    tableWidget_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableWidget_->horizontalHeader()->setStretchLastSection(true);
    status_->setText(QStringLiteral(u"%1 个文件 · %2 条记录 · %3 个字段 · — 表示缺少数据")
                         .arg(csvTables_.size()).arg(count).arg(csvColumns_.size()));
}

void ResultSummaryTablePage::populateForceTable()
{
    const int coefficientIndex = coefficient_->currentIndex();
    if (forceCases_.isEmpty() || coefficientIndex < 0)
        return;

    const QStringList caseHeaders{QStringLiteral(u"工况"), QStringLiteral("Ref."),
        QStringLiteral("Ma"), QStringLiteral(u"Beta (°)"), QStringLiteral(u"Alpha (°)")};
    tableWidget_->clear();
    tableWidget_->setColumnCount(caseHeaders.size() + forceWalls_.size());
    tableWidget_->setRowCount(forceCases_.size());
    tableWidget_->setHorizontalHeaderLabels(caseHeaders + forceWalls_);
    for (int row = 0; row < forceCases_.size(); ++row) {
        const auto &result = forceCases_.at(row);
        const QStringList caseFields{result.name, result.reference, result.mach, result.beta, result.alpha};
        for (int column = 0; column < caseFields.size(); ++column) {
            const QString &value = caseFields.at(column);
            tableWidget_->setItem(row, column, readOnlyItem(value.isEmpty() ? kMissing : value));
        }
        for (int column = 0; column < forceWalls_.size(); ++column) {
            const QString &wall = forceWalls_.at(column);
            const auto values = result.valuesByWall.constFind(wall);
            const QString key = comparisonFields_.at(coefficientIndex).toCaseFolded();
            const QString value = values == result.valuesByWall.constEnd()
                ? QString() : values.value().value(key);
            auto *item = readOnlyItem(value.isEmpty() ? kMissing : value);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (wall == kAllWall) {
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
            }
            tableWidget_->setItem(row, caseFields.size() + column, item);
        }
    }
    auto *header = tableWidget_->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setStretchLastSection(true);
    status_->setText(QStringLiteral(u"%1 个工况 · %2 · ALL_WALL 为全壁面合计 · — 表示缺少数据")
                         .arg(forceCases_.size()).arg(comparisonFields_.at(coefficientIndex)));
}

void ResultSummaryTablePage::clearData()
{
    table_.clear();
    forceCases_.clear();
    forceWalls_.clear();
    csvTables_.clear();
    csvColumns_.clear();
    comparisonFields_.clear();
    csvView_->hide();
    coefficient_->clear();
    forceControls_->hide();
    tableWidget_->clear();
    tableWidget_->setRowCount(0);
    tableWidget_->setColumnCount(0);
    sourceLabel_->setText(QStringLiteral(u"尚未打开结果文件。"));
    sourceLabel_->setToolTip(QString());
    updateStatus();
}

void ResultSummaryTablePage::populateTable()
{
    const QStringList headers{
        QStringLiteral(u"变量"), QStringLiteral(u"单位"), QStringLiteral(u"采样数"),
        QStringLiteral(u"最小值"), QStringLiteral(u"最大值"), QStringLiteral(u"平均值"),
        QStringLiteral(u"末值"), QStringLiteral(u"类型")};

    tableWidget_->clear();
    tableWidget_->setColumnCount(headers.size());
    tableWidget_->setRowCount(table_.variables.size());
    tableWidget_->setHorizontalHeaderLabels(headers);
    tableWidget_->horizontalHeader()->setStretchLastSection(false);
    tableWidget_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    for (int row = 0; row < table_.variables.size(); ++row) {
        const ResultVariable &variable = table_.variables.at(row);
        const QVector<double> values = table_.samples.value(variable.key);

        double minimum = 0.0;
        double maximum = 0.0;
        double sum = 0.0;
        int finite = 0;
        double last = 0.0;
        bool haveLast = false;
        for (double value : values) {
            if (!std::isfinite(value))
                continue;
            if (finite == 0) {
                minimum = maximum = value;
            } else {
                minimum = qMin(minimum, value);
                maximum = qMax(maximum, value);
            }
            sum += value;
            ++finite;
            last = value;
            haveLast = true;
        }

        tableWidget_->setItem(row, 0, readOnlyItem(variable.key));
        tableWidget_->setItem(row, 1, readOnlyItem(variable.unit));
        tableWidget_->setItem(row, 2, readOnlyItem(QString::number(finite)));
        tableWidget_->setItem(row, 3, readOnlyItem(numberText(minimum)));
        tableWidget_->setItem(row, 4, readOnlyItem(numberText(maximum)));
        tableWidget_->setItem(row, 5, readOnlyItem(
            finite > 0 ? numberText(sum / finite) : QStringLiteral(u"—")));
        tableWidget_->setItem(row, 6, readOnlyItem(haveLast ? numberText(last)
                                                            : QStringLiteral(u"—")));
        tableWidget_->setItem(row, 7, readOnlyItem(variable.isResidual
                                                       ? QStringLiteral(u"残差")
                                                       : QStringLiteral(u"变量")));
    }
}

void ResultSummaryTablePage::updateStatus()
{
    if (table_.isEmpty()) {
        status_->setText(QStringLiteral(u"选择一个或多个 CSV 查看实际字段，也可以打开历史文件查看采样统计。"));
        return;
    }

    QString text = QStringLiteral(u"%1 个变量 · %2 个采样点 · 横轴 %3")
                       .arg(table_.variables.size())
                       .arg(table_.sampleCount())
                       .arg(table_.axisLabel.isEmpty() ? QStringLiteral(u"—")
                                                       : table_.axisLabel);
    if (!table_.warnings.isEmpty())
        text += QStringLiteral(u" · %1").arg(table_.warnings.join(QStringLiteral("；")));
    status_->setText(text);
}

void ResultSummaryTablePage::chooseFile()
{
    if (source_ == nullptr)
        return;
    const QString filter = source_->fileFilter();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral(u"打开结果文件"), QString(), filter);
    if (path.isEmpty())
        return;
    QString error;
    if (!loadHistoryFile(path, &error))
        status_->setText(error);
}

} // namespace aerosim
