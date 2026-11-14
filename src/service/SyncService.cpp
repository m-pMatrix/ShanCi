#include "core/Logger.h"
#include "core/Utils.h"
#include "service/Services.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace vt {

namespace {
const char *kFormat = "vocab-tutor-sync";
const int kVersion = 1;

QJsonObject wrongToJson(const WrongEntry &e) {
    QJsonObject o;
    o.insert(QStringLiteral("spelling"), e.word.spelling);
    o.insert(QStringLiteral("book"), toKey(e.word.book));
    o.insert(QStringLiteral("questionType"), toKey(e.questionType));
    o.insert(QStringLiteral("wrongCount"), e.wrongCount);
    o.insert(QStringLiteral("correctCount"), e.correctCount);
    o.insert(QStringLiteral("lastWrongAt"), util::toIso(e.lastWrongAt));
    o.insert(QStringLiteral("lastReviewAt"), util::toIso(e.lastReviewAt));
    o.insert(QStringLiteral("nextReviewAt"), util::toIso(e.nextReviewAt));
    o.insert(QStringLiteral("priority"), toKey(e.priority));
    o.insert(QStringLiteral("status"), toKey(e.status));
    o.insert(QStringLiteral("myAnswer"), e.myAnswer);
    o.insert(QStringLiteral("remark"), e.remark);
    o.insert(QStringLiteral("updatedAt"), util::toIso(e.updatedAt));
    return o;
}

WrongEntry wrongFromJson(const QJsonObject &o) {
    WrongEntry e;
    e.questionType = questionTypeFromKey(o.value(QStringLiteral("questionType")).toString());
    e.wrongCount = o.value(QStringLiteral("wrongCount")).toInt();
    e.correctCount = o.value(QStringLiteral("correctCount")).toInt();
    e.lastWrongAt = util::fromIso(o.value(QStringLiteral("lastWrongAt")).toString());
    e.lastReviewAt = util::fromIso(o.value(QStringLiteral("lastReviewAt")).toString());
    e.nextReviewAt = util::fromIso(o.value(QStringLiteral("nextReviewAt")).toString());
    e.priority = wrongPriorityFromKey(o.value(QStringLiteral("priority")).toString());
    e.status = wrongStatusFromKey(o.value(QStringLiteral("status")).toString());
    e.myAnswer = o.value(QStringLiteral("myAnswer")).toString();
    e.remark = o.value(QStringLiteral("remark")).toString();
    e.updatedAt = util::fromIso(o.value(QStringLiteral("updatedAt")).toString());
    return e;
}
} // namespace

Result<QString> SyncService::exportTo(const QString &path, qint64 userId) {
    const auto user = m_users.findById(userId);
    if (!user) return Result<QString>::err(QStringLiteral("用户不存在"));

    const qint64 wrongBookId = m_wrong.ensureBook(userId);
    const Setting setting = m_settings.load(userId);
    WordQuery wq;
    wq.limit = 100000;
    const auto entries = m_wrong.listEntries(wrongBookId, wq);

    QJsonObject root;
    root.insert(QStringLiteral("format"), QLatin1String(kFormat));
    root.insert(QStringLiteral("version"), kVersion);
    root.insert(QStringLiteral("exportedAt"), util::nowIso());
    root.insert(QStringLiteral("username"), user->username);
    root.insert(QStringLiteral("settings"), SettingService::toJson(setting));

    QJsonArray arr;
    for (const auto &e : entries) arr.append(wrongToJson(e));
    root.insert(QStringLiteral("wrongEntries"), arr);

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        SyncLog log;
        log.userId = userId;
        log.direction = SyncDirection::Export;
        log.filePath = path;
        log.status = SyncStatus::Failed;
        log.message = QStringLiteral("无法写入文件");
        m_logs.add(log);
        return Result<QString>::err(QStringLiteral("无法写入同步文件：%1").arg(path));
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();

    SyncLog log;
    log.userId = userId;
    log.direction = SyncDirection::Export;
    log.filePath = path;
    log.itemCount = entries.size();
    log.status = SyncStatus::Success;
    log.message = QStringLiteral("导出错题本 %1 条与个人设置").arg(entries.size());
    m_logs.add(log);

    Logger::info(QStringLiteral("Sync"), QStringLiteral("导出同步文件: %1 (%2 条)").arg(path).arg(entries.size()));
    return Result<QString>::ok(path);
}

