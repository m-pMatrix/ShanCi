#pragma once
//
// LoginDialog.h - 登录界面（离线登录 / 新建账户 / 选择本地账户）
//   支持：记住上次登录账号、可选“记住登录状态（下次自动登录）”、重新设置密码
//
#include <QDialog>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QCheckBox;
QT_END_NAMESPACE

namespace vt {

class AppContext;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(AppContext &ctx, QWidget *parent = nullptr);

    qint64 loggedInUserId() const { return m_userId; }

private slots:
    void onAccountSelected(int index);
    void onLogin();
    void onCreateAccount();
    void onResetPassword();

private:
    void reloadAccounts();
    void setStatus(const QString &text, bool error);
    void rememberLogin(qint64 userId);

    AppContext &m_ctx;
    QComboBox *m_accountCombo = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QCheckBox *m_rememberCheck = nullptr;
    QLabel *m_statusLabel = nullptr;
    qint64 m_userId = 0;
};

} // namespace vt
