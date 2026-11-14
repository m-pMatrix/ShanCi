#pragma once
//
// Database.h - 数据库管理与建表/迁移（第六层）
//
#include "data/SqliteDb.h"

#include <QString>

namespace vt {

class Database {
public:
    Database() = default;

    static Database &instance();

    bool open(const QString &filePath);
    void close();
    SqliteDb &db() { return m_db; }

    bool isOpen() const { return m_db.isOpen(); }

    // 确保表结构存在（幂等）
    bool migrate();

    // 统计
    qint64 count(const QString &table);

    // 元数据（键值对，用于记录一次性初始化标记等）
    QString meta(const QString &key, const QString &def = QString());
    bool setMeta(const QString &key, const QString &value);

private:
    bool createTables();

    SqliteDb m_db;
};

} // namespace vt
