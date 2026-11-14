#pragma once
//
// ChangePasswordDialog.h - 修改 / 重新设置密码
//
#include <QDialog>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QLabel;
QT_END_NAMESPACE

namespace vt {

class AppContext;

// 需要校验原密码后才能设置新密码（离线账户，不提供绕过原密码的“找回”）
class ChangePasswordDialog : public QDialog {
    Q_OBJECT
public:
    ChangePasswordDialog(AppContext &ctx, qint64 userId, const QString &username,
                         QWidget *parent = nullptr);

private slots:
    void onOk();

private:
    AppContext &m_ctx;
    qint64 m_userId;
    QLineEdit *m_oldEdit = nullptr;
    QLineEdit *m_newEdit = nullptr;
    QLineEdit *m_confirmEdit = nullptr;
    QLabel *m_msg = nullptr;
};

} // namespace vt
