#pragma once
//
// WordBookPage.h - 单词本（收藏夹）页：可创建多个单词本，手动管理其中的单词
//
#include "ui/Page.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QComboBox;
class QTableWidget;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace vt {

class WordBookPage : public Page {
    Q_OBJECT
public:
    WordBookPage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("wordbook"); }
    void refresh() override;

private slots:
    void onSearch();
    void onBookChanged();
    void onNewBook();
    void onRenameBook();
    void onDeleteBook();
    void onBatchAddBookWords();
    void onDetail();
    void onEdit();
    void onRemove();
    void onContextMenu(const QPoint &pos);

private:
    int selectedRow() const;
    qint64 selectedWordId() const;
    qint64 currentBookId() const;
    void reloadBooks(qint64 keepId = -1);

    QComboBox *m_bookCombo = nullptr;
    QLineEdit *m_search = nullptr;
    QComboBox *m_bookFilter = nullptr;
    QComboBox *m_famFilter = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    int m_total = 0;
};

} // namespace vt
