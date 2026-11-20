#pragma once

#include "aerosim/ui/RadioChoice.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>

namespace aerosim { namespace parameterTable {

// Cell editors keep their normal height while the choice determines row height.
inline QWidget *cell(QTableWidget *table, QWidget *editor)
{
    auto *host = new QWidget(table);
    host->setProperty("parameterTableCell", true);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(0);
    layout->addWidget(editor, 1, Qt::AlignVCenter);
    return host;
}

inline QWidget *buttonCell(QTableWidget *table, QPushButton *button)
{
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *host = cell(table, button);
    host->layout()->setAlignment(button, Qt::AlignCenter);
    return host;
}

inline void updateRow(QTableWidget *table, int row)
{
    int height = 0;
    for (int column = 0; column < table->columnCount(); ++column) {
        auto *host = table->cellWidget(row, column);
        auto *choice = host ? host->findChild<RadioChoice *>() : nullptr;
        if (choice)
            height = qMax(height, choice->heightForWidth(table->columnWidth(column) - 17) + 17);
    }
    if (height) table->setRowHeight(row, height);
}

inline void updateRows(QTableWidget *table)
{
    for (int row = 0; row < table->rowCount(); ++row) updateRow(table, row);
}

inline void choiceCell(QTableWidget *table, int row, int column, RadioChoice *choice)
{
    auto *host = cell(table, choice);
    host->setMinimumWidth(choice->sizeHint().width() + 16);
    table->setCellWidget(row, column, host);
    table->resizeColumnToContents(column);
    QObject::connect(choice, &RadioChoice::layoutHeightChanged, table, [table, row](int) {
        updateRow(table, row);
    });
    updateRow(table, row);
}

inline void configure(QTableWidget *table)
{
    table->verticalHeader()->hide();
    table->setAlternatingRowColors(true);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setMinimumSectionSize(96);
    QObject::connect(table->horizontalHeader(), &QHeaderView::sectionResized,
                     table, [table] { updateRows(table); });
}

}}
