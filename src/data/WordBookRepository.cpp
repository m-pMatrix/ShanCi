#include "core/Utils.h"
#include "data/Repositories.h"
#include "data/WordMapper.h"

namespace vt {

namespace {
const char *kEntryCols =
    "e.id, e.word_book_id, e.word_id, e.familiarity, e.review_count, e.next_review_at, e.source, e.added_at";
constexpr int kEntryColCount = 8;

WordBookEntry entryFromStmt(Stmt &st) {
    WordBookEntry e;
    e.id = st.colInt64(0);
    e.wordBookId = st.colInt64(1);
    e.wordId = st.colInt64(2);
    e.familiarity = st.colInt(3);
    e.reviewCount = st.colInt(4);
    e.nextReviewAt = util::fromIso(st.colText(5));
    e.source = entrySourceFromKey(st.colText(6));
    e.addedAt = util::fromIso(st.colText(7));
    e.word = mapper::wordFromStmt(st, kEntryColCount);
    return e;
}
} // namespace

qint64 WordBookRepository::createBook(const WordBook &b) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO word_books (user_id, name, description, created_at, updated_at) VALUES (?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    const QString now = util::nowIso();
    st.bind(1, b.userId).bind(2, b.name).bind(3, b.description).bind(4, now).bind(5, now);
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

std::optional<WordBook> WordBookRepository::findBook(qint64 id) {
    Stmt st(m_db.db(),
            QStringLiteral("SELECT b.id, b.user_id, b.name, b.description, b.created_at, b.updated_at, "
                           "(SELECT COUNT(*) FROM word_book_entries e WHERE e.word_book_id=b.id) "
                           "FROM word_books b WHERE b.id=?"));
    st.bind(1, id);
    if (st.isValid() && st.step()) {
        WordBook b;
        b.id = st.colInt64(0);
        b.userId = st.colInt64(1);
        b.name = st.colText(2);
        b.description = st.colText(3);
        b.createdAt = util::fromIso(st.colText(4));
        b.updatedAt = util::fromIso(st.colText(5));
        b.entryCount = st.colInt(6);
        return b;
    }
    return std::nullopt;
}

std::optional<WordBook> WordBookRepository::findDefaultBook(qint64 userId) {
    Stmt st(m_db.db(),
            QStringLiteral("SELECT b.id, b.user_id, b.name, b.description, b.created_at, b.updated_at, "
                           "(SELECT COUNT(*) FROM word_book_entries e WHERE e.word_book_id=b.id) "
                           "FROM word_books b WHERE b.user_id=? ORDER BY b.id ASC LIMIT 1"));
    st.bind(1, userId);
    if (st.isValid() && st.step()) {
        WordBook b;
        b.id = st.colInt64(0);
        b.userId = st.colInt64(1);
        b.name = st.colText(2);
        b.description = st.colText(3);
        b.createdAt = util::fromIso(st.colText(4));
        b.updatedAt = util::fromIso(st.colText(5));
        b.entryCount = st.colInt(6);
        return b;
    }
    return std::nullopt;
}

QList<WordBook> WordBookRepository::listBooks(qint64 userId) {
    QList<WordBook> out;
    Stmt st(m_db.db(),
            QStringLiteral("SELECT b.id, b.user_id, b.name, b.description, b.created_at, b.updated_at, "
                           "(SELECT COUNT(*) FROM word_book_entries e WHERE e.word_book_id=b.id) "
                           "FROM word_books b WHERE b.user_id=? ORDER BY b.id ASC"));
    st.bind(1, userId);
    if (!st.isValid()) return out;
    while (st.step()) {
        WordBook b;
        b.id = st.colInt64(0);
        b.userId = st.colInt64(1);
        b.name = st.colText(2);
        b.description = st.colText(3);
        b.createdAt = util::fromIso(st.colText(4));
        b.updatedAt = util::fromIso(st.colText(5));
        b.entryCount = st.colInt(6);
        out << b;
    }
    return out;
}

bool WordBookRepository::renameBook(qint64 id, const QString &name, const QString &desc) {
    Stmt st(m_db.db(), QStringLiteral("UPDATE word_books SET name=?, description=?, updated_at=? WHERE id=?"));
    st.bind(1, name).bind(2, desc).bind(3, util::nowIso()).bind(4, id);
    return st.run();
}

bool WordBookRepository::deleteBook(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM word_books WHERE id=?"));
    st.bind(1, id);
    return st.run();
}

QList<WordBookEntry> WordBookRepository::findByWord(qint64 wordId) {
    QList<WordBookEntry> out;
    const QString sql = QStringLiteral("SELECT %1, %2 FROM word_book_entries e JOIN words w ON w.id=e.word_id "
                                       "WHERE e.word_id=?")
                            .arg(QLatin1String(kEntryCols), mapper::wordColumns());
    Stmt st(m_db.db(), sql);
    st.bind(1, wordId);
    if (!st.isValid()) return out;
    while (st.step()) out << entryFromStmt(st);
    return out;
}

bool WordBookRepository::addEntry(qint64 bookId, qint64 wordId, EntrySource source, bool ignoreIfExists) {
    const QString verb = ignoreIfExists ? QStringLiteral("INSERT OR IGNORE") : QStringLiteral("INSERT OR REPLACE");
    Stmt st(m_db.db(),
            QStringLiteral("%1 INTO word_book_entries (word_book_id, word_id, familiarity, review_count, "
                           "next_review_at, source, added_at) VALUES (?,?,?,?,?,?,?)")
                .arg(verb));
    if (!st.isValid()) return false;
    st.bind(1, bookId).bind(2, wordId).bind(3, 0).bind(4, 0).bindNull(5)
        .bind(6, toKey(source)).bind(7, util::nowIso());
    if (!st.run()) return false;
    Stmt touch(m_db.db(), QStringLiteral("UPDATE word_books SET updated_at=? WHERE id=?"));
    touch.bind(1, util::nowIso()).bind(2, bookId);
    touch.run();
    return true;
}

int WordBookRepository::addAllWords(qint64 bookId, const QString &bookFilter, EntrySource source) {
    if (bookId <= 0) return 0;
    QString sql = QStringLiteral(
        "INSERT OR IGNORE INTO word_book_entries "
        "(word_book_id, word_id, familiarity, review_count, next_review_at, source, added_at) "
        "SELECT ?, id, 0, 0, NULL, ?, ? FROM words");
    if (!bookFilter.isEmpty()) sql += QStringLiteral(" WHERE book=?");
    Stmt st(m_db.db(), sql);
    if (!st.isValid()) return 0;
    const QString now = util::nowIso();
    st.bind(1, bookId).bind(2, toKey(source)).bind(3, now);
    if (!bookFilter.isEmpty()) st.bind(4, bookFilter);
    if (!st.run()) return 0;
    const int added = m_db.db().changes();
    if (added > 0) {
        Stmt touch(m_db.db(), QStringLiteral("UPDATE word_books SET updated_at=? WHERE id=?"));
        touch.bind(1, util::nowIso()).bind(2, bookId);
        touch.run();
    }
    return added;
}

bool WordBookRepository::removeEntry(qint64 bookId, qint64 wordId) {    Stmt st(m_db.db(), QStringLiteral("DELETE FROM word_book_entries WHERE word_book_id=? AND word_id=?"));
    st.bind(1, bookId).bind(2, wordId);
    return st.run();
}

bool WordBookRepository::removeEntryById(qint64 entryId) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM word_book_entries WHERE id=?"));
    st.bind(1, entryId);
    return st.run();
}

bool WordBookRepository::hasEntry(qint64 bookId, qint64 wordId) {
    Stmt st(m_db.db(), QStringLiteral("SELECT COUNT(*) FROM word_book_entries WHERE word_book_id=? AND word_id=?"));
    st.bind(1, bookId).bind(2, wordId);
    return st.isValid() && st.step() && st.colInt(0) > 0;
}

bool WordBookRepository::updateEntryProgress(qint64 bookId, qint64 wordId, int familiarity, int reviewCount,
                                             const QDateTime &nextReview) {
    Stmt st(m_db.db(),
            QStringLiteral("UPDATE word_book_entries SET familiarity=?, review_count=?, next_review_at=? "
                           "WHERE word_book_id=? AND word_id=?"));
    st.bind(1, familiarity).bind(2, reviewCount);
    if (nextReview.isValid()) st.bind(3, util::toIso(nextReview)); else st.bindNull(3);
    st.bind(4, bookId).bind(5, wordId);
    return st.run();
}

bool WordBookRepository::updateEntryFamiliarity(qint64 entryId, int familiarity) {
    Stmt st(m_db.db(), QStringLiteral("UPDATE word_book_entries SET familiarity=? WHERE id=?"));
    st.bind(1, familiarity).bind(2, entryId);
    return st.run();
}

QList<WordBookEntry> WordBookRepository::listEntries(qint64 bookId, const WordQuery &q, int *totalOut) {
    QString where = QStringLiteral(" WHERE e.word_book_id=?");
    QList<QString> textBinds;
    QList<int> intBinds;
    if (!q.keyword.trimmed().isEmpty()) {
        where += QStringLiteral(" AND (w.spelling LIKE ? OR w.translation_cn LIKE ?)");
        const QString kw = QStringLiteral("%") + q.keyword.trimmed() + QStringLiteral("%");
        textBinds << kw << kw;
    }
    if (!q.book.isEmpty()) {
        where += QStringLiteral(" AND w.book=?");
        textBinds << q.book;
    }
    if (q.minStar >= 0) {
        where += QStringLiteral(" AND w.star >= ?");
        intBinds << q.minStar;
    }
    if (q.familiarityMin >= 0) {
        where += QStringLiteral(" AND e.familiarity >= ?");
        intBinds << q.familiarityMin;
    }

    const QString join = QStringLiteral(" FROM word_book_entries e JOIN words w ON w.id=e.word_id") + where;

    if (totalOut) {
        Stmt cnt(m_db.db(), QStringLiteral("SELECT COUNT(*)") + join);
        int idx = 1;
        cnt.bind(idx++, bookId);
        for (const auto &b : textBinds) cnt.bind(idx++, b);
        for (auto b : intBinds) cnt.bind(idx++, b);
        *totalOut = (cnt.isValid() && cnt.step()) ? cnt.colInt(0) : 0;
    }

    QString order = QStringLiteral("w.word_rank ASC");
    if (q.orderBy == QLatin1String("added")) order = QStringLiteral("e.added_at DESC");
    else if (q.orderBy == QLatin1String("familiarity")) order = QStringLiteral("e.familiarity ASC");

    const QString sql = QStringLiteral("SELECT %1, %2")
                            .arg(QLatin1String(kEntryCols), mapper::wordColumns()) +
                        join + QStringLiteral(" ORDER BY ") + order + QStringLiteral(" LIMIT ? OFFSET ?");
    Stmt st(m_db.db(), sql);
    QList<WordBookEntry> out;
    if (!st.isValid()) return out;
    int idx = 1;
    st.bind(idx++, bookId);
    for (const auto &b : textBinds) st.bind(idx++, b);
    for (auto b : intBinds) st.bind(idx++, b);
    st.bind(idx++, q.limit).bind(idx++, q.offset);
    while (st.step()) out << entryFromStmt(st);
    return out;
}

std::optional<WordBookEntry> WordBookRepository::findEntry(qint64 bookId, qint64 wordId) {
    const QString sql = QStringLiteral("SELECT %1, %2 FROM word_book_entries e JOIN words w ON w.id=e.word_id "
                                       "WHERE e.word_book_id=? AND e.word_id=?")
                            .arg(QLatin1String(kEntryCols), mapper::wordColumns());
    Stmt st(m_db.db(), sql);
    st.bind(1, bookId).bind(2, wordId);
    if (st.isValid() && st.step()) return entryFromStmt(st);
    return std::nullopt;
}

qint64 WordBookRepository::countEntries(qint64 bookId) {
    Stmt st(m_db.db(), QStringLiteral("SELECT COUNT(*) FROM word_book_entries WHERE word_book_id=?"));
    st.bind(1, bookId);
    return (st.isValid() && st.step()) ? st.colInt64(0) : 0;
}

qint64 WordBookRepository::countAllEntries(qint64 userId) {
    Stmt st(m_db.db(),
            QStringLiteral("SELECT COUNT(*) FROM word_book_entries e "
                           "JOIN word_books b ON b.id=e.word_book_id WHERE b.user_id=?"));
    st.bind(1, userId);
    return (st.isValid() && st.step()) ? st.colInt64(0) : 0;
}

QList<qint64> WordBookRepository::wordIdsOfBook(qint64 bookId) {
    QList<qint64> out;
    Stmt st(m_db.db(), QStringLiteral("SELECT e.word_id FROM word_book_entries e JOIN words w ON w.id=e.word_id "
                                      "WHERE e.word_book_id=? ORDER BY w.word_rank ASC, e.id ASC"));
    st.bind(1, bookId);
    if (!st.isValid()) return out;
    while (st.step()) out << st.colInt64(0);
    return out;
}

QList<WordBookEntry> WordBookRepository::dueForReview(qint64 bookId, const QDateTime &before, int limit) {
    const QString sql = QStringLiteral("SELECT %1, %2 FROM word_book_entries e JOIN words w ON w.id=e.word_id "
                                       "WHERE e.word_book_id=? AND (e.next_review_at IS NULL OR e.next_review_at<=?) "
                                       "ORDER BY e.next_review_at IS NULL DESC, e.next_review_at ASC LIMIT ?")
                            .arg(QLatin1String(kEntryCols), mapper::wordColumns());
    Stmt st(m_db.db(), sql);
    QList<WordBookEntry> out;
    if (!st.isValid()) return out;
    st.bind(1, bookId).bind(2, util::toIso(before)).bind(3, limit);
    while (st.step()) out << entryFromStmt(st);
    return out;
}

} // namespace vt
