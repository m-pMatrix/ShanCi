#include "core/Utils.h"
#include "data/Repositories.h"

namespace vt {

namespace {
const char *kPlanCols =
    "id, user_id, name, book, total_days, new_per_day, review_per_day, include_wrong, "
    "start_date, created_at, active";

StudyPlan planFromStmt(Stmt &st) {
    StudyPlan p;
    p.id = st.colInt64(0);
    p.userId = st.colInt64(1);
    p.name = st.colText(2);
    p.book = st.colText(3);
    p.totalDays = st.colInt(4);
    p.newPerDay = st.colInt(5);
    p.reviewPerDay = st.colInt(6);
    p.includeWrong = st.colInt(7) != 0;
    p.startDate = st.colText(8);
    p.createdAt = util::fromIso(st.colText(9));
    p.active = st.colInt(10) != 0;
    return p;
}
} // namespace

qint64 StudyPlanRepository::save(const StudyPlan &p) {
    if (p.id > 0) {
        Stmt st(m_db.db(),
                QStringLiteral("UPDATE study_plans SET name=?, book=?, total_days=?, new_per_day=?, "
                               "review_per_day=?, include_wrong=?, start_date=?, active=? WHERE id=?"));
        if (!st.isValid()) return -1;
        st.bind(1, p.name).bind(2, p.book).bind(3, p.totalDays).bind(4, p.newPerDay)
            .bind(5, p.reviewPerDay).bind(6, p.includeWrong ? 1 : 0).bind(7, p.startDate)
            .bind(8, p.active ? 1 : 0).bind(9, p.id);
        return st.run() ? p.id : -1;
    }
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO study_plans (user_id, name, book, total_days, new_per_day, "
                           "review_per_day, include_wrong, start_date, created_at, active) "
                           "VALUES (?,?,?,?,?,?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    const QString now = util::nowIso();
    const QString start = p.startDate.isEmpty() ? util::todayDate() : p.startDate;
    st.bind(1, p.userId).bind(2, p.name.isEmpty() ? QStringLiteral("我的背诵计划") : p.name)
        .bind(3, p.book).bind(4, p.totalDays).bind(5, p.newPerDay).bind(6, p.reviewPerDay)
        .bind(7, p.includeWrong ? 1 : 0).bind(8, start).bind(9, now).bind(10, p.active ? 1 : 0);
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

std::optional<StudyPlan> StudyPlanRepository::findById(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM study_plans WHERE id=?").arg(QLatin1String(kPlanCols)));
    st.bind(1, id);
    if (st.isValid() && st.step()) return planFromStmt(st);
    return std::nullopt;
}

std::optional<StudyPlan> StudyPlanRepository::activePlan(qint64 userId) {
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM study_plans WHERE user_id=? AND active=1 "
                                      "ORDER BY id DESC LIMIT 1")
                           .arg(QLatin1String(kPlanCols)));
    st.bind(1, userId);
    if (st.isValid() && st.step()) return planFromStmt(st);
    return std::nullopt;
}

QList<StudyPlan> StudyPlanRepository::listPlans(qint64 userId) {
    QList<StudyPlan> out;
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM study_plans WHERE user_id=? ORDER BY id DESC")
                           .arg(QLatin1String(kPlanCols)));
    st.bind(1, userId);
    if (!st.isValid()) return out;
    while (st.step()) out << planFromStmt(st);
    return out;
}

bool StudyPlanRepository::remove(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM study_plans WHERE id=?"));
    st.bind(1, id);
    return st.run();
}

bool StudyPlanRepository::setActive(qint64 userId, qint64 planId) {
    Stmt st(m_db.db(),
            QStringLiteral("UPDATE study_plans SET active = CASE WHEN id=? THEN 1 ELSE 0 END "
                           "WHERE user_id=?"));
    st.bind(1, planId).bind(2, userId);
    return st.run();
}

QList<qint64> StudyPlanRepository::nextNewWordIds(qint64 planId, const QString &book, int limit) {
    QList<qint64> out;
    QString sql = QStringLiteral(
        "SELECT w.id FROM words w WHERE w.id NOT IN "
        "(SELECT word_id FROM plan_words WHERE plan_id=?)");
    if (!book.isEmpty()) sql += QStringLiteral(" AND w.book=?");
    sql += QStringLiteral(" ORDER BY w.word_rank ASC, w.id ASC LIMIT ?");
    Stmt st(m_db.db(), sql);
    if (!st.isValid()) return out;
    st.bind(1, planId);
    int idx = 2;
    if (!book.isEmpty()) st.bind(idx++, book);
    st.bind(idx++, limit);
    while (st.step()) out << st.colInt64(0);
    return out;
}

