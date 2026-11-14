#include "ui/dialogs/WordDialog.h"

#include "core/AppContext.h"
#include "core/Utils.h"
#include "ui/UiHelpers.h"
#include "ui/dialogs/WordBookPicker.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace vt {

namespace {
QString esc(const QString &s) { return s.toHtmlEscaped(); }
QString buildHtml(const Word &w) {
    QString h;
    h += QStringLiteral("<h2 style='margin:0'>%1</h2>").arg(esc(w.spelling));
    QStringList ph;
    if (!w.phoneticUk.isEmpty()) ph << QStringLiteral("英 /%1/").arg(esc(w.phoneticUk));
    if (!w.phoneticUs.isEmpty()) ph << QStringLiteral("美 /%1/").arg(esc(w.phoneticUs));
    if (!ph.isEmpty()) h += QStringLiteral("<p style='color:#6B7280;margin:2px 0 10px'>%1</p>").arg(ph.join(QStringLiteral("　")));

    h += QStringLiteral("<p><b>词书：</b>%1　<b>序号：</b>%2　<b>来源：</b>%3</p>")
             .arg(displayName(w.book)).arg(w.wordRank).arg(esc(w.source));

    if (!w.translations.isEmpty()) {
        h += QStringLiteral("<h4>释义</h4><ul>");
        for (const auto &t : w.translations) {
            QString line = t.pos.isEmpty() ? QString() : QStringLiteral("<i>%1.</i> ").arg(esc(t.pos));
            line += esc(t.tranCn);
            if (!t.tranEn.isEmpty()) line += QStringLiteral("　<span style='color:#6B7280'>%1</span>").arg(esc(t.tranEn));
            h += QStringLiteral("<li>%1</li>").arg(line);
        }
        h += QStringLiteral("</ul>");
    }
    if (!w.memMethod.isEmpty())
        h += QStringLiteral("<p><b>记忆：</b>%1</p>").arg(esc(w.memMethod));
    if (!w.phrases.isEmpty()) {
        h += QStringLiteral("<h4>短语</h4><ul>");
        for (const auto &p : w.phrases)
            h += QStringLiteral("<li>%1 — %2</li>").arg(esc(p.content), esc(p.meaning));
        h += QStringLiteral("</ul>");
    }
    if (!w.synonyms.isEmpty()) {
        h += QStringLiteral("<h4>同近义词</h4><ul>");
        for (const auto &s : w.synonyms)
            h += QStringLiteral("<li><i>%1</i> %2：%3</li>")
                     .arg(esc(s.pos), esc(s.tran), esc(s.words.join(QStringLiteral(", "))));
        h += QStringLiteral("</ul>");
    }
    if (!w.relWords.isEmpty()) {
        h += QStringLiteral("<h4>同根词</h4><ul>");
        for (const auto &r : w.relWords) {
            QStringList ws;
            for (const auto &p : r.words) ws << QStringLiteral("%1%2").arg(esc(p.first), esc(p.second));
            h += QStringLiteral("<li><i>%1</i> %2</li>").arg(esc(r.pos), ws.join(QStringLiteral("；")));
        }
        h += QStringLiteral("</ul>");
    }
    if (!w.sentences.isEmpty()) {
        h += QStringLiteral("<h4>例句</h4><ul>");
        for (const auto &s : w.sentences) {
            h += QStringLiteral("<li>%1<br><span style='color:#6B7280'>%2</span> <i>[%3]</i></li>")
                     .arg(esc(s.en), esc(s.cn), esc(s.tag));
        }
        h += QStringLiteral("</ul>");
    }
    if (!w.exams.isEmpty()) {
        h += QStringLiteral("<h4>真题</h4>");
        for (const auto &e : w.exams) {
            h += QStringLiteral("<p>%1</p><ul>").arg(esc(e.question));
            for (int i = 0; i < e.choices.size(); ++i)
                h += QStringLiteral("<li>%1. %2</li>").arg(QChar('A' + i)).arg(esc(e.choices[i]));
            h += QStringLiteral("</ul>");
            if (!e.explain.isEmpty())
                h += QStringLiteral("<p style='color:#2E9E5B'>解析：%1</p>").arg(esc(e.explain));
        }
    }
    return h;
}
} // namespace

