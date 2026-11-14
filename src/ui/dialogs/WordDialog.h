#pragma once
//
// WordDialog.h - 单词详情查看 / 编辑对话框
//
#include "domain/Entities.h"

#include <QDialog>

#include <functional>

QT_BEGIN_NAMESPACE
class QTextBrowser;
class QPushButton;
QT_END_NAMESPACE

namespace vt {

class AppContext;

class WordDialog : public QDialog {
    Q_OBJECT
public:
    WordDialog(AppContext &ctx, qint64 wordId, QWidget *parent = nullptr);

signals:
    void wordChanged();

private slots:
    void reload();
    void onPlay(bool uk);
    void onAddToWordBook();

private:
    AppContext &m_ctx;
    qint64 m_wordId;
    QTextBrowser *m_browser = nullptr;
    QPushButton *m_addWordBtn = nullptr;
};

// 编辑单词（新建或修改）
class WordEditDialog : public QDialog {
    Q_OBJECT
public:
    WordEditDialog(AppContext &ctx, const Word &seed, bool isNew, QWidget *parent = nullptr);

    bool saved() const { return m_saved; }
    qint64 wordId() const { return m_wordId; }

private slots:
    void onSave();

private:
    AppContext &m_ctx;
    Word m_word;
    bool m_isNew;
    bool m_saved = false;
    qint64 m_wordId = 0;
    std::function<void()> m_pendingSave;
};

} // namespace vt
