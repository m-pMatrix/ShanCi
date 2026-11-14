#pragma once
//
// WordBookPicker.h - 选择单词本（收藏夹）的通用对话框
//
#include <QString>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace vt {

class AppContext;

namespace ui {

// 弹出“选择单词本”对话框，返回选中的单词本 id；用户取消返回 0。
// 列表中提供“新建单词本…”选项。
qint64 pickWordBook(AppContext &ctx, qint64 userId, QWidget *parent = nullptr, qint64 current = 0);

// 选择单词本并把单词加入；返回加入后的单词本名称（失败/取消返回空）
QString addWordToChosenBook(AppContext &ctx, qint64 userId, qint64 wordId, QWidget *parent = nullptr);

} // namespace ui
} // namespace vt
