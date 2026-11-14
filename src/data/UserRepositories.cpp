#include "core/Utils.h"
#include "data/Repositories.h"

namespace vt {

// ---------------------------------------------------------------------------
// UserRepository
// ---------------------------------------------------------------------------
namespace {
User userFromStmt(Stmt &st) {
    User u;
    u.id = st.colInt64(0);
    u.username = st.colText(1);
    u.passwordHash = st.colText(2);
    u.salt = st.colText(3);
    u.displayName = st.colText(4);
    u.createdAt = util::fromIso(st.colText(5));
    u.lastLoginAt = util::fromIso(st.colText(6));
    return u;
}
const char *kUserCols = "id, username, password_hash, salt, display_name, created_at, last_login_at";
} // namespace

std::optional<User> UserRepository::findById(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM users WHERE id=?").arg(QLatin1String(kUserCols)));
    st.bind(1, id);
    if (st.isValid() && st.step()) return userFromStmt(st);
    return std::nullopt;
}

std::optional<User> UserRepository::findByUsername(const QString &username) {
    Stmt st(m_db.db(),
            QStringLiteral("SELECT %1 FROM users WHERE username=? COLLATE NOCASE").arg(QLatin1String(kUserCols)));
    st.bind(1, username.trimmed());
    if (st.isValid() && st.step()) return userFromStmt(st);
    return std::nullopt;
}

QList<User> UserRepository::listAll() {
    QList<User> out;
    Stmt st(m_db.db(),
            QStringLiteral("SELECT %1 FROM users ORDER BY last_login_at DESC, id ASC").arg(QLatin1String(kUserCols)));
    if (!st.isValid()) return out;
    while (st.step()) out << userFromStmt(st);
    return out;
}

qint64 UserRepository::create(const User &u) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO users (username, password_hash, salt, display_name, created_at, last_login_at) "
                           "VALUES (?,?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    st.bind(1, u.username.trimmed()).bind(2, u.passwordHash).bind(3, u.salt)
        .bind(4, u.displayName.isEmpty() ? u.username.trimmed() : u.displayName)
        .bind(5, util::nowIso()).bind(6, util::nowIso());
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

bool UserRepository::updateLastLogin(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("UPDATE users SET last_login_at=? WHERE id=?"));
    st.bind(1, util::nowIso()).bind(2, id);
    return st.run();
}

bool UserRepository::updatePassword(qint64 id, const QString &passwordHash, const QString &salt) {
    Stmt st(m_db.db(), QStringLiteral("UPDATE users SET password_hash=?, salt=? WHERE id=?"));
    st.bind(1, passwordHash).bind(2, salt).bind(3, id);
    return st.run();
}

bool UserRepository::exists(const QString &username) {
    Stmt st(m_db.db(), QStringLiteral("SELECT COUNT(*) FROM users WHERE username=? COLLATE NOCASE"));
    st.bind(1, username.trimmed());
    return st.isValid() && st.step() && st.colInt(0) > 0;
}

// ---------------------------------------------------------------------------
// SettingRepository
// ---------------------------------------------------------------------------
Setting SettingRepository::load(qint64 userId) {
    Setting s;
    s.userId = userId;
    Stmt st(m_db.db(),
            QStringLiteral("SELECT daily_word_count, question_types, theme, sync_path, font_size, "
                           "audio_enabled, show_phonetic, auto_add_wrong, strict_spelling, updated_at "
                           "FROM settings WHERE user_id=?"));
    st.bind(1, userId);
    if (st.isValid() && st.step()) {
        s.dailyWordCount = st.colInt(0);
        s.questionTypes = questionTypesFromKeys(util::splitCsv(st.colText(1)));
        s.theme = st.colText(2);
        s.syncPath = st.colText(3);
        s.fontSize = st.colInt(4);
        s.audioEnabled = st.colInt(5) != 0;
        s.showPhonetic = st.colInt(6) != 0;
        s.autoAddWrong = st.colInt(7) != 0;
        s.checkSpellingStrict = st.colInt(8) != 0;
        s.updatedAt = util::fromIso(st.colText(9));
    } else {
        // 返回默认值
        s.updatedAt = QDateTime::currentDateTime();
    }
    return s;
}

bool SettingRepository::save(const Setting &s) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO settings (user_id, daily_word_count, question_types, theme, sync_path, "
                           "font_size, audio_enabled, show_phonetic, auto_add_wrong, strict_spelling, updated_at) "
                           "VALUES (?,?,?,?,?,?,?,?,?,?,?) "
                           "ON CONFLICT(user_id) DO UPDATE SET "
                           "daily_word_count=excluded.daily_word_count, question_types=excluded.question_types, "
                           "theme=excluded.theme, sync_path=excluded.sync_path, font_size=excluded.font_size, "
                           "audio_enabled=excluded.audio_enabled, show_phonetic=excluded.show_phonetic, "
                           "auto_add_wrong=excluded.auto_add_wrong, strict_spelling=excluded.strict_spelling, "
                           "updated_at=excluded.updated_at"));
    if (!st.isValid()) return false;
    st.bind(1, s.userId).bind(2, s.dailyWordCount)
        .bind(3, util::joinCsv(questionTypesToKeys(s.questionTypes)))
        .bind(4, s.theme).bind(5, s.syncPath).bind(6, s.fontSize)
        .bind(7, s.audioEnabled ? 1 : 0).bind(8, s.showPhonetic ? 1 : 0)
        .bind(9, s.autoAddWrong ? 1 : 0).bind(10, s.checkSpellingStrict ? 1 : 0)
        .bind(11, util::nowIso());
    return st.run();
}

bool SettingRepository::remove(qint64 userId) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM settings WHERE user_id=?"));
    st.bind(1, userId);
    return st.run();
}

bool SettingRepository::exists(qint64 userId) {
    Stmt st(m_db.db(), QStringLiteral("SELECT COUNT(*) FROM settings WHERE user_id=?"));
    st.bind(1, userId);
    return st.isValid() && st.step() && st.colInt(0) > 0;
}

} // namespace vt
