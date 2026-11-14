#pragma once
//
// SqliteDb.h - SQLite 轻量封装（第六层：数据访问与存储）
//
#include <QString>

struct sqlite3;
struct sqlite3_stmt;

namespace vt {

class SqliteDb;

// 预编译语句，RAII
class Stmt {
public:
    Stmt(SqliteDb &db, const QString &sql);
    ~Stmt();

    Stmt(const Stmt &) = delete;
    Stmt &operator=(const Stmt &) = delete;

    bool isValid() const { return m_stmt != nullptr; }
    QString error() const;

    Stmt &bind(int index, const QString &v);
    Stmt &bind(int index, int v);
    Stmt &bind(int index, qint64 v);
    Stmt &bind(int index, double v);
    Stmt &bindNull(int index);

    // 执行一步，返回 true 表示有一行数据
    bool step();
    // 执行写操作（无结果集）
    bool run();

    void reset();

    // 列读取（0-based）
    QString colText(int col) const;
    int colInt(int col) const;
    qint64 colInt64(int col) const;
    double colDouble(int col) const;
    bool colIsNull(int col) const;

private:
    SqliteDb &m_db;
    sqlite3_stmt *m_stmt = nullptr;
};

class SqliteDb {
public:
    SqliteDb() = default;
    ~SqliteDb();

    SqliteDb(const SqliteDb &) = delete;
    SqliteDb &operator=(const SqliteDb &) = delete;

    bool open(const QString &filePath);
    void close();
    bool isOpen() const { return m_db != nullptr; }

    bool exec(const QString &sql, QString *err = nullptr);

    qint64 lastInsertId() const;
    int changes() const;
    QString lastError() const;

    bool begin();
    bool commit();
    bool rollback();

    sqlite3 *handle() const { return m_db; }

    // 便捷：单值查询
    qint64 queryInt64(const QString &sql, qint64 def = 0);

private:
    sqlite3 *m_db = nullptr;
};

} // namespace vt
