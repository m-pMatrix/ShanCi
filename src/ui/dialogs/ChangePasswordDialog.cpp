#include "ui/dialogs/ChangePasswordDialog.h"

#include "core/AppContext.h"
#include "core/Config.h"
#include "ui/UiHelpers.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace vt {

ChangePasswordDialog::ChangePasswordDialog(AppContext &ctx, qint64 userId, const QString &username,
                                           QWidget *parent)
    : QDialog(parent), m_ctx(ctx), m_userId(userId) {
    setWindowTitle(QStringLiteral("重新设置密码"));
    setMinimumWidth(380);

    auto *root = new QVBoxLayout(this);
    auto *tip = new QLabel(QStringLiteral("账户：%1").arg(username), this);
    tip->setObjectName(QStringLiteral("SubHeader"));
    root->addWidget(tip);

    auto *form = new QFormLayout;
    m_oldEdit = new QLineEdit(this);
    m_oldEdit->setEchoMode(QLineEdit::Password);
    m_oldEdit->setPlaceholderText(QStringLiteral("当前密码"));
    m_newEdit = new QLineEdit(this);
    m_newEdit->setEchoMode(QLineEdit::Password);
    m_newEdit->setPlaceholderText(QStringLiteral("至少 4 个字符"));
    m_confirmEdit = new QLineEdit(this);
    m_confirmEdit->setEchoMode(QLineEdit::Password);
    m_confirmEdit->setPlaceholderText(QStringLiteral("再次输入新密码"));

    form->addRow(QStringLiteral("原密码"), m_oldEdit);
    form->addRow(QStringLiteral("新密码"), m_newEdit);
    form->addRow(QStringLiteral("确认新密码"), m_confirmEdit);
    root->addLayout(form);

    m_msg = new QLabel(this);
    m_msg->setStyleSheet(QStringLiteral("color:#D64545;"));
    m_msg->setWordWrap(true);
    m_msg->hide();
    root->addWidget(m_msg);

    auto *bar = new QHBoxLayout;
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    cancel->setProperty("secondary", true);
    auto *ok = new QPushButton(QStringLiteral("确定修改"), this);
    ok->setDefault(true);
    bar->addStretch();
    bar->addWidget(cancel);
    bar->addWidget(ok);
    root->addLayout(bar);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &ChangePasswordDialog::onOk);
    connect(m_confirmEdit, &QLineEdit::returnPressed, this, &ChangePasswordDialog::onOk);
}

void ChangePasswordDialog::onOk() {
    const QString oldPwd = m_oldEdit->text();
    const QString newPwd = m_newEdit->text();
    const QString confirm = m_confirmEdit->text();

    auto fail = [this](const QString &text) {
        m_msg->setText(text);
        m_msg->show();
    };

    if (oldPwd.isEmpty()) return fail(QStringLiteral("请输入原密码"));
    if (newPwd.size() < 4) return fail(QStringLiteral("新密码至少 4 个字符"));
    if (newPwd != confirm) return fail(QStringLiteral("两次输入的新密码不一致"));

    const auto st = m_ctx.auth().changePassword(m_userId, oldPwd, newPwd);
    if (!st.isOk()) return fail(st.error());

    // 密码已变更，“记住登录状态”令牌随之失效，清理掉避免下次自动登录失败
    Config &cfg = Config::instance();
    cfg.set(QStringLiteral("rememberUserId"), QString());
    cfg.set(QStringLiteral("rememberToken"), QString());
    cfg.save();

    ui::info(this, QStringLiteral("密码已修改成功。"));
    accept();
}

} // namespace vt
