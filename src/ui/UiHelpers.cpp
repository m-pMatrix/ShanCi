#include "ui/UiHelpers.h"

#include "core/Utils.h"

#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QTableWidget>

namespace vt {
namespace ui {

void info(QWidget *parent, const QString &text, const QString &title) {
    QMessageBox::information(parent, title, text);
}
void warn(QWidget *parent, const QString &text, const QString &title) {
    QMessageBox::warning(parent, title, text);
}
void error(QWidget *parent, const QString &text, const QString &title) {
    QMessageBox::critical(parent, title, text);
}
bool confirm(QWidget *parent, const QString &text, const QString &title) {
    return QMessageBox::question(parent, title, text, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}
QString askText(QWidget *parent, const QString &label, const QString &title, const QString &initial) {
    bool ok = false;
    const QString s = QInputDialog::getText(parent, title, label, QLineEdit::Normal, initial, &ok);
    return ok ? s : QString();
}

QTableWidget *makeTable(const QStringList &headers, QWidget *parent) {
    auto *t = new QTableWidget(parent);
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setAlternatingRowColors(true);
    t->setShowGrid(false);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(34);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setHighlightSections(false);
    t->setSortingEnabled(false);
    return t;
}

void setRow(QTableWidget *table, int row, const QStringList &values) {
    for (int c = 0; c < values.size() && c < table->columnCount(); ++c) {
        auto *item = new QTableWidgetItem(values.at(c));
        item->setToolTip(values.at(c));
        table->setItem(row, c, item);
    }
}

void clearTable(QTableWidget *table) { table->setRowCount(0); }

QString shortMeaning(const Word &w, int maxLen) {
    const QString m = w.primaryMeaning().simplified();
    if (m.size() <= maxLen) return m;
    return m.left(maxLen) + QStringLiteral("…");
}

QString fmtDate(const QDateTime &dt, const QString &def) {
    if (!dt.isValid()) return def;
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString statusBadge(WrongStatus s) {
    switch (s) {
    case WrongStatus::New: return QStringLiteral("新错题");
    case WrongStatus::Reviewing: return QStringLiteral("复习中");
    case WrongStatus::Mastered: return QStringLiteral("已掌握");
    }
    return QString();
}

QString priorityBadge(WrongPriority p) {
    switch (p) {
    case WrongPriority::Low: return QStringLiteral("低");
    case WrongPriority::Normal: return QStringLiteral("中");
    case WrongPriority::High: return QStringLiteral("高");
    }
    return QString();
}

QString sourceBadge(EntrySource s) {
    switch (s) {
    case EntrySource::WordBook: return QStringLiteral("单词本");
    case EntrySource::WrongBook: return QStringLiteral("错题本");
    case EntrySource::Manual: return QStringLiteral("手动");
    }
    return QString();
}

} // namespace ui
} // namespace vt
