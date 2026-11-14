#include "data/SqliteDb.h"

#include "core/Logger.h"

#include <QDir>
#include <QFileInfo>

#include "sqlite3.h"

namespace vt {

// ---------------------------------------------------------------------------
// Stmt
// ---------------------------------------------------------------------------
Stmt::Stmt(SqliteDb &db, const QString &sql) : m_db(db) {
    if (!db.isOpen()) {
        Logger::error(QStringLiteral("Sqlite"), QStringLiteral("prepare on closed db: %1").arg(sql));
        return;
    }
    if (sqlite3_prepare_v2(db.handle(), sql.toUtf8().constData(), -1, &m_stmt, nullptr) != SQLITE_OK) {
        Logger::error(QStringLiteral("Sqlite"),
                      QStringLiteral("prepare failed: %1 | %2")
                          .arg(sql, QString::fromUtf8(sqlite3_errmsg(db.handle()))));
        m_stmt = nullptr;
    }
}

Stmt::~Stmt() {
    if (m_stmt) sqlite3_finalize(m_stmt);
}

QString Stmt::error() const {
    return QString::fromUtf8(sqlite3_errmsg(m_db.handle()));
}

Stmt &Stmt::bind(int index, const QString &v) {
    if (m_stmt) sqlite3_bind_text(m_stmt, index, v.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    return *this;
}
Stmt &Stmt::bind(int index, int v) {
    if (m_stmt) sqlite3_bind_int(m_stmt, index, v);
    return *this;
}
Stmt &Stmt::bind(int index, qint64 v) {
    if (m_stmt) sqlite3_bind_int64(m_stmt, index, v);
    return *this;
}
Stmt &Stmt::bind(int index, double v) {
    if (m_stmt) sqlite3_bind_double(m_stmt, index, v);
    return *this;
}
Stmt &Stmt::bindNull(int index) {
    if (m_stmt) sqlite3_bind_null(m_stmt, index);
    return *this;
}

bool Stmt::step() {
    if (!m_stmt) return false;
    const int rc = sqlite3_step(m_stmt);
    return rc == SQLITE_ROW;
}

bool Stmt::run() {
    if (!m_stmt) return false;
    const int rc = sqlite3_step(m_stmt);
    if (rc != SQLITE_DONE) {
        Logger::error(QStringLiteral("Sqlite"), QStringLiteral("step failed: %1").arg(error()));
        return false;
    }
    return true;
}

void Stmt::reset() {
    if (m_stmt) {
        sqlite3_reset(m_stmt);
        sqlite3_clear_bindings(m_stmt);
    }
}

QString Stmt::colText(int col) const {
    if (!m_stmt) return QString();
    const unsigned char *p = sqlite3_column_text(m_stmt, col);
    if (!p) return QString();
    return QString::fromUtf8(reinterpret_cast<const char *>(p));
}
int Stmt::colInt(int col) const { return m_stmt ? sqlite3_column_int(m_stmt, col) : 0; }
qint64 Stmt::colInt64(int col) const { return m_stmt ? sqlite3_column_int64(m_stmt, col) : 0; }
double Stmt::colDouble(int col) const { return m_stmt ? sqlite3_column_double(m_stmt, col) : 0.0; }
bool Stmt::colIsNull(int col) const {
    return !m_stmt || sqlite3_column_type(m_stmt, col) == SQLITE_NULL;
}

// ---------------------------------------------------------------------------
// SqliteDb
// ---------------------------------------------------------------------------
SqliteDb::~SqliteDb() { close(); }

bool SqliteDb::open(const QString &filePath) {
    close();
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    const int rc = sqlite3_open_v2(filePath.toUtf8().constData(), &m_db,
                                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        Logger::error(QStringLiteral("Sqlite"),
                      QStringLiteral("open failed: %1").arg(QString::fromUtf8(sqlite3_errmsg(m_db))));
        if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
        return false;
    }
    sqlite3_busy_timeout(m_db, 5000);
    exec(QStringLiteral("PRAGMA foreign_keys = ON;"));
    exec(QStringLiteral("PRAGMA journal_mode = WAL;"));
    exec(QStringLiteral("PRAGMA synchronous = NORMAL;"));
    return true;
}

void SqliteDb::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool SqliteDb::exec(const QString &sql, QString *err) {
    if (!m_db) {
        if (err) *err = QStringLiteral("database not open");
        return false;
    }
    char *msg = nullptr;
    const int rc = sqlite3_exec(m_db, sql.toUtf8().constData(), nullptr, nullptr, &msg);
    if (rc != SQLITE_OK) {
        const QString e = msg ? QString::fromUtf8(msg) : lastError();
        if (err) *err = e;
        Logger::error(QStringLiteral("Sqlite"), QStringLiteral("exec failed: %1 | %2").arg(e, sql));
        if (msg) sqlite3_free(msg);
        return false;
    }
    if (msg) sqlite3_free(msg);
    return true;
}

qint64 SqliteDb::lastInsertId() const { return m_db ? sqlite3_last_insert_rowid(m_db) : 0; }
int SqliteDb::changes() const { return m_db ? sqlite3_changes(m_db) : 0; }
QString SqliteDb::lastError() const {
    return m_db ? QString::fromUtf8(sqlite3_errmsg(m_db)) : QString();
}

bool SqliteDb::begin() { return exec(QStringLiteral("BEGIN TRANSACTION;")); }
bool SqliteDb::commit() { return exec(QStringLiteral("COMMIT;")); }
bool SqliteDb::rollback() { return exec(QStringLiteral("ROLLBACK;")); }

qint64 SqliteDb::queryInt64(const QString &sql, qint64 def) {
    Stmt st(*this, sql);
    if (st.isValid() && st.step()) return st.colInt64(0);
    return def;
}

} // namespace vt
