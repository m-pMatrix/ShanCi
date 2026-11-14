#pragma once
//
// SettingsPage.h - 个人设置页
//
#include "ui/Page.h"
#include "domain/Entities.h"

QT_BEGIN_NAMESPACE
class QSpinBox;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QListWidget;
class QLabel;
QT_END_NAMESPACE

namespace vt {

class SettingsPage : public Page {
    Q_OBJECT
public:
    SettingsPage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("settings"); }
    void refresh() override;

private slots:
    void onSave();
    void onReset();
    void onBrowseSyncPath();
    void onChangePassword();

private:
    void loadIntoWidgets(const Setting &s);

    QLabel *m_accountLabel = nullptr;
    QSpinBox *m_dailyCount = nullptr;
    QSpinBox *m_fontSize = nullptr;
    QComboBox *m_theme = nullptr;
    QLineEdit *m_syncPath = nullptr;
    QCheckBox *m_audio = nullptr;
    QCheckBox *m_showPhonetic = nullptr;
    QCheckBox *m_autoWrong = nullptr;
    QCheckBox *m_strictSpelling = nullptr;
    QCheckBox *m_typeEnToCn = nullptr;
    QCheckBox *m_typeCnToEn = nullptr;
    QCheckBox *m_typeSimilar = nullptr;
    QCheckBox *m_typeCloze = nullptr;
};

} // namespace vt