QList<qint64> StudyPlanRepository::dueReviewWordIds(qint64 planId, const QDateTime &before, int limit) {
    QList<qint64> out;
    // 注意：这里不能限定 learned=1。
    // 之前被排入计划但用户尚未作答的单词（learned=0、next_review_at 为空）
    // 必须继续出现，否则它们既不会作为“新词”再次出现（已在计划中），
    // 也不会作为“复习”出现，造成永久漏背。
    Stmt st(m_db.db(),
            QStringLiteral("SELECT word_id FROM plan_words WHERE plan_id=? "
                           "AND (next_review_at IS NULL OR next_review_at<=?) "
                           "ORDER BY (next_review_at IS NULL) ASC, next_review_at ASC, id ASC LIMIT ?"));
    if (!st.isValid()) return out;
    st.bind(1, planId).bind(2, util::toIso(before)).bind(3, limit);
    while (st.step()) out << st.colInt64(0);
    return out;
}

bool StudyPlanRepository::addPlanWords(qint64 planId, const QList<qint64> &wordIds, const QString &date) {
    if (wordIds.isEmpty()) return true;
    Stmt st(m_db.db(),
            QStringLiteral("INSERT OR IGNORE INTO plan_words (plan_id, word_id, first_date, learned) "
                           "VALUES (?,?,?,0)"));
    if (!st.isValid()) return false;
    const bool inTx = m_db.db().begin();
    for (auto id : wordIds) {
        st.reset();
        st.bind(1, planId).bind(2, id).bind(3, date);
        st.run();
    }
    if (inTx) m_db.db().commit();
    return true;
}

bool StudyPlanRepository::recordAnswer(qint64 planId, qint64 wordId, int familiarity, int reviewCount,
                                       const QDateTime &nextReview) {
    Stmt st(m_db.db(),
            QStringLiteral("UPDATE plan_words SET learned=1, familiarity=?, review_count=?, "
                           "next_review_at=?, last_review_at=? WHERE plan_id=? AND word_id=?"));
    if (!st.isValid()) return false;
    st.bind(1, familiarity).bind(2, reviewCount);
    if (nextReview.isValid()) st.bind(3, util::toIso(nextReview)); else st.bindNull(3);
    st.bind(4, util::nowIso()).bind(5, planId).bind(6, wordId);
    return st.run();
}

QList<PlanWord> StudyPlanRepository::planWordsForWord(qint64 userId, qint64 wordId) {
    QList<PlanWord> out;
    Stmt st(m_db.db(),
            QStringLiteral("SELECT pw.id, pw.plan_id, pw.word_id, pw.first_date, pw.learned, "
                           "pw.familiarity, pw.review_count, pw.next_review_at, pw.last_review_at "
                           "FROM plan_words pw JOIN study_plans sp ON sp.id=pw.plan_id "
                           "WHERE pw.word_id=? AND sp.user_id=? AND sp.active=1"));
    if (!st.isValid()) return out;
    st.bind(1, wordId).bind(2, userId);
    while (st.step()) {
        PlanWord p;
        p.id = st.colInt64(0);
        p.planId = st.colInt64(1);
        p.wordId = st.colInt64(2);
        p.firstDate = st.colText(3);
        p.learned = st.colInt(4) != 0;
        p.familiarity = st.colInt(5);
        p.reviewCount = st.colInt(6);
        p.nextReviewAt = util::fromIso(st.colText(7));
        p.lastReviewAt = util::fromIso(st.colText(8));
        out << p;
    }
    return out;
}

qint64 StudyPlanRepository::countLearned(qint64 planId) {
    Stmt st(m_db.db(), QStringLiteral("SELECT COUNT(*) FROM plan_words WHERE plan_id=? AND learned=1"));
    st.bind(1, planId);
    return (st.isValid() && st.step()) ? st.colInt64(0) : 0;
}

qint64 StudyPlanRepository::countInPlan(qint64 planId) {
    Stmt st(m_db.db(), QStringLiteral("SELECT COUNT(*) FROM plan_words WHERE plan_id=?"));
    st.bind(1, planId);
    return (st.isValid() && st.step()) ? st.colInt64(0) : 0;
}

} // namespace vt
