#pragma once
//
// DictionaryPage.h - 查询字典页（在“词库”中查询，可加入单词本/错题本）
//
#include "ui/Page.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QComboBox;
class QTableWidget;
class QLabel;
class QListWidget;
class QTimer;
QT_END_NAMESPACE

namespace vt {

class DictionaryPage : public Page {
    Q_OBJECT
public:
    DictionaryPage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("dictionary"); }
    void refresh() override;

private slots:
    void onSearch();
    void onImportDict();
    void onDetail();
    void onAddWord();
    void onSuggestionClicked();
    void onTextEdited();

private:
    qint64 selectedWordId() const;
    int selectedRow() const;
    void updateSuggestions();
    void jumpToWord(qint64 wordId, const QString &spelling);

    QLineEdit *m_search = nullptr;
    QComboBox *m_bookFilter = nullptr;
    QLabel *m_info = nullptr;
    QListWidget *m_suggestions = nullptr;
    QTableWidget *m_table = nullptr;
    QTimer *m_suggestTimer = nullptr;
    bool m_importing = false;
};

} // namespace vt
