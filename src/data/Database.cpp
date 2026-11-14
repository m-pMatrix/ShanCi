#include "data/Database.h"

#include "core/Logger.h"

namespace vt {

Database &Database::instance() {
    static Database db;
    return db;
}

bool Database::open(const QString &filePath) {
    if (!m_db.open(filePath)) return false;
    return migrate();
}

void Database::close() { m_db.close(); }

bool Database::migrate() {
    if (!m_db.isOpen()) return false;
    return createTables();
}

bool Database::createTables() {
    static const char *kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    salt          TEXT NOT NULL,
    display_name  TEXT,
    created_at    TEXT NOT NULL,
    last_login_at TEXT
);

CREATE TABLE IF NOT EXISTS settings (
    user_id           INTEGER PRIMARY KEY,
    daily_word_count  INTEGER NOT NULL DEFAULT 20,
    question_types    TEXT NOT NULL DEFAULT 'EnToCn,CnToEn,SimilarMatch,Cloze',
    theme             TEXT NOT NULL DEFAULT 'light',
    sync_path         TEXT,
    font_size         INTEGER NOT NULL DEFAULT 13,
    audio_enabled     INTEGER NOT NULL DEFAULT 1,
    show_phonetic     INTEGER NOT NULL DEFAULT 1,
    auto_add_wrong    INTEGER NOT NULL DEFAULT 1,
    strict_spelling   INTEGER NOT NULL DEFAULT 1,
    updated_at        TEXT NOT NULL,
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS words (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    book            TEXT NOT NULL,
    word_rank       INTEGER DEFAULT 0,
    spelling        TEXT NOT NULL,
    phonetic_uk     TEXT,
    phonetic_us     TEXT,
    audio_uk        TEXT,
    audio_us        TEXT,
    star            INTEGER DEFAULT 0,
    pos             TEXT,
    translation_cn  TEXT,
    translation_en  TEXT,
    mem_method      TEXT,
    source          TEXT,
    phrases_json    TEXT,
    sentences_json  TEXT,
    synos_json      TEXT,
    relwords_json   TEXT,
    exam_json       TEXT,
    created_at      TEXT NOT NULL,
    UNIQUE(book, spelling)
);
CREATE INDEX IF NOT EXISTS idx_words_spelling ON words(spelling);
CREATE INDEX IF NOT EXISTS idx_words_book ON words(book, word_rank);

CREATE TABLE IF NOT EXISTS word_books (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL,
    name        TEXT NOT NULL,
    description TEXT,
    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL,
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS word_book_entries (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    word_book_id   INTEGER NOT NULL,
    word_id        INTEGER NOT NULL,
    familiarity    INTEGER NOT NULL DEFAULT 0,
    review_count   INTEGER NOT NULL DEFAULT 0,
    next_review_at TEXT,
    source         TEXT NOT NULL DEFAULT 'Manual',
    added_at       TEXT NOT NULL,
    UNIQUE(word_book_id, word_id),
    FOREIGN KEY(word_book_id) REFERENCES word_books(id) ON DELETE CASCADE,
    FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS wrong_books (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id    INTEGER NOT NULL,
    name       TEXT NOT NULL,
    created_at TEXT NOT NULL,
    UNIQUE(user_id)
);

CREATE TABLE IF NOT EXISTS wrong_entries (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    wrong_book_id  INTEGER NOT NULL,
    word_id        INTEGER NOT NULL,
    question_type  TEXT,
    wrong_count    INTEGER NOT NULL DEFAULT 0,
    correct_count  INTEGER NOT NULL DEFAULT 0,
    last_wrong_at  TEXT,
    last_review_at TEXT,
    next_review_at TEXT,
    priority       TEXT NOT NULL DEFAULT 'NORMAL',
    status         TEXT NOT NULL DEFAULT 'NEW',
    my_answer      TEXT,
    remark         TEXT,
    updated_at     TEXT NOT NULL,
    UNIQUE(wrong_book_id, word_id),
    FOREIGN KEY(wrong_book_id) REFERENCES wrong_books(id) ON DELETE CASCADE,
    FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS questions (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    type        TEXT NOT NULL,
    stem        TEXT NOT NULL,
    options_json TEXT,
    answer      TEXT NOT NULL,
    explanation TEXT,
    difficulty  INTEGER DEFAULT 1,
    word_id     INTEGER,
    created_at  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS papers (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER NOT NULL,
    title         TEXT NOT NULL,
    mode          TEXT NOT NULL,
    total_count   INTEGER DEFAULT 0,
    correct_count INTEGER DEFAULT 0,
    score         REAL DEFAULT 0,
    created_at    TEXT NOT NULL,
    finished_at   TEXT,
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS paper_entries (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    paper_id       INTEGER NOT NULL,
    question_id    INTEGER NOT NULL,
    seq            INTEGER NOT NULL,
    user_answer    TEXT,
    is_correct     INTEGER,
    answer_time_ms INTEGER DEFAULT 0,
    answered_at    TEXT,
    FOREIGN KEY(paper_id) REFERENCES papers(id) ON DELETE CASCADE,
    FOREIGN KEY(question_id) REFERENCES questions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS daily_lists (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id      INTEGER NOT NULL,
    list_date    TEXT NOT NULL,
    mode         TEXT NOT NULL,
    total_count  INTEGER DEFAULT 0,
    done_count   INTEGER DEFAULT 0,
    created_at   TEXT NOT NULL,
    completed_at TEXT,
    UNIQUE(user_id, list_date),
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS daily_entries (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    daily_list_id INTEGER NOT NULL,
    word_id       INTEGER NOT NULL,
    source_type   TEXT NOT NULL,
    seq           INTEGER NOT NULL,
    is_completed  INTEGER NOT NULL DEFAULT 0,
    UNIQUE(daily_list_id, word_id),
    FOREIGN KEY(daily_list_id) REFERENCES daily_lists(id) ON DELETE CASCADE,
    FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS sync_logs (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id       INTEGER NOT NULL,
    direction     TEXT NOT NULL,
    file_path     TEXT,
    item_count    INTEGER DEFAULT 0,
    conflict_count INTEGER DEFAULT 0,
    status        TEXT NOT NULL,
    message       TEXT,
    created_at    TEXT NOT NULL,
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value TEXT
);

CREATE TABLE IF NOT EXISTS study_plans (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id        INTEGER NOT NULL,
    name           TEXT NOT NULL,
    book           TEXT NOT NULL DEFAULT '',
    total_days     INTEGER NOT NULL DEFAULT 30,
    new_per_day    INTEGER NOT NULL DEFAULT 20,
    review_per_day INTEGER NOT NULL DEFAULT 20,
    include_wrong  INTEGER NOT NULL DEFAULT 1,
    start_date     TEXT NOT NULL,
    created_at     TEXT NOT NULL,
    active         INTEGER NOT NULL DEFAULT 1,
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_plans_user ON study_plans(user_id, active);

CREATE TABLE IF NOT EXISTS plan_words (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    plan_id        INTEGER NOT NULL,
    word_id        INTEGER NOT NULL,
    first_date     TEXT NOT NULL,
    learned        INTEGER NOT NULL DEFAULT 0,
    familiarity    INTEGER NOT NULL DEFAULT 0,
    review_count   INTEGER NOT NULL DEFAULT 0,
    next_review_at TEXT,
    last_review_at TEXT,
    UNIQUE(plan_id, word_id),
    FOREIGN KEY(plan_id) REFERENCES study_plans(id) ON DELETE CASCADE,
    FOREIGN KEY(word_id) REFERENCES words(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_plan_words_due ON plan_words(plan_id, learned, next_review_at);
)SQL";

    QString err;
    if (!m_db.exec(QString::fromUtf8(kSchema), &err)) {
        Logger::error(QStringLiteral("Database"), QStringLiteral("schema creation failed: %1").arg(err));
        return false;
    }
    Logger::info(QStringLiteral("Database"), QStringLiteral("schema ready"));
    return true;
}

qint64 Database::count(const QString &table) {
    return m_db.queryInt64(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table), 0);
}

QString Database::meta(const QString &key, const QString &def) {
    Stmt st(m_db, QStringLiteral("SELECT value FROM meta WHERE key=?"));
    st.bind(1, key);
    if (st.isValid() && st.step()) return st.colText(0);
    return def;
}

bool Database::setMeta(const QString &key, const QString &value) {
    Stmt st(m_db, QStringLiteral(
                       "INSERT INTO meta (key, value) VALUES (?,?) "
                       "ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
    st.bind(1, key).bind(2, value);
    return st.run();
}

} // namespace vt