// ---------------------------------------------------------------------------
// WordDialog
// ---------------------------------------------------------------------------
WordDialog::WordDialog(AppContext &ctx, qint64 wordId, QWidget *parent)
    : QDialog(parent), m_ctx(ctx), m_wordId(wordId) {
    setWindowTitle(QStringLiteral("单词详情"));
    resize(680, 620);

    auto *root = new QVBoxLayout(this);
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    root->addWidget(m_browser, 1);

    auto *bar = new QHBoxLayout;
    auto *ukBtn = new QPushButton(QStringLiteral("英音"), this);
    ukBtn->setProperty("secondary", true);
    auto *usBtn = new QPushButton(QStringLiteral("美音"), this);
    usBtn->setProperty("secondary", true);
    m_addWordBtn = new QPushButton(QStringLiteral("加入单词本"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setProperty("secondary", true);

    bar->addWidget(ukBtn);
    bar->addWidget(usBtn);
    bar->addStretch();
    bar->addWidget(m_addWordBtn);
    bar->addWidget(closeBtn);
    root->addLayout(bar);

    connect(ukBtn, &QPushButton::clicked, this, [this] { onPlay(true); });
    connect(usBtn, &QPushButton::clicked, this, [this] { onPlay(false); });
    connect(m_addWordBtn, &QPushButton::clicked, this, &WordDialog::onAddToWordBook);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    reload();
}

void WordDialog::reload() {
    auto w = m_ctx.wordService().detail(m_wordId);
    if (!w) {
        m_browser->setHtml(QStringLiteral("<p>单词不存在</p>"));
        return;
    }
    QString html = buildHtml(w.value());
    const qint64 uid = m_ctx.currentUserId();
    if (uid > 0) {
        const auto books = m_ctx.wordBookService().booksContaining(uid, m_wordId);
        if (!books.isEmpty()) {
            QStringList names;
            for (const auto &b : books) names << esc(b.name);
            html = QStringLiteral("<p style='color:#2E9E5B'>已收藏于：%1</p>")
                       .arg(names.join(QStringLiteral("、"))) +
                   html;
        }
    }
    m_browser->setHtml(html);
    m_addWordBtn->setText(QStringLiteral("加入单词本"));
    m_addWordBtn->setEnabled(uid > 0);
}

void WordDialog::onPlay(bool uk) {
    auto w = m_ctx.wordService().detail(m_wordId);
    if (w) m_ctx.audio().playWord(w.value().spelling, uk);
}

void WordDialog::onAddToWordBook() {
    const qint64 uid = m_ctx.currentUserId();
    if (uid <= 0) return;
    const QString book = ui::addWordToChosenBook(m_ctx, uid, m_wordId, this);
    if (!book.isEmpty()) {
        ui::info(this, QStringLiteral("已加入单词本「%1」").arg(book));
        reload();
        emit wordChanged();
    }
}

// ---------------------------------------------------------------------------
// WordEditDialog
// ---------------------------------------------------------------------------
WordEditDialog::WordEditDialog(AppContext &ctx, const Word &seed, bool isNew, QWidget *parent)
    : QDialog(parent), m_ctx(ctx), m_word(seed), m_isNew(isNew) {
    setWindowTitle(isNew ? QStringLiteral("新建单词") : QStringLiteral("编辑单词"));
    resize(520, 520);

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    auto *spelling = new QLineEdit(m_word.spelling, this);
    auto *uk = new QLineEdit(m_word.phoneticUk, this);
    auto *us = new QLineEdit(m_word.phoneticUs, this);
    auto *book = new QComboBox(this);
    book->addItem(QStringLiteral("四级"), QStringLiteral("CET4"));
    book->addItem(QStringLiteral("六级"), QStringLiteral("CET6"));
    book->addItem(QStringLiteral("自定义"), QStringLiteral("CUSTOM"));
    book->setCurrentIndex(book->findData(toKey(m_word.book)));
    auto *mem = new QLineEdit(m_word.memMethod, this);

    auto *meanings = new QPlainTextEdit(this);
    QStringList lines;
    for (const auto &t : m_word.translations)
        lines << QStringLiteral("%1 | %2 | %3").arg(t.pos, t.tranCn, t.tranEn);
    meanings->setPlainText(lines.join(QLatin1Char('\n')));
    meanings->setPlaceholderText(QStringLiteral("每行一个释义，格式：词性 | 中文释义 | 英文释义"));

    form->addRow(QStringLiteral("拼写"), spelling);
    form->addRow(QStringLiteral("英音标"), uk);
    form->addRow(QStringLiteral("美音标"), us);
    form->addRow(QStringLiteral("所属词书"), book);
    form->addRow(QStringLiteral("记忆方法"), mem);
    root->addLayout(form);
    root->addWidget(new QLabel(QStringLiteral("释义（每行：词性 | 中文 | 英文）"), this));
    root->addWidget(meanings, 1);

    auto *bar = new QHBoxLayout;
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    cancel->setProperty("secondary", true);
    auto *save = new QPushButton(QStringLiteral("保存"), this);
    bar->addStretch();
    bar->addWidget(cancel);
    bar->addWidget(save);
    root->addLayout(bar);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &WordEditDialog::onSave);

    // 保存时读取控件
    m_pendingSave = [this, spelling, uk, us, book, mem, meanings] { m_saved = true;
        m_word.spelling = spelling->text().trimmed();
        m_word.phoneticUk = uk->text().trimmed();
        m_word.phoneticUs = us->text().trimmed();
        m_word.book = bookTypeFromKey(book->currentData().toString());
        m_word.memMethod = mem->text().trimmed();
        QList<Translation> list;
        const auto rawLines = meanings->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const auto &ln : rawLines) {
            const auto parts = ln.split(QLatin1Char('|'));
            Translation t;
            if (parts.size() >= 1) t.pos = parts[0].trimmed();
            if (parts.size() >= 2) t.tranCn = parts[1].trimmed();
            if (parts.size() >= 3) t.tranEn = parts[2].trimmed();
            if (!t.tranCn.isEmpty() || !t.tranEn.isEmpty()) list << t;
        }
        m_word.translations = list;
    };
}

void WordEditDialog::onSave() {
    if (m_pendingSave) m_pendingSave();
    if (m_word.spelling.isEmpty()) {
        ui::warn(this, QStringLiteral("拼写不能为空"));
        m_saved = false;
        return;
    }
    if (m_isNew) {
        qint64 id = 0;
        const auto st = m_ctx.wordService().add(m_word, &id);
        if (!st.isOk()) { ui::error(this, st.error()); m_saved = false; return; }
        m_wordId = id;
    } else {
        const auto st = m_ctx.wordService().update(m_word);
        if (!st.isOk()) { ui::error(this, st.error()); m_saved = false; return; }
        m_wordId = m_word.id;
    }
    accept();
}

} // namespace vt
