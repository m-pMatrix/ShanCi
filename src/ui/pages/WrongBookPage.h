#pragma once
//
// WrongBookPage.h - 错题本页
//
#include "ui/Page.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QComboBox;
class QTableWidget;
class QLabel;
QT_END_NAMESPACE

namespace vt {

class WrongBookPage : public Page {
    Q_OBJECT
public:
    WrongBookPage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("wrongbook"); }
    void refresh() override;

private slots:
    void onSearch();
    void onDetail();
    void onEdit();
    void onRemove();
    void onMaster();
    void onContextMenu(const QPoint &pos);

private:
    qint64 selectedEntryId() const;
    int selectedRow() const;

    QLineEdit *m_search = nullptr;    QComboBox *m_statusFilter = nullptr;
    QComboBox *m_bookFilter = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_countLabel = nullptr;
    int m_total = 0;
};

} // namespace vt