Result<SyncService::ImportResult> SyncService::importFrom(const QString &path, qint64 userId,
                                                          ConflictPolicy policy) {
    ImportResult result;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        SyncLog log;
        log.userId = userId;
        log.direction = SyncDirection::Import;
        log.filePath = path;
        log.status = SyncStatus::Failed;
        log.message = QStringLiteral("无法读取文件");
        m_logs.add(log);
        return Result<ImportResult>::err(QStringLiteral("无法读取同步文件：%1").arg(path));
    }
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return Result<ImportResult>::err(QStringLiteral("同步文件格式错误"));

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormat))
        return Result<ImportResult>::err(QStringLiteral("不是本系统的同步文件"));

    const qint64 wrongBookId = m_wrong.ensureBook(userId);
    Setting localSetting = m_settings.load(userId);
    WordRepository words(Database::instance());

    // ---- 个人设置 ----
    if (root.contains(QStringLiteral("settings"))) {
        const QJsonObject so = root.value(QStringLiteral("settings")).toObject();
        Setting remote = SettingService::fromJson(userId, so, localSetting);
        bool apply = false;
        switch (policy) {
        case ConflictPolicy::PreferLocal: apply = false; break;
        case ConflictPolicy::PreferRemote: apply = true; break;
        case ConflictPolicy::PreferNewer:
        case ConflictPolicy::Merge:
        default: apply = !localSetting.updatedAt.isValid() || remote.updatedAt > localSetting.updatedAt; break;
        }
        if (apply) {
            remote.updatedAt = QDateTime::currentDateTime();
            m_settings.save(remote);
            ++result.updated;
        } else {
            ++result.conflicts;
            result.details << QStringLiteral("设置：保留本地版本");
        }
    }

    // ---- 错题本 ----
    const QJsonArray arr = root.value(QStringLiteral("wrongEntries")).toArray();
    for (const auto &v : arr) {
        const QJsonObject o = v.toObject();
        const QString spelling = o.value(QStringLiteral("spelling")).toString().trimmed();
        const QString book = o.value(QStringLiteral("book")).toString();
        if (spelling.isEmpty()) continue;

        // 查找本地单词（先按书别，再全局）
        std::optional<Word> local = words.findBySpelling(spelling, book);
        if (!local) local = words.findBySpelling(spelling, QString());
        if (!local) {
            ++result.conflicts;
            result.details << QStringLiteral("未找到单词：%1").arg(spelling);
            continue;
        }

        WrongEntry remote = wrongFromJson(o);
        remote.wrongBookId = wrongBookId;
        remote.wordId = local->id;
        remote.updatedAt = remote.updatedAt.isValid() ? remote.updatedAt : QDateTime::currentDateTime();

        const auto localEntry = m_wrong.findEntry(wrongBookId, local->id);
        if (!localEntry) {
            if (m_wrong.upsertEntry(remote) >= 0) ++result.added;
            else ++result.conflicts;
            continue;
        }

        // 冲突处理
        WrongEntry merged = *localEntry;
        bool changed = false;
        switch (policy) {
        case ConflictPolicy::PreferLocal:
            ++result.conflicts;
            result.details << QStringLiteral("%1：保留本地").arg(spelling);
            break;
        case ConflictPolicy::PreferRemote:
            remote.id = localEntry->id;
            merged = remote;
            changed = true;
            break;
        case ConflictPolicy::PreferNewer:
            if (remote.updatedAt > localEntry->updatedAt) {
                remote.id = localEntry->id;
                merged = remote;
                changed = true;
            } else {
                ++result.conflicts;
                result.details << QStringLiteral("%1：本地较新，保留本地").arg(spelling);
            }
            break;
        case ConflictPolicy::Merge:
        default:
            merged.wrongCount = std::max(localEntry->wrongCount, remote.wrongCount);
            merged.correctCount = std::max(localEntry->correctCount, remote.correctCount);
            merged.priority = static_cast<int>(localEntry->priority) >= static_cast<int>(remote.priority)
                                  ? localEntry->priority : remote.priority;
            merged.status = localEntry->updatedAt > remote.updatedAt ? localEntry->status : remote.status;
            merged.lastWrongAt = std::max(localEntry->lastWrongAt, remote.lastWrongAt);
            merged.lastReviewAt = std::max(localEntry->lastReviewAt, remote.lastReviewAt);
            merged.nextReviewAt = std::max(localEntry->nextReviewAt, remote.nextReviewAt);
            merged.myAnswer = remote.updatedAt > localEntry->updatedAt ? remote.myAnswer : localEntry->myAnswer;
            merged.remark = remote.updatedAt > localEntry->updatedAt ? remote.remark : localEntry->remark;
            changed = true;
            break;
        }
        if (changed) {
            if (m_wrong.updateEntry(merged)) ++result.updated;
            else ++result.conflicts;
        }
    }

    SyncLog log;
    log.userId = userId;
    log.direction = SyncDirection::Import;
    log.filePath = path;
    log.itemCount = result.added + result.updated;
    log.conflictCount = result.conflicts;
    log.status = result.conflicts > 0 ? SyncStatus::Partial : SyncStatus::Success;
    log.message = QStringLiteral("导入：新增 %1，更新 %2，冲突 %3")
                      .arg(result.added).arg(result.updated).arg(result.conflicts);
    m_logs.add(log);

    Logger::info(QStringLiteral("Sync"), log.message);
    return Result<ImportResult>::ok(result);
}

QList<SyncLog> SyncService::logs(qint64 userId, int limit) { return m_logs.listRecent(userId, limit); }

Status SyncService::clearLogs(qint64 userId) {
    return m_logs.clear(userId) ? Status::ok() : Status::err(QStringLiteral("清空日志失败"));
}

} // namespace vt
