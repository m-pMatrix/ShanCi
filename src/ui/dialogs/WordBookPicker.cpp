#include "ui/dialogs/WordBookPicker.h"

#include "core/AppContext.h"
#include "ui/UiHelpers.h"

#include <QInputDialog>

namespace vt {
namespace ui {

namespace {
const QString kNewBookItem = QStringLiteral("➕ 新建单词本…");
}

qint64 pickWordBook(AppContext &ctx, qint64 userId, QWidget *parent, qint64 current) {
    if (userId <= 0) return 0;

    while (true) {
        auto books = ctx.wordBookService().listBooks(userId);
        if (books.isEmpty()) {
            // 保证至少有一个单词本
            const qint64 id = ctx.wordBookService().ensureDefaultBook(userId);
            if (id <= 0) return 0;
            books = ctx.wordBookService().listBooks(userId);
        }

        QStringList items;
        for (const auto &b : books)
            items << QStringLiteral("%1（%2 词）").arg(b.name).arg(b.entryCount);
        items << kNewBookItem;

        int currentIndex = 0;
        for (int i = 0; i < books.size(); ++i)
            if (books[i].id == current) { currentIndex = i; break; }

        bool ok = false;
        const QString chosen = QInputDialog::getItem(
            parent, QStringLiteral("选择单词本"),
            QStringLiteral("把该单词加入哪个单词本？"), items, currentIndex, false, &ok);
        if (!ok || chosen.isEmpty()) return 0;

        if (chosen == kNewBookItem) {
            const QString name = QInputDialog::getText(parent, QStringLiteral("新建单词本"),
                                                       QStringLiteral("单词本名称："),
                                                       QLineEdit::Normal, QString(), &ok);
            if (!ok || name.trimmed().isEmpty()) continue;
            auto created = ctx.wordBookService().createBook(userId, name.trimmed());
            if (!created) {
                ui::warn(parent, created.error());
                continue;
            }
            return created.value().id;
        }

        const int idx = items.indexOf(chosen);
        if (idx >= 0 && idx < books.size()) return books[idx].id;
    }
}

QString addWordToChosenBook(AppContext &ctx, qint64 userId, qint64 wordId, QWidget *parent) {
    const qint64 bookId = pickWordBook(ctx, userId, parent);
    if (bookId <= 0) return QString();
    const auto st = ctx.wordBookService().addWord(bookId, wordId, EntrySource::Manual);
    if (!st.isOk()) {
        ui::warn(parent, st.error());
        return QString();
    }
    const auto b = ctx.wordBookService().listBooks(userId);
    for (const auto &x : b)
        if (x.id == bookId) return x.name;
    return QStringLiteral("单词本");
}

} // namespace ui
} // namespace vt
