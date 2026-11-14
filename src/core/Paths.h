#pragma once
//
// Paths.h - 本地目录管理（第六层：数据存储位置）
//
#include <QString>

namespace vt {

// 应用所有本地路径的单一来源
class Paths {
public:
    // 初始化（在 main 中调用一次）
    static void init(const QString &appDir = QString());

    static QString appRoot();       // 应用数据根目录
    static QString databaseFile();  // SQLite 数据库文件
    static QString audioDir();      // 音频缓存目录
    static QString syncDir();       // 同步文件默认目录
    static QString dictDir();       // 内置词表目录（data/dict）
    static QString logFile();       // 日志文件
    static QString configFile();    // 应用级配置

    static bool ensureDir(const QString &path);
};

} // namespace vt
