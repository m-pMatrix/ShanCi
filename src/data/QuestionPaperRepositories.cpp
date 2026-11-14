#include "core/Utils.h"
#include "data/Repositories.h"

#include <QJsonDocument>
#include <QJsonArray>

namespace vt {

namespace {
Question questionFromStmt(Stmt &st, int base) {
    Question q;
    q.id = st.colInt64(base + 0);
    q.type = questionTypeFromKey(st.colText(base + 1));
    q.stem = st.colText(base + 2);
    q.options = util::fromJsonArray(QJsonDocument::fromJson(st.colText(base + 3).toUtf8()).array());
    q.answer = st.colText(base + 4);
    q.explanation = st.colText(base + 5);
    q.difficulty = st.colInt(base + 6);
    q.wordId = st.colInt64(base + 7);
    q.createdAt = util::fromIso(st.colText(base + 8));
    return q;
}
const char *kPaperEntryCols =
    "pe.id, pe.paper_id, pe.question_id, pe.seq, pe.user_answer, pe.is_correct, pe.answer_time_ms, pe.answered_at";
constexpr int kPaperEntryColCount = 8;

// 题目表列（带别名，供 JOIN 使用）
QString questionColumns(const QString &alias = QString()) {
    const QString a = alias.isEmpty() ? QString() : alias + QLatin1Char('.');
    const QStringList cols = {QStringLiteral("id"), QStringLiteral("type"), QStringLiteral("stem"),
                              QStringLiteral("options_json"), QStringLiteral("answer"),
                              QStringLiteral("explanation"), QStringLiteral("difficulty"),
                              QStringLiteral("word_id"), QStringLiteral("created_at")};
    QStringList out;
    for (const auto &c : cols) out << a + c;
    return out.join(QStringLiteral(", "));
}
} // namespace

// ---------------------------------------------------------------------------
// QuestionRepository
// ---------------------------------------------------------------------------
qint64 QuestionRepository::insert(const Question &q) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO questions (type, stem, options_json, answer, explanation, difficulty, "
                           "word_id, created_at) VALUES (?,?,?,?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    st.bind(1, toKey(q.type)).bind(2, q.stem).bind(3, util::compactJson(util::toJsonArray(q.options)))
        .bind(4, q.answer).bind(5, q.explanation).bind(6, q.difficulty).bind(7, q.wordId)
        .bind(8, util::nowIso());
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

std::optional<Question> QuestionRepository::findById(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM questions WHERE id=?").arg(questionColumns()));
    st.bind(1, id);
    if (st.isValid() && st.step()) return questionFromStmt(st, 0);
    return std::nullopt;
}

QList<Question> QuestionRepository::findByIds(const QList<qint64> &ids) {    QList<Question> out;
    if (ids.isEmpty()) return out;
    QStringList ph;
    for (int i = 0; i < ids.size(); ++i) ph << QStringLiteral("?");
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM questions WHERE id IN (%2)")
                           .arg(questionColumns(), ph.join(QStringLiteral(","))));
    if (!st.isValid()) return out;
    for (int i = 0; i < ids.size(); ++i) st.bind(i + 1, ids[i]);
    while (st.step()) out << questionFromStmt(st, 0);
    return out;
}

bool QuestionRepository::remove(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM questions WHERE id=?"));
    st.bind(1, id);
    return st.run();
}

// ---------------------------------------------------------------------------
// PaperRepository
// ---------------------------------------------------------------------------
qint64 PaperRepository::createPaper(const Paper &p) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO papers (user_id, title, mode, total_count, correct_count, score, "
                           "created_at, finished_at) VALUES (?,?,?,?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    st.bind(1, p.userId).bind(2, p.title).bind(3, toKey(p.mode)).bind(4, p.totalCount)
        .bind(5, p.correctCount).bind(6, p.score).bind(7, util::nowIso());
    if (p.finishedAt.isValid()) st.bind(8, util::toIso(p.finishedAt)); else st.bindNull(8);
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

bool PaperRepository::finishPaper(qint64 paperId, int correct, int total, double score,
                                  const QDateTime &finishedAt) {
    Stmt st(m_db.db(),
            QStringLiteral("UPDATE papers SET correct_count=?, total_count=?, score=?, finished_at=? WHERE id=?"));
    st.bind(1, correct).bind(2, total).bind(3, score)
        .bind(4, util::toIso(finishedAt.isValid() ? finishedAt : QDateTime::currentDateTime()))
        .bind(5, paperId);
    return st.run();
}

std::optional<Paper> PaperRepository::findPaper(qint64 id) {
    Stmt st(m_db.db(),
            QStringLiteral("SELECT id, user_id, title, mode, total_count, correct_count, score, created_at, "
                           "finished_at FROM papers WHERE id=?"));
    st.bind(1, id);
    if (st.isValid() && st.step()) {
        Paper p;
        p.id = st.colInt64(0);
        p.userId = st.colInt64(1);
        p.title = st.colText(2);
        p.mode = paperModeFromKey(st.colText(3));
        p.totalCount = st.colInt(4);
        p.correctCount = st.colInt(5);
        p.score = st.colDouble(6);
        p.createdAt = util::fromIso(st.colText(7));
        p.finishedAt = util::fromIso(st.colText(8));
        return p;
    }
    return std::nullopt;
}

QList<Paper> PaperRepository::listPapers(qint64 userId, int limit) {
    QList<Paper> out;
    Stmt st(m_db.db(),
            QStringLiteral("SELECT id, user_id, title, mode, total_count, correct_count, score, created_at, "
                           "finished_at FROM papers WHERE user_id=? ORDER BY created_at DESC LIMIT ?"));
    st.bind(1, userId).bind(2, limit);
    if (!st.isValid()) return out;
    while (st.step()) {
        Paper p;
        p.id = st.colInt64(0);
        p.userId = st.colInt64(1);
        p.title = st.colText(2);
        p.mode = paperModeFromKey(st.colText(3));
        p.totalCount = st.colInt(4);
        p.correctCount = st.colInt(5);
        p.score = st.colDouble(6);
        p.createdAt = util::fromIso(st.colText(7));
        p.finishedAt = util::fromIso(st.colText(8));
        out << p;
    }
    return out;
}

bool PaperRepository::remove(qint64 paperId) {
    // paper_entries 通过外键级联删除
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM papers WHERE id=?"));
    st.bind(1, paperId);
    return st.run();
}

qint64 PaperRepository::addEntry(const PaperEntry &e) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO paper_entries (paper_id, question_id, seq, user_answer, is_correct, "
                           "answer_time_ms, answered_at) VALUES (?,?,?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    st.bind(1, e.paperId).bind(2, e.questionId).bind(3, e.seq);
    if (e.answered) st.bind(4, e.userAnswer); else st.bindNull(4);
    if (e.answered) st.bind(5, e.isCorrect ? 1 : 0); else st.bindNull(5);
    st.bind(6, e.answerTimeMs);
    if (e.answered) st.bind(7, util::toIso(e.answeredAt.isValid() ? e.answeredAt : QDateTime::currentDateTime()));
    else st.bindNull(7);
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

bool PaperRepository::updateEntryAnswer(qint64 entryId, const QString &userAnswer, bool isCorrect, qint64 timeMs) {
    Stmt st(m_db.db(),
            QStringLiteral("UPDATE paper_entries SET user_answer=?, is_correct=?, answer_time_ms=?, answered_at=? "
                           "WHERE id=?"));
    st.bind(1, userAnswer).bind(2, isCorrect ? 1 : 0).bind(3, timeMs).bind(4, util::nowIso()).bind(5, entryId);
    return st.run();
}

std::optional<PaperEntry> PaperRepository::findEntry(qint64 entryId) {
    const QString sql = QStringLiteral("SELECT %1, %2 FROM paper_entries pe JOIN questions q ON q.id=pe.question_id "
                                       "WHERE pe.id=?")
                            .arg(QLatin1String(kPaperEntryCols), questionColumns(QStringLiteral("q")));
    Stmt st(m_db.db(), sql);
    st.bind(1, entryId);
    if (st.isValid() && st.step()) {
        PaperEntry e;
        e.id = st.colInt64(0);
        e.paperId = st.colInt64(1);
        e.questionId = st.colInt64(2);
        e.seq = st.colInt(3);
        e.userAnswer = st.colText(4);
        e.answered = !st.colIsNull(5);
        e.isCorrect = st.colInt(5) != 0;
        e.answerTimeMs = st.colInt64(6);
        e.answeredAt = util::fromIso(st.colText(7));
        e.question = questionFromStmt(st, kPaperEntryColCount);
        return e;
    }
    return std::nullopt;
}

QList<PaperEntry> PaperRepository::listEntries(qint64 paperId) {    QList<PaperEntry> out;
    const QString sql = QStringLiteral("SELECT %1, %2 FROM paper_entries pe JOIN questions q ON q.id=pe.question_id "
                                       "WHERE pe.paper_id=? ORDER BY pe.seq ASC")
                            .arg(QLatin1String(kPaperEntryCols), questionColumns(QStringLiteral("q")));
    Stmt st(m_db.db(), sql);
    st.bind(1, paperId);
    if (!st.isValid()) return out;
    while (st.step()) {
        PaperEntry e;
        e.id = st.colInt64(0);
        e.paperId = st.colInt64(1);
        e.questionId = st.colInt64(2);
        e.seq = st.colInt(3);
        e.userAnswer = st.colText(4);
        e.answered = !st.colIsNull(5);
        e.isCorrect = st.colInt(5) != 0;
        e.answerTimeMs = st.colInt64(6);
        e.answeredAt = util::fromIso(st.colText(7));
        e.question = questionFromStmt(st, kPaperEntryColCount);
        out << e;
    }
    return out;
}

} // namespace vt
