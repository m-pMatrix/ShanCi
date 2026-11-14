#include "core/Utils.h"
#include "data/Repositories.h"
#include "data/WordMapper.h"

namespace vt {

namespace {
DailyList dailyFromStmt(Stmt &st) {
    DailyList d;
    d.id = st.colInt64(0);
    d.userId = st.colInt64(1);
    d.listDate = st.colText(2);
    d.mode = paperModeFromKey(st.colText(3));
    d.totalCount = st.colInt(4);
    d.doneCount = st.colInt(5);
    d.createdAt = util::fromIso(st.colText(6));
    d.completedAt = util::fromIso(st.colText(7));
    return d;
}
const char *kDailyCols =
    "id, user_id, list_date, mode, total_count, done_count, created_at, completed_at";
const char *kDailyEntryCols = "e.id, e.daily_list_id, e.word_id, e.source_type, e.seq, e.is_completed";
constexpr int kDailyEntryColCount = 6;
} // namespace

std::optional<DailyList> DailyListRepository::findByDate(qint64 userId, const QString &date) {
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM daily_lists WHERE user_id=? AND list_date=?")
                           .arg(QLatin1String(kDailyCols)));
    st.bind(1, userId).bind(2, date);
    if (st.isValid() && st.step()) return dailyFromStmt(st);
    return std::nullopt;
}

std::optional<DailyList> DailyListRepository::findById(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM daily_lists WHERE id=?").arg(QLatin1String(kDailyCols)));
    st.bind(1, id);
    if (st.isValid() && st.step()) return dailyFromStmt(st);
    return std::nullopt;
}

QList<DailyList> DailyListRepository::listRecent(qint64 userId, int limit) {
    QList<DailyList> out;
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM daily_lists WHERE user_id=? ORDER BY list_date DESC LIMIT ?")
                           .arg(QLatin1String(kDailyCols)));
    st.bind(1, userId).bind(2, limit);
    if (!st.isValid()) return out;
    while (st.step()) out << dailyFromStmt(st);
    return out;
}

bool DailyListRepository::removeByDate(qint64 userId, const QString &date) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM daily_lists WHERE user_id=? AND list_date=?"));
    st.bind(1, userId).bind(2, date);
    return st.run();
}

qint64 DailyListRepository::createList(const DailyList &list) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO daily_lists (user_id, list_date, mode, total_count, done_count, "
                           "created_at, completed_at) VALUES (?,?,?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    st.bind(1, list.userId).bind(2, list.listDate).bind(3, toKey(list.mode)).bind(4, list.totalCount)
        .bind(5, list.doneCount).bind(6, util::nowIso());
    if (list.completedAt.isValid()) st.bind(7, util::toIso(list.completedAt)); else st.bindNull(7);
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

qint64 DailyListRepository::addEntry(const DailyEntry &e) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT OR IGNORE INTO daily_entries (daily_list_id, word_id, source_type, seq, "
                           "is_completed) VALUES (?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    st.bind(1, e.dailyListId).bind(2, e.wordId).bind(3, toKey(e.sourceType)).bind(4, e.seq)
        .bind(5, e.isCompleted ? 1 : 0);
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

bool DailyListRepository::setEntryCompleted(qint64 entryId, bool completed) {
    Stmt st(m_db.db(), QStringLiteral("UPDATE daily_entries SET is_completed=? WHERE id=?"));
    st.bind(1, completed ? 1 : 0).bind(2, entryId);
    return st.run();
}

bool DailyListRepository::updateProgress(qint64 listId, int doneCount, bool completed, const QDateTime &completedAt) {
    Stmt st(m_db.db(), QStringLiteral("UPDATE daily_lists SET done_count=?, completed_at=? WHERE id=?"));
    st.bind(1, doneCount);
    if (completed) st.bind(2, util::toIso(completedAt.isValid() ? completedAt : QDateTime::currentDateTime()));
    else st.bindNull(2);
    st.bind(3, listId);
    return st.run();
}

QList<DailyEntry> DailyListRepository::listEntries(qint64 listId) {
    QList<DailyEntry> out;
    const QString sql = QStringLiteral("SELECT %1, %2 FROM daily_entries e JOIN words w ON w.id=e.word_id "
                                       "WHERE e.daily_list_id=? ORDER BY e.seq ASC")
                            .arg(QLatin1String(kDailyEntryCols), mapper::wordColumns());
    Stmt st(m_db.db(), sql);
    st.bind(1, listId);
    if (!st.isValid()) return out;
    while (st.step()) {
        DailyEntry e;
        e.id = st.colInt64(0);
        e.dailyListId = st.colInt64(1);
        e.wordId = st.colInt64(2);
        e.sourceType = entrySourceFromKey(st.colText(3));
        e.seq = st.colInt(4);
        e.isCompleted = st.colInt(5) != 0;
        e.word = mapper::wordFromStmt(st, kDailyEntryColCount);
        out << e;
    }
    return out;
}

// ---------------------------------------------------------------------------
// SyncLogRepository
// ---------------------------------------------------------------------------
qint64 SyncLogRepository::add(const SyncLog &log) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO sync_logs (user_id, direction, file_path, item_count, conflict_count, "
                           "status, message, created_at) VALUES (?,?,?,?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    st.bind(1, log.userId).bind(2, toKey(log.direction)).bind(3, log.filePath).bind(4, log.itemCount)
        .bind(5, log.conflictCount).bind(6, toKey(log.status)).bind(7, log.message).bind(8, util::nowIso());
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

QList<SyncLog> SyncLogRepository::listRecent(qint64 userId, int limit) {
    QList<SyncLog> out;
    Stmt st(m_db.db(),
            QStringLiteral("SELECT id, user_id, direction, file_path, item_count, conflict_count, status, "
                           "message, created_at FROM sync_logs WHERE user_id=? ORDER BY created_at DESC LIMIT ?"));
    st.bind(1, userId).bind(2, limit);
    if (!st.isValid()) return out;
    while (st.step()) {
        SyncLog l;
        l.id = st.colInt64(0);
        l.userId = st.colInt64(1);
        l.direction = syncDirectionFromKey(st.colText(2));
        l.filePath = st.colText(3);
        l.itemCount = st.colInt(4);
        l.conflictCount = st.colInt(5);
        l.status = syncStatusFromKey(st.colText(6));
        l.message = st.colText(7);
        l.createdAt = util::fromIso(st.colText(8));
        out << l;
    }
    return out;
}

bool SyncLogRepository::clear(qint64 userId) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM sync_logs WHERE user_id=?"));
    st.bind(1, userId);
    return st.run();
}

} // namespace vt
