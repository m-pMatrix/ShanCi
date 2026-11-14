//
// ImportTool.cpp - 命令行词表导入工具
//   用法:
//     vocab-import [--db <文件>] [--replace] <词表.jsonl> [...]
//     vocab-import --builtin        # 导入随程序分发的 data/dict 下全部词表
//
#include "core/Logger.h"
#include "core/Paths.h"
#include "data/Database.h"
#include "data/Repositories.h"
#include "importer/JsonlImporter.h"
#include "service/Services.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

using namespace vt;

static void printStats(QTextStream &out, const QString &name, const ImportStats &s) {
    out << QStringLiteral("[%1] 新增 %2  更新 %3  失败 %4  用时 %5 ms")
               .arg(name)
               .arg(s.inserted)
               .arg(s.updated)
               .arg(s.failed)
               .arg(s.elapsedMs)
        << Qt::endl;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("VocabTutor"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("四六级单词辅助背诵系统 - 词表导入工具"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dbOpt({QStringLiteral("d"), QStringLiteral("db")},
                             QStringLiteral("指定数据库文件（默认使用应用数据目录）"),
                             QStringLiteral("文件"));
    QCommandLineOption replaceOpt({QStringLiteral("r"), QStringLiteral("replace")},
                                  QStringLiteral("导入前清空同词书已有数据"));
    QCommandLineOption builtinOpt(QStringLiteral("builtin"),
                                  QStringLiteral("导入 data/dict 目录下的全部 JSONL 词表"));
    QCommandLineOption fillOpt(QStringLiteral("fill-wordbook"),
                               QStringLiteral("导入后把词库全部单词加入指定用户的单词本（all 表示所有用户）"),
                               QStringLiteral("用户名"));
    parser.addOption(dbOpt);
    parser.addOption(replaceOpt);
    parser.addOption(builtinOpt);
    parser.addOption(fillOpt);
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("要导入的 .jsonl 文件"));
    parser.process(app);

    QTextStream out(stdout);
    QTextStream err(stderr);

    Paths::init();
    const QString dbPath = parser.isSet(dbOpt) ? parser.value(dbOpt) : Paths::databaseFile();
    Logger::init(Paths::logFile(), false);

    if (!Database::instance().open(dbPath)) {
        err << QStringLiteral("无法打开数据库: %1").arg(dbPath) << Qt::endl;
        return 1;
    }
    out << QStringLiteral("数据库: %1").arg(dbPath) << Qt::endl;

    WordRepository repo(Database::instance());
    JsonlImporter importer(repo);
    const bool replace = parser.isSet(replaceOpt);

    QStringList files = parser.positionalArguments();
    if (parser.isSet(builtinOpt)) {
        const QString dir = Paths::dictDir();
        QDir d(dir);
        const auto entries = d.entryInfoList({QStringLiteral("*.jsonl")}, QDir::Files, QDir::Name);
        if (entries.isEmpty()) {
            err << QStringLiteral("未在 %1 找到任何 .jsonl 词表").arg(dir) << Qt::endl;
            return 1;
        }
        for (const auto &fi : entries) files << fi.absoluteFilePath();
    }

    if (files.isEmpty() && !parser.isSet(fillOpt)) {
        err << QStringLiteral("请指定要导入的文件，或使用 --builtin") << Qt::endl;
        return 1;
    }

    ImportStats total;
    for (const QString &f : files) {
        if (!QFileInfo::exists(f)) {
            err << QStringLiteral("文件不存在: %1").arg(f) << Qt::endl;
            continue;
        }
        const ImportStats s = importer.importFile(f, replace, [&](int done, int totalLines) {
            if (totalLines > 0 && done % 500 == 0)
                out << QStringLiteral("  进度 %1/%2").arg(done).arg(totalLines) << Qt::endl;
        });
        printStats(out, QFileInfo(f).fileName(), s);
        total.inserted += s.inserted;
        total.updated += s.updated;
        total.failed += s.failed;
    }

    if (!files.isEmpty())
        out << QStringLiteral("合计: 新增 %1  更新 %2  失败 %3")
                   .arg(total.inserted).arg(total.updated).arg(total.failed)
            << Qt::endl;

    // 可选：把词库全部单词加入指定用户的单词本
    if (parser.isSet(fillOpt)) {
        Database &db = Database::instance();
        UserRepository users(db);
        SettingRepository settings(db);
        WordBookRepository books(db);
        WrongBookRepository wrong(db);
        AuthService auth(users, settings, books, wrong);
        WordBookService bookService(books, auth, db);

        const QString who = parser.value(fillOpt).trimmed();
        QList<User> targets;
        if (who.compare(QStringLiteral("all"), Qt::CaseInsensitive) == 0) {
            targets = users.listAll();
        } else {
            const auto u = users.findByUsername(who);
            if (u) targets << *u;
        }
        if (targets.isEmpty()) {
            err << QStringLiteral("未找到用户: %1").arg(who) << Qt::endl;
            return 1;
        }
        for (const User &u : targets) {
            const int added = bookService.addAllDictionaryWords(u.id);
            out << QStringLiteral("用户 %1：单词本新增 %2 条（当前共 %3 条）")
                       .arg(u.username)
                       .arg(added)
                       .arg(books.countEntries(auth.defaultBookId(u.id)))
                << Qt::endl;
        }
    }

    out << QStringLiteral("词库总数: %1").arg(repo.countAll()) << Qt::endl;
    Database::instance().close();
    return total.failed > 0 ? 2 : 0;
}
