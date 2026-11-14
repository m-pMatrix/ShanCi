#pragma once
//
// Logger.h - 日志管理（第七层：基础设施）
//
#include <QString>

namespace vt {

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static void init(const QString &filePath, bool echoToConsole = true);
    static void shutdown();

    static void log(LogLevel level, const QString &tag, const QString &message);

    static void debug(const QString &tag, const QString &message);
    static void info(const QString &tag, const QString &message);
    static void warn(const QString &tag, const QString &message);
    static void error(const QString &tag, const QString &message);

private:
    static bool s_echo;
};

} // namespace vt
