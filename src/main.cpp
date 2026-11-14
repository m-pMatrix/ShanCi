//
// main.cpp - 程序入口
//
#include "core/AppContext.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "core/Paths.h"
#include "core/Utils.h"
#include "importer/JsonlImporter.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QIcon>
#include <QProgressDialog>

using namespace vt;

namespace {

// 尝试用“记住登录状态”的令牌自动登录（仅启动时一次）
bool tryAutoLogin(AppContext &ctx) {
    Config &cfg = Config::instance();
    const qint64 uid = cfg.getString(QStringLiteral("rememberUserId")).toLongLong();
    const QString token = cfg.getString(QStringLiteral("rememberToken"));
    if (uid <= 0 || token.isEmpty()) return false;

    const auto res = ctx.auth().loginWithRememberToken(uid, token);
    if (!res) {
        Logger::warn(QStringLiteral("Main"), QStringLiteral("自动登录失败: %1").arg(res.error()));
        cfg.set(QStringLiteral("rememberUserId"), QString());
        cfg.set(QStringLiteral("rememberToken"), QString());
        cfg.save();
        return false;
    }
    ctx.setCurrentUser(res.value());
    Logger::info(QStringLiteral("Main"), QStringLiteral("已自动登录: %1").arg(res.value().username));
    return true;
}

// 根据用户个人设置应用主题
void applyUserTheme(QApplication &app, AppContext &ctx) {
    const Setting s = ctx.currentSetting();
    if (!s.theme.isEmpty()) {
        theme::apply(app, s.theme);
        Config::instance().set(QStringLiteral("theme"), s.theme);
        Config::instance().save();
    }
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ShanCi"));
    QApplication::setOrganizationName(QStringLiteral("ShanCi"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    // 本地目录与日志
    Paths::init();
    Logger::init(Paths::logFile(), true);
    Config::instance().load(Paths::configFile());

    Logger::info(QStringLiteral("Main"), QStringLiteral("应用启动"));

    // 初始化应用上下文（数据库 + 服务）
    if (!AppContext::instance().init(Paths::databaseFile())) {
        Logger::error(QStringLiteral("Main"), QStringLiteral("数据库初始化失败"));
        return 1;
    }
    AppContext &ctx = AppContext::instance();

    // 主题
    const QString themeName = Config::instance().getString(QStringLiteral("theme"), QStringLiteral("light"));
    theme::apply(app, themeName);

    // 随机种子
    util::seedRandom(static_cast<unsigned int>(QDateTime::currentMSecsSinceEpoch()));

    // 首次运行/词库不完整：按词书逐个检查，缺失的内置词表自动补导
    {
        QDir dictDir(Paths::dictDir());
        const auto dictFiles = dictDir.entryInfoList({QStringLiteral("*.jsonl")}, QDir::Files, QDir::Name);
        QStringList pending;
        for (const auto &fi : dictFiles) {
            const BookType book = JsonlImporter::inferBook(fi.fileName());
            if (ctx.wordService().count(toKey(book)) == 0) pending << fi.absoluteFilePath();
        }
        if (!pending.isEmpty()) {
            QProgressDialog progress(QStringLiteral("正在导入内置词表，请稍候…"), QString(), 0, pending.size());
            progress.setWindowTitle(QStringLiteral("闪词"));
            progress.setWindowModality(Qt::ApplicationModal);
            progress.setMinimumDuration(0);
            progress.setValue(0);
            QApplication::processEvents();

            int done = 0;
            for (const auto &path : pending) {
                progress.setLabelText(QStringLiteral("正在导入 %1 …").arg(QFileInfo(path).fileName()));
                QApplication::processEvents();
                ctx.wordService().importJsonl(path, false);
                progress.setValue(++done);
                QApplication::processEvents();
            }
            progress.setValue(pending.size());
            Logger::info(QStringLiteral("Main"),
                         QStringLiteral("内置词表导入完成，共 %1 个文件").arg(pending.size()));
        }
    }

    int exitCode = 0;
    bool relogin = false;
    bool firstAttempt = true;
    do {
        relogin = false;

        bool loggedIn = false;
        if (firstAttempt && tryAutoLogin(ctx)) {
            // 已通过“记住登录状态”自动登录
            loggedIn = true;
        } else {
            LoginDialog login(ctx);
            if (login.exec() != QDialog::Accepted) break;

            const auto user = ctx.users().findById(login.loggedInUserId());
            if (!user) break;
            ctx.setCurrentUser(*user);
        }
        firstAttempt = false;
        Q_UNUSED(loggedIn);

        applyUserTheme(app, ctx);

        MainWindow window(ctx);
        QObject::connect(&window, &MainWindow::loggedOut, [&relogin] { relogin = true; });
        window.show();
        exitCode = app.exec();
    } while (relogin);

    Logger::info(QStringLiteral("Main"), QStringLiteral("应用退出"));
    AppContext::instance().shutdown();
    Logger::shutdown();
    return exitCode;
}
