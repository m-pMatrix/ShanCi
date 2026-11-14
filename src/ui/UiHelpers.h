#pragma once
//
// UiHelpers.h - 界面通用工具
//
#include "domain/Entities.h"

#include <QString>

QT_BEGIN_NAMESPACE
class QTableWidget;
class QWidget;
class QLabel;
QT_END_NAMESPACE

namespace vt {
namespace ui {

// 消息框
void info(QWidget *parent, const QString &text, const QString &title = QStringLiteral("提示"));
void warn(QWidget *parent, const QString &text, const QString &title = QStringLiteral("警告"));
void error(QWidget *parent, const QString &text, const QString &title = QStringLiteral("错误"));
bool confirm(QWidget *parent, const QString &text, const QString &title = QStringLiteral("确认"));
QString askText(QWidget *parent, const QString &label, const QString &title = QStringLiteral("输入"),
                const QString &initial = QString());

// 表格
QTableWidget *makeTable(const QStringList &headers, QWidget *parent = nullptr);
void setRow(QTableWidget *table, int row, const QStringList &values);
void clearTable(QTableWidget *table);

// 文本
QString shortMeaning(const Word &w, int maxLen = 40);
QString fmtDate(const QDateTime &dt, const QString &def = QStringLiteral("-"));
QString statusBadge(WrongStatus s);
QString priorityBadge(WrongPriority p);
QString sourceBadge(EntrySource s);

} // namespace ui
} // namespace vt
