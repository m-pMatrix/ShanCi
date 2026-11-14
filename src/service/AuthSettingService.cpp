#include "core/Logger.h"
#include "core/Utils.h"
#include "domain/Rules.h"
#include "service/Services.h"

namespace vt {

// ---------------------------------------------------------------------------
// AuthService
// ---------------------------------------------------------------------------
Status AuthService::createAccount(const QString &username, const QString &password,
                                  const QString &displayName, qint64 *userIdOut) {
    QString reason;
    if (!rules::isUsernameValid(username, &reason)) return Status::err(reason);
    if (!rules::isPasswordValid(password, &reason)) return Status::err(reason);
    if (m_users.exists(username)) return Status::err(QStringLiteral("该用户名已存在"));

    User u;
    u.username = username.trimmed();
    u.salt = util::generateSalt();
    u.passwordHash = rules::hashPassword(password, u.salt);
    u.displayName = displayName.trimmed().isEmpty() ? u.username : displayName.trimmed();
    const qint64 id = m_users.create(u);
    if (id < 0) return Status::err(QStringLiteral("账户创建失败"));

    ensureUserScaffold(id);
    if (userIdOut) *userIdOut = id;
    Logger::info(QStringLiteral("Auth"), QStringLiteral("创建账户: %1").arg(u.username));
    return Status::ok();
}

Result<User> AuthService::login(const QString &username, const QString &password) {
    const auto found = m_users.findByUsername(username);
    if (!found) return Result<User>::err(QStringLiteral("账户不存在"));
    const User &u = *found;
    if (rules::hashPassword(password, u.salt) != u.passwordHash)
        return Result<User>::err(QStringLiteral("密码错误"));
    m_users.updateLastLogin(u.id);
    ensureUserScaffold(u.id);
    Logger::info(QStringLiteral("Auth"), QStringLiteral("登录成功: %1").arg(u.username));
    return Result<User>::ok(u);
}

QList<User> AuthService::listAccounts() { return m_users.listAll(); }

bool AuthService::accountExists(const QString &username) { return m_users.exists(username); }

Status AuthService::changePassword(qint64 userId, const QString &oldPassword, const QString &newPassword) {
    const auto found = m_users.findById(userId);
    if (!found) return Status::err(QStringLiteral("账户不存在"));
    if (rules::hashPassword(oldPassword, found->salt) != found->passwordHash)
        return Status::err(QStringLiteral("原密码不正确"));

    QString reason;
    if (!rules::isPasswordValid(newPassword, &reason)) return Status::err(reason);
    if (oldPassword == newPassword) return Status::err(QStringLiteral("新密码不能与原密码相同"));

    User updated = *found;
    updated.salt = util::generateSalt();
    updated.passwordHash = rules::hashPassword(newPassword, updated.salt);
    if (!m_users.updatePassword(updated.id, updated.passwordHash, updated.salt))
        return Status::err(QStringLiteral("密码保存失败"));

    Logger::info(QStringLiteral("Auth"), QStringLiteral("账户 %1 修改密码成功").arg(updated.username));
    return Status::ok();
}

QString AuthService::rememberToken(const User &u) {
    // 派生令牌：不包含明文密码；修改密码后令牌自动失效
    return util::sha256Hex(QStringLiteral("remember|%1|%2|%3|%4|vocab-tutor")
                               .arg(u.id)
                               .arg(u.username)
                               .arg(u.passwordHash)
                               .arg(u.salt));
}

Result<User> AuthService::loginWithRememberToken(qint64 userId, const QString &token) {
    const auto found = m_users.findById(userId);
    if (!found) return Result<User>::err(QStringLiteral("账户不存在"));
    if (token.isEmpty() || rememberToken(*found) != token)
        return Result<User>::err(QStringLiteral("登录凭据已失效，请重新登录"));
    m_users.updateLastLogin(found->id);
    ensureUserScaffold(found->id);
    Logger::info(QStringLiteral("Auth"), QStringLiteral("自动登录: %1").arg(found->username));
    return Result<User>::ok(*found);
}

void AuthService::ensureUserScaffold(qint64 userId) {
    if (!m_settings.exists(userId)) {
        Setting s;
        s.userId = userId;
        s.updatedAt = QDateTime::currentDateTime();
        m_settings.save(s);
    }
    if (!m_books.findDefaultBook(userId)) {
        WordBook b;
        b.userId = userId;
        b.name = QStringLiteral("我的单词本");
        b.description = QStringLiteral("默认单词本");
        m_books.createBook(b);
    }
    m_wrong.ensureBook(userId);
}

qint64 AuthService::defaultBookId(qint64 userId) {
    auto b = m_books.findDefaultBook(userId);
    if (b) return b->id;
    WordBook nb;
    nb.userId = userId;
    nb.name = QStringLiteral("我的单词本");
    return m_books.createBook(nb);
}

// ---------------------------------------------------------------------------
// SettingService
// ---------------------------------------------------------------------------
Setting SettingService::get(qint64 userId) { return m_repo.load(userId); }

Status SettingService::update(const Setting &s) {
    Setting copy = s;
    copy.updatedAt = QDateTime::currentDateTime();
    return m_repo.save(copy) ? Status::ok() : Status::err(QStringLiteral("设置保存失败"));
}

Status SettingService::reset(qint64 userId) {
    Setting s;
    s.userId = userId;
    s.updatedAt = QDateTime::currentDateTime();
    return m_repo.save(s) ? Status::ok() : Status::err(QStringLiteral("设置重置失败"));
}

QJsonObject SettingService::toJson(const Setting &s) {
    QJsonObject o;
    o.insert(QStringLiteral("dailyWordCount"), s.dailyWordCount);
    o.insert(QStringLiteral("questionTypes"), util::joinCsv(questionTypesToKeys(s.questionTypes)));
    o.insert(QStringLiteral("theme"), s.theme);
    o.insert(QStringLiteral("fontSize"), s.fontSize);
    o.insert(QStringLiteral("audioEnabled"), s.audioEnabled);
    o.insert(QStringLiteral("showPhonetic"), s.showPhonetic);
    o.insert(QStringLiteral("autoAddWrong"), s.autoAddWrong);
    o.insert(QStringLiteral("strictSpelling"), s.checkSpellingStrict);
    o.insert(QStringLiteral("updatedAt"), util::toIso(s.updatedAt));
    return o;
}

Setting SettingService::fromJson(qint64 userId, const QJsonObject &o, const Setting &fallback) {
    Setting s = fallback;
    s.userId = userId;
    if (o.contains(QStringLiteral("dailyWordCount")))
        s.dailyWordCount = o.value(QStringLiteral("dailyWordCount")).toInt(s.dailyWordCount);
    if (o.contains(QStringLiteral("questionTypes"))) {
        const auto list = questionTypesFromKeys(util::splitCsv(o.value(QStringLiteral("questionTypes")).toString()));
        if (!list.isEmpty()) s.questionTypes = list;
    }
    if (o.contains(QStringLiteral("theme"))) s.theme = o.value(QStringLiteral("theme")).toString(s.theme);
    if (o.contains(QStringLiteral("fontSize"))) s.fontSize = o.value(QStringLiteral("fontSize")).toInt(s.fontSize);
    if (o.contains(QStringLiteral("audioEnabled"))) s.audioEnabled = o.value(QStringLiteral("audioEnabled")).toBool();
    if (o.contains(QStringLiteral("showPhonetic"))) s.showPhonetic = o.value(QStringLiteral("showPhonetic")).toBool();
    if (o.contains(QStringLiteral("autoAddWrong"))) s.autoAddWrong = o.value(QStringLiteral("autoAddWrong")).toBool();
    if (o.contains(QStringLiteral("strictSpelling")))
        s.checkSpellingStrict = o.value(QStringLiteral("strictSpelling")).toBool();
    s.updatedAt = util::fromIso(o.value(QStringLiteral("updatedAt")).toString());
    return s;
}

} // namespace vt
