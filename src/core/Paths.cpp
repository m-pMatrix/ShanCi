#include "core/Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace vt {

namespace {
QString g_appRoot;
}

bool Paths::ensureDir(const QString &path) {
    QDir d;
    if (d.exists(path)) return true;
    return d.mkpath(path);
}

void Paths::init(const QString &appDir) {
    if (!appDir.isEmpty()) {
        g_appRoot = appDir;
    } else {
        // 便携式：优先使用可执行文件同级的 data 目录，便于随软件打包
        const QString exeDir = QCoreApplication::applicationDirPath();
        const QString portable = exeDir + QStringLiteral("/data");
        // 若可执行目录可写则使用便携数据目录，否则退回系统数据目录
        QDir d;
        bool portableOk = false;
        if (d.exists(portable)) {
            portableOk = true;
        } else {
            portableOk = d.mkpath(portable);
        }
        if (portableOk) {
            g_appRoot = portable;
        } else {
            g_appRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        }
    }
    ensureDir(g_appRoot);
}

QString Paths::appRoot() { return g_appRoot; }
QString Paths::databaseFile() { return g_appRoot + QStringLiteral("/vocab.db"); }
QString Paths::audioDir() { return g_appRoot + QStringLiteral("/audio"); }
QString Paths::syncDir() { return g_appRoot + QStringLiteral("/sync"); }
QString Paths::logFile() { return g_appRoot + QStringLiteral("/vocab.log"); }
QString Paths::configFile() { return g_appRoot + QStringLiteral("/config.json"); }

QString Paths::dictDir() {
    // 优先使用随程序分发的词表目录
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/data/dict"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../data/dict"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../data/dict"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../../../data/dict"),
    };
    QDir d;
    for (const auto &c : candidates) {
        if (d.exists(c)) return QDir::cleanPath(c);
    }
    return QDir::cleanPath(candidates.first());
}

} // namespace vt
