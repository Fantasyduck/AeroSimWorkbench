#include "aerosim/ui/intelligent/ResultSummaryTablePage.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QTextStream>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

namespace aerosim {
namespace {

const QStringList kCoefficients{
    QStringLiteral("Cl"), QStringLiteral("Cd"), QStringLiteral("Cx"), QStringLiteral("Cy"),
    QStringLiteral("Cz"), QStringLiteral("Cmx"), QStringLiteral("Cmy"), QStringLiteral("Cmz")};
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
    auto *forceButton = new QPushButton(QStringLiteral(u"打开气动力 CSV…"), this);
    connect(forceButton, &QPushButton::clicked, this, [this] {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this, QStringLiteral(u"打开气动力结果"), QString(),
            QStringLiteral(u"气动力结果 (*_force.csv);;CSV (*.csv)"));
        if (paths.isEmpty())
            return;
        QString error;
        if (!loadForceFiles(paths, &error))
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
    auto *coefficientLabel = new QLabel(QStringLiteral(u"系数"), forceControls_);
    coefficient_ = new QComboBox(forceControls_);
    coefficient_->setObjectName(QStringLiteral("forceSummaryCoefficient"));
    coefficient_->setAccessibleName(coefficientLabel->text());
    coefficientLabel->setBuddy(coefficient_);
    coefficient_->addItems(kCoefficients);
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
    if (path.endsWith(QStringLiteral("_force.csv"), Qt::CaseInsensitive))
        return loadForceFiles({path}, error);
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
    QStringList columns{QStringLiteral("BC index"), QStringLiteral("wallname")};
    columns += kCoefficients;
    QVector<ForceCase> cases;
    QStringList walls;
    QStringList filenames;
    bool hasTotal = false;
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    for (const QString &path : paths) {
        const QString filename = QFileInfo(path).fileName();
        QString caseName = QFileInfo(path).completeBaseName();
        if (caseName.endsWith(QStringLiteral("_force"), Qt::CaseInsensitive))
            caseName.chop(6);
        ForceCase result;
        result.name = caseName;
        const auto condition = kCaseCondition.match(caseName);
        if (condition.hasMatch()) {
            result.mach = condition.captured(1);
            result.alpha = condition.captured(3);
            result.beta = condition.captured(4);
            auto fields = kReferenceField.globalMatch(condition.captured(2));
            QStringList reference;
            while (fields.hasNext()) {
                const auto field = fields.next();
                reference.append(QStringLiteral("%1:%2").arg(field.captured(1), field.captured(2)));
            }
            result.reference = reference.join(QStringLiteral(", "));
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return fail(QStringLiteral(u"无法打开 %1：%2").arg(filename, file.errorString()));
        QTextStream input(&file);
        input.setCodec("UTF-8");
        QStringList headers = input.readLine().split(QLatin1Char(','));
        for (QString &header : headers)
            header = header.trimmed().toCaseFolded();
        QVector<int> indices;
        for (const QString &column : columns) {
            const QString key = column.toCaseFolded();
            const int index = headers.indexOf(key);
            if (index < 0 || headers.lastIndexOf(key) != index)
                return fail(QStringLiteral(u"%1 的列缺失或重复：%2").arg(filename, column));
            indices.append(index);
        }

        int lineNumber = 1;
        while (!input.atEnd()) {
            const QString line = input.readLine();
            ++lineNumber;
            if (line.trimmed().isEmpty())
                continue;
            const QStringList cells = line.split(QLatin1Char(','));
            QStringList row;
            bool valid = cells.size() == headers.size();
            for (int i = 0; valid && i < indices.size(); ++i) {
                const QString cell = cells.at(indices.at(i)).trimmed();
                if (i < 2) {
                    valid = !cell.isEmpty();
                } else {
                    bool numeric = false;
                    const double value = cell.toDouble(&numeric);
                    valid = numeric && std::isfinite(value);
                }
                row.append(cell);
            }
            if (!valid)
                return fail(QStringLiteral(u"%1 第 %2 行含有无效的壁面或系数。")
                                .arg(filename).arg(lineNumber));
            QString wall = row.at(1);
            if (wall.compare(kAllWall, Qt::CaseInsensitive) == 0) {
                wall = kAllWall;
                hasTotal = true;
            } else if (!walls.contains(wall)) {
                walls.append(wall);
            }
            if (result.coefficientsByWall.contains(wall))
                return fail(QStringLiteral(u"%1 第 %2 行的壁面重复：%3").arg(filename).arg(lineNumber).arg(wall));
            result.coefficientsByWall.insert(wall, row.mid(2));
        }
        if (input.status() != QTextStream::Ok || result.coefficientsByWall.isEmpty())
            return fail(QStringLiteral(u"%1 没有可读取的气动力结果。").arg(filename));
        cases.append(result);
        filenames.append(filename);
    }
    if (cases.isEmpty())
        return fail(QStringLiteral(u"请至少选择一个气动力 CSV 文件。"));

    if (hasTotal)
        walls.append(kAllWall);
    table_.clear();
    forceCases_ = cases;
    forceWalls_ = walls;
    sourceLabel_->setText(filenames.size() == 1 ? filenames.first()
        : QStringLiteral(u"已导入 %1 个气动力文件").arg(filenames.size()));
    sourceLabel_->setToolTip(paths.join(QLatin1Char('\n')));
    forceControls_->show();
    populateForceTable();
    if (error)
        error->clear();
    for (const QString &path : paths)
        emit fileLoaded(path);
    return true;
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
            const auto values = result.coefficientsByWall.constFind(wall);
            auto *item = readOnlyItem(values == result.coefficientsByWall.constEnd()
                                      ? kMissing : values.value().at(coefficientIndex));
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
                         .arg(forceCases_.size()).arg(kCoefficients.at(coefficientIndex)));
}

void ResultSummaryTablePage::clearData()
{
    table_.clear();
    forceCases_.clear();
    forceWalls_.clear();
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
        status_->setText(QStringLiteral(u"选择一个或多个气动力 CSV 查看壁面系数，也可以打开历史文件查看采样统计。"));
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
    if (!loadFile(path, &error))
        sourceLabel_->setText(error);
}

} // namespace aerosim
