#include "ui/LoginDialog.h"

#include "core/AppContext.h"
#include "core/Config.h"
#include "ui/UiHelpers.h"
#include "ui/dialogs/ChangePasswordDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace vt {

namespace {
const QString kLastUsername = QStringLiteral("lastUsername");
const QString kRememberEnabled = QStringLiteral("rememberEnabled");
const QString kRememberUserId = QStringLiteral("rememberUserId");
const QString kRememberToken = QStringLiteral("rememberToken");
} // namespace

LoginDialog::LoginDialog(AppContext &ctx, QWidget *parent) : QDialog(parent), m_ctx(ctx) {
    setWindowTitle(QStringLiteral("闪词 - 登录"));
    setMinimumWidth(440);
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 28, 32, 24);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("闪词"), this);
    title->setObjectName(QStringLiteral("BrandTitle"));
    title->setAlignment(Qt::AlignCenter);
    auto *sub = new QLabel(QStringLiteral("四六级单词辅助背诵 · 离线账户 · 本地数据"), this);
    sub->setObjectName(QStringLiteral("SubHeader"));
    sub->setAlignment(Qt::AlignCenter);
    root->addWidget(title);
    root->addWidget(sub);
    root->addSpacing(6);

    auto *form = new QFormLayout;
    form->setSpacing(10);

    m_accountCombo = new QComboBox(this);
    m_accountCombo->setPlaceholderText(QStringLiteral("选择本地账户…"));
    form->addRow(QStringLiteral("本地账户"), m_accountCombo);

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(QStringLiteral("用户名"));
    form->addRow(QStringLiteral("用户名"), m_usernameEdit);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("密码"));
    form->addRow(QStringLiteral("密码"), m_passwordEdit);

    root->addLayout(form);

    m_rememberCheck = new QCheckBox(QStringLiteral("记住登录状态（下次启动自动登录，无需输入密码）"), this);
    m_rememberCheck->setChecked(
        Config::instance().getBool(kRememberEnabled, false));
    root->addWidget(m_rememberCheck);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#D64545;"));
    m_statusLabel->hide();
    root->addWidget(m_statusLabel);

    auto *buttons = new QHBoxLayout;
    auto *resetBtn = new QPushButton(QStringLiteral("重新设置密码"), this);
    resetBtn->setProperty("flat", true);
    auto *createBtn = new QPushButton(QStringLiteral("新建账户"), this);
    createBtn->setProperty("flat", true);
    auto *loginBtn = new QPushButton(QStringLiteral("登录"), this);
    loginBtn->setDefault(true);
    buttons->addWidget(resetBtn);
    buttons->addWidget(createBtn);
    buttons->addStretch();
    buttons->addWidget(loginBtn);
    root->addLayout(buttons);

    connect(m_accountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &LoginDialog::onAccountSelected);
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);
    connect(createBtn, &QPushButton::clicked, this, &LoginDialog::onCreateAccount);
    connect(resetBtn, &QPushButton::clicked, this, &LoginDialog::onResetPassword);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);

    reloadAccounts();

    // 记录上次登录账号：自动填入并选中
    const QString last = Config::instance().getString(kLastUsername);
    if (!last.isEmpty()) {
        m_usernameEdit->setText(last);
        for (int i = 0; i < m_accountCombo->count(); ++i) {
            const QVariant data = m_accountCombo->itemData(i);
            if (!data.isValid()) continue;
            const auto u = m_ctx.users().findById(data.toLongLong());
            if (u && u->username.compare(last, Qt::CaseInsensitive) == 0) {
                m_accountCombo->setCurrentIndex(i);
                break;
            }
        }
        m_passwordEdit->setFocus();
    }
}

void LoginDialog::reloadAccounts() {
    m_accountCombo->clear();
    m_accountCombo->addItem(QStringLiteral("（新账户 / 手动输入）"), QVariant());
    const auto accounts = m_ctx.auth().listAccounts();
    for (const auto &u : accounts)
        m_accountCombo->addItem(u.displayName.isEmpty() ? u.username : u.displayName, u.id);
}

void LoginDialog::onAccountSelected(int index) {
    const QVariant data = m_accountCombo->itemData(index);
    if (!data.isValid()) {
        m_usernameEdit->clear();
        return;
    }
    const qint64 id = data.toLongLong();
    const auto u = m_ctx.users().findById(id);
    if (u) m_usernameEdit->setText(u->username);
    m_passwordEdit->setFocus();
}

void LoginDialog::setStatus(const QString &text, bool error) {
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(error ? QStringLiteral("color:#D64545;") : QStringLiteral("color:#2E9E5B;"));
    m_statusLabel->setVisible(!text.isEmpty());
}

// 登录成功后：记录上次账号；按需保存“保持登录”令牌
void LoginDialog::rememberLogin(qint64 userId) {
    Config &cfg = Config::instance();
    const auto u = m_ctx.users().findById(userId);
    if (u) cfg.set(kLastUsername, u->username);
    cfg.set(kRememberEnabled, m_rememberCheck->isChecked());

    if (m_rememberCheck->isChecked() && u) {
        cfg.set(kRememberUserId, QString::number(userId));
        cfg.set(kRememberToken, AuthService::rememberToken(*u));
    } else {
        cfg.set(kRememberUserId, QString());
        cfg.set(kRememberToken, QString());
    }
    cfg.save();
}

void LoginDialog::onLogin() {
    const QString username = m_usernameEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        setStatus(QStringLiteral("请输入用户名和密码"), true);
        return;
    }
    auto result = m_ctx.auth().login(username, password);
    if (!result) {
        setStatus(result.error(), true);
        return;
    }
    m_userId = result.value().id;
    rememberLogin(m_userId);
    accept();
}

void LoginDialog::onCreateAccount() {
    const QString username = m_usernameEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        setStatus(QStringLiteral("请先填写用户名和密码，再点击“新建账户”"), true);
        return;
    }
    qint64 newId = 0;
    const auto st = m_ctx.auth().createAccount(username, password, username, &newId);
    if (!st.isOk()) {
        setStatus(st.error(), true);
        return;
    }
    m_userId = newId;
    rememberLogin(m_userId);
    setStatus(QStringLiteral("账户创建成功，正在登录…"), false);
    accept();
}

void LoginDialog::onResetPassword() {
    const QString username = m_usernameEdit->text().trimmed();
    if (username.isEmpty()) {
        setStatus(QStringLiteral("请先选择或输入要重置密码的账户"), true);
        return;
    }
    const auto u = m_ctx.users().findByUsername(username);
    if (!u) {
        setStatus(QStringLiteral("账户不存在：%1").arg(username), true);
        return;
    }
    ChangePasswordDialog dlg(m_ctx, u->id, u->username, this);
    if (dlg.exec() == QDialog::Accepted) {
        setStatus(QStringLiteral("密码已重新设置，请使用新密码登录"), false);
        m_passwordEdit->clear();
        m_passwordEdit->setFocus();
    }
}

} // namespace vt
