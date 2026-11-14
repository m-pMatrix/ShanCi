#include "core/Logger.h"
#include "core/Utils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace vt {

namespace {
QMutex g_mutex;
QString g_filePath;
bool g_initialized = false;

const char *levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO ";
    case LogLevel::Warn: return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "INFO ";
}

// GUI 子系统程序没有控制台，向 stderr 输出没有意义（也可能拖慢启动）
bool hasConsole() {
#ifdef Q_OS_WIN
    return GetConsoleWindow() != nullptr;
#else
    return true;
#endif
}
} // namespace

bool Logger::s_echo = true;

void Logger::init(const QString &filePath, bool echoToConsole) {
    QMutexLocker lock(&g_mutex);
    g_filePath = filePath;
    // 仅当确实存在控制台时才回显到 stderr
    s_echo = echoToConsole && hasConsole();
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    g_initialized = true;
}

void Logger::shutdown() {
    QMutexLocker lock(&g_mutex);
    g_initialized = false;
}

void Logger::log(LogLevel level, const QString &tag, const QString &message) {
    const QString line = QStringLiteral("%1 [%2] [%3] %4")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                  QString::fromLatin1(levelName(level)), tag, message);
    QMutexLocker lock(&g_mutex);
    if (s_echo) {
        const QByteArray utf8 = line.toUtf8();
        std::fprintf(stderr, "%s\n", utf8.constData());
    }
    if (g_initialized && !g_filePath.isEmpty()) {
        QFile f(g_filePath);
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream ts(&f);
            ts.setEncoding(QStringConverter::Utf8);
            ts << line << '\n';
            f.close();
        }
    }
}

void Logger::debug(const QString &tag, const QString &message) { log(LogLevel::Debug, tag, message); }
void Logger::info(const QString &tag, const QString &message) { log(LogLevel::Info, tag, message); }
void Logger::warn(const QString &tag, const QString &message) { log(LogLevel::Warn, tag, message); }
void Logger::error(const QString &tag, const QString &message) { log(LogLevel::Error, tag, message); }

} // namespace vt
