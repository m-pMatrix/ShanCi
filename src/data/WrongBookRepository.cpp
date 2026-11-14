#include "core/Utils.h"
#include "data/Repositories.h"
#include "data/WordMapper.h"
#include "domain/Rules.h"

namespace vt {

namespace {
const char *kWrongCols =
    "e.id, e.wrong_book_id, e.word_id, e.question_type, e.wrong_count, e.correct_count, "
    "e.last_wrong_at, e.last_review_at, e.next_review_at, e.priority, e.status, e.my_answer, e.remark, e.updated_at";
constexpr int kWrongColCount = 14;

WrongEntry wrongFromStmt(Stmt &st) {
    WrongEntry e;
    e.id = st.colInt64(0);
    e.wrongBookId = st.colInt64(1);
    e.wordId = st.colInt64(2);
    e.questionType = questionTypeFromKey(st.colText(3));
    e.wrongCount = st.colInt(4);
    e.correctCount = st.colInt(5);
    e.lastWrongAt = util::fromIso(st.colText(6));
    e.lastReviewAt = util::fromIso(st.colText(7));
    e.nextReviewAt = util::fromIso(st.colText(8));
    e.priority = wrongPriorityFromKey(st.colText(9));
    e.status = wrongStatusFromKey(st.colText(10));
    e.myAnswer = st.colText(11);
    e.remark = st.colText(12);
    e.updatedAt = util::fromIso(st.colText(13));
    e.word = mapper::wordFromStmt(st, kWrongColCount);
    return e;
}
} // namespace

qint64 WrongBookRepository::ensureBook(qint64 userId) {
    Stmt find(m_db.db(), QStringLiteral("SELECT id FROM wrong_books WHERE user_id=?"));
    find.bind(1, userId);
    if (find.isValid() && find.step()) return find.colInt64(0);

    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO wrong_books (user_id, name, created_at) VALUES (?,?,?)"));
    if (!st.isValid()) return -1;
    st.bind(1, userId).bind(2, QStringLiteral("我的错题本")).bind(3, util::nowIso());
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

std::optional<WrongBook> WrongBookRepository::findBook(qint64 id) {
    Stmt st(m_db.db(),
            QStringLiteral("SELECT b.id, b.user_id, b.name, b.created_at, "
                           "(SELECT COUNT(*) FROM wrong_entries e WHERE e.wrong_book_id=b.id) "
                           "FROM wrong_books b WHERE b.id=?"));
    st.bind(1, id);
    if (st.isValid() && st.step()) {
        WrongBook b;
        b.id = st.colInt64(0);
        b.userId = st.colInt64(1);
        b.name = st.colText(2);
        b.createdAt = util::fromIso(st.colText(3));
        b.entryCount = st.colInt(4);
        return b;
    }
    return std::nullopt;
}

std::optional<WrongBook> WrongBookRepository::findBookByUser(qint64 userId) {
    Stmt st(m_db.db(),
            QStringLiteral("SELECT b.id, b.user_id, b.name, b.created_at, "
                           "(SELECT COUNT(*) FROM wrong_entries e WHERE e.wrong_book_id=b.id) "
                           "FROM wrong_books b WHERE b.user_id=? LIMIT 1"));
    st.bind(1, userId);
    if (st.isValid() && st.step()) {
        WrongBook b;
        b.id = st.colInt64(0);
        b.userId = st.colInt64(1);
        b.name = st.colText(2);
        b.createdAt = util::fromIso(st.colText(3));
        b.entryCount = st.colInt(4);
        return b;
    }
    return std::nullopt;
}

qint64 WrongBookRepository::recordWrong(qint64 bookId, qint64 wordId, QuestionType type,
                                        const QString &myAnswer, const QString &remark) {
    Stmt find(m_db.db(), QStringLiteral("SELECT id, wrong_count, correct_count, status FROM wrong_entries "
                                        "WHERE wrong_book_id=? AND word_id=?"));
    find.bind(1, bookId).bind(2, wordId);
    const QString now = util::nowIso();

    if (find.isValid() && find.step()) {
        const qint64 id = find.colInt64(0);
        const int wrongCount = find.colInt(1) + 1;
        const int correctCount = find.colInt(2);
        const WrongStatus cur = wrongStatusFromKey(find.colText(3));
        const WrongPriority prio = rules::computePriority(wrongCount);
        const WrongStatus status = rules::computeStatus(wrongCount, correctCount, cur);
        Stmt up(m_db.db(),
                QStringLiteral("UPDATE wrong_entries SET wrong_count=?, last_wrong_at=?, priority=?, status=?, "
                               "question_type=?, my_answer=?, remark=?, updated_at=? WHERE id=?"));
        up.bind(1, wrongCount).bind(2, now).bind(3, toKey(prio)).bind(4, toKey(status))
            .bind(5, toKey(type)).bind(6, myAnswer).bind(7, remark).bind(8, now).bind(9, id);
        if (!up.run()) return -1;
        return id;
    }

    const WrongPriority prio = rules::computePriority(1);
    const WrongStatus status = rules::computeStatus(1, 0, WrongStatus::New);
    Stmt ins(m_db.db(),
             QStringLiteral("INSERT INTO wrong_entries (wrong_book_id, word_id, question_type, wrong_count, "
                            "correct_count, last_wrong_at, last_review_at, next_review_at, priority, status, "
                            "my_answer, remark, updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    if (!ins.isValid()) return -1;
    ins.bind(1, bookId).bind(2, wordId).bind(3, toKey(type)).bind(4, 1).bind(5, 0)
        .bind(6, now).bindNull(7).bind(8, util::toIso(QDateTime::currentDateTime().addDays(1)))
        .bind(9, toKey(prio)).bind(10, toKey(status)).bind(11, myAnswer).bind(12, remark).bind(13, now);
    if (!ins.run()) return -1;
    return m_db.db().lastInsertId();
}

bool WrongBookRepository::updateEntry(const WrongEntry &e) {
    Stmt st(m_db.db(),
            QStringLiteral("UPDATE wrong_entries SET question_type=?, wrong_count=?, correct_count=?, "
                           "last_wrong_at=?, last_review_at=?, next_review_at=?, priority=?, status=?, "
                           "my_answer=?, remark=?, updated_at=? WHERE id=?"));
    if (!st.isValid()) return false;
    st.bind(1, toKey(e.questionType)).bind(2, e.wrongCount).bind(3, e.correctCount);
    if (e.lastWrongAt.isValid()) st.bind(4, util::toIso(e.lastWrongAt)); else st.bindNull(4);
    if (e.lastReviewAt.isValid()) st.bind(5, util::toIso(e.lastReviewAt)); else st.bindNull(5);
    if (e.nextReviewAt.isValid()) st.bind(6, util::toIso(e.nextReviewAt)); else st.bindNull(6);
    st.bind(7, toKey(e.priority)).bind(8, toKey(e.status)).bind(9, e.myAnswer).bind(10, e.remark)
        .bind(11, util::nowIso()).bind(12, e.id);
    return st.run();
}

qint64 WrongBookRepository::upsertEntry(const WrongEntry &e) {
    Stmt find(m_db.db(), QStringLiteral("SELECT id FROM wrong_entries WHERE wrong_book_id=? AND word_id=?"));
    find.bind(1, e.wrongBookId).bind(2, e.wordId);
    const QString now = util::nowIso();
    if (find.isValid() && find.step()) {
        const qint64 id = find.colInt64(0);
        WrongEntry copy = e;
        copy.id = id;
        return updateEntry(copy) ? id : -1;
    }
    Stmt ins(m_db.db(),
             QStringLiteral("INSERT INTO wrong_entries (wrong_book_id, word_id, question_type, wrong_count, "
                            "correct_count, last_wrong_at, last_review_at, next_review_at, priority, status, "
                            "my_answer, remark, updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    if (!ins.isValid()) return -1;
    ins.bind(1, e.wrongBookId).bind(2, e.wordId).bind(3, toKey(e.questionType)).bind(4, e.wrongCount)
        .bind(5, e.correctCount);
    if (e.lastWrongAt.isValid()) ins.bind(6, util::toIso(e.lastWrongAt)); else ins.bindNull(6);
    if (e.lastReviewAt.isValid()) ins.bind(7, util::toIso(e.lastReviewAt)); else ins.bindNull(7);
    if (e.nextReviewAt.isValid()) ins.bind(8, util::toIso(e.nextReviewAt)); else ins.bindNull(8);
    ins.bind(9, toKey(e.priority)).bind(10, toKey(e.status)).bind(11, e.myAnswer).bind(12, e.remark)
        .bind(13, now);
    if (!ins.run()) return -1;
    return m_db.db().lastInsertId();
}

bool WrongBookRepository::removeEntry(qint64 entryId) {    Stmt st(m_db.db(), QStringLiteral("DELETE FROM wrong_entries WHERE id=?"));
    st.bind(1, entryId);
    return st.run();
}

bool WrongBookRepository::removeEntryByWord(qint64 bookId, qint64 wordId) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM wrong_entries WHERE wrong_book_id=? AND word_id=?"));
    st.bind(1, bookId).bind(2, wordId);
    return st.run();
}

std::optional<WrongEntry> WrongBookRepository::findEntry(qint64 bookId, qint64 wordId) {
    const QString sql = QStringLiteral("SELECT %1, %2 FROM wrong_entries e JOIN words w ON w.id=e.word_id "
                                       "WHERE e.wrong_book_id=? AND e.word_id=?")
                            .arg(QLatin1String(kWrongCols), mapper::wordColumns());
    Stmt st(m_db.db(), sql);
    st.bind(1, bookId).bind(2, wordId);
    if (st.isValid() && st.step()) return wrongFromStmt(st);
    return std::nullopt;
}

QList<WrongEntry> WrongBookRepository::listEntries(qint64 bookId, const WordQuery &q, int *totalOut) {
    QString where = QStringLiteral(" WHERE e.wrong_book_id=?");
    QList<QString> textBinds;
    if (!q.keyword.trimmed().isEmpty()) {
        where += QStringLiteral(" AND (w.spelling LIKE ? OR w.translation_cn LIKE ?)");
        const QString kw = QStringLiteral("%") + q.keyword.trimmed() + QStringLiteral("%");
        textBinds << kw << kw;
    }
    if (!q.wrongStatus.isEmpty()) {
        where += QStringLiteral(" AND e.status=?");
        textBinds << q.wrongStatus;
    }
    if (!q.book.isEmpty()) {
        where += QStringLiteral(" AND w.book=?");
        textBinds << q.book;
    }

    const QString join = QStringLiteral(" FROM wrong_entries e JOIN words w ON w.id=e.word_id") + where;

    if (totalOut) {
        Stmt cnt(m_db.db(), QStringLiteral("SELECT COUNT(*)") + join);
        int idx = 1;
        cnt.bind(idx++, bookId);
        for (const auto &b : textBinds) cnt.bind(idx++, b);
        *totalOut = (cnt.isValid() && cnt.step()) ? cnt.colInt(0) : 0;
    }

    QString order = QStringLiteral("e.wrong_count DESC, e.last_wrong_at DESC");
    if (q.orderBy == QLatin1String("spelling")) order = QStringLiteral("w.spelling COLLATE NOCASE ASC");
    else if (q.orderBy == QLatin1String("recent")) order = QStringLiteral("e.last_wrong_at DESC");

    const QString sql = QStringLiteral("SELECT %1, %2")
                            .arg(QLatin1String(kWrongCols), mapper::wordColumns()) +
                        join + QStringLiteral(" ORDER BY ") + order + QStringLiteral(" LIMIT ? OFFSET ?");
    Stmt st(m_db.db(), sql);
    QList<WrongEntry> out;
    if (!st.isValid()) return out;
    int idx = 1;
    st.bind(idx++, bookId);
    for (const auto &b : textBinds) st.bind(idx++, b);
    st.bind(idx++, q.limit).bind(idx++, q.offset);
    while (st.step()) out << wrongFromStmt(st);
    return out;
}

qint64 WrongBookRepository::countEntries(qint64 bookId) {
    Stmt st(m_db.db(), QStringLiteral("SELECT COUNT(*) FROM wrong_entries WHERE wrong_book_id=?"));
    st.bind(1, bookId);
    return (st.isValid() && st.step()) ? st.colInt64(0) : 0;
}

QList<qint64> WrongBookRepository::wordIdsOfBook(qint64 bookId) {
    QList<qint64> out;
    Stmt st(m_db.db(), QStringLiteral("SELECT e.word_id FROM wrong_entries e JOIN words w ON w.id=e.word_id "
                                      "WHERE e.wrong_book_id=? AND e.status<>'MASTERED' "
                                      "ORDER BY e.wrong_count DESC, e.id ASC"));
    st.bind(1, bookId);
    if (!st.isValid()) return out;
    while (st.step()) out << st.colInt64(0);
    return out;
}

QList<WrongEntry> WrongBookRepository::dueForReview(qint64 bookId, const QDateTime &before, int limit) {
    const QString sql = QStringLiteral("SELECT %1, %2 FROM wrong_entries e JOIN words w ON w.id=e.word_id "
                                       "WHERE e.wrong_book_id=? AND e.status<>'MASTERED' "
                                       "AND (e.next_review_at IS NULL OR e.next_review_at<=?) "
                                       "ORDER BY e.wrong_count DESC, e.next_review_at ASC LIMIT ?")
                            .arg(QLatin1String(kWrongCols), mapper::wordColumns());
    Stmt st(m_db.db(), sql);
    QList<WrongEntry> out;
    if (!st.isValid()) return out;
    st.bind(1, bookId).bind(2, util::toIso(before)).bind(3, limit);
    while (st.step()) out << wrongFromStmt(st);
    return out;
}

} // namespace vt
