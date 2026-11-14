#include "ui/pages/SettingsPage.h"

#include "core/AppContext.h"
#include "core/Paths.h"
#include "ui/Navigation.h"
#include "ui/Theme.h"
#include "ui/UiHelpers.h"
#include "ui/dialogs/ChangePasswordDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace vt {

SettingsPage::SettingsPage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("个人设置"), this);
    title->setObjectName(QStringLiteral("Header"));
    root->addWidget(title);

    // ---- 账户 ----
    auto *accountBox = new QGroupBox(QStringLiteral("账户"), this);
    auto *accLay = new QHBoxLayout(accountBox);
    m_accountLabel = new QLabel(this);
    auto *pwdBtn = new QPushButton(QStringLiteral("修改密码"), accountBox);
    pwdBtn->setProperty("secondary", true);
    accLay->addWidget(m_accountLabel);
    accLay->addStretch();
    accLay->addWidget(pwdBtn);
    root->addWidget(accountBox);

    auto *general = new QGroupBox(QStringLiteral("学习与显示"), this);
    auto *form = new QFormLayout(general);
    m_dailyCount = new QSpinBox(general);
    m_dailyCount->setRange(1, 200);
    m_fontSize = new QSpinBox(general);
    m_fontSize->setRange(9, 22);
    m_theme = new QComboBox(general);
    m_theme->addItem(QStringLiteral("浅色"), QStringLiteral("light"));
    m_theme->addItem(QStringLiteral("深色"), QStringLiteral("dark"));
    m_syncPath = new QLineEdit(general);
    m_syncPath->setPlaceholderText(QStringLiteral("默认同步目录"));
    auto *browseBtn = new QPushButton(QStringLiteral("浏览…"), general);
    browseBtn->setProperty("secondary", true);
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_syncPath, 1);
    pathRow->addWidget(browseBtn);

    m_audio = new QCheckBox(QStringLiteral("启用发音播放"), general);
    m_showPhonetic = new QCheckBox(QStringLiteral("显示音标"), general);
    m_autoWrong = new QCheckBox(QStringLiteral("答错自动加入错题本"), general);
    m_strictSpelling = new QCheckBox(QStringLiteral("拼写严格校验（不允许容错）"), general);

    form->addRow(QStringLiteral("每日词数"), m_dailyCount);
    form->addRow(QStringLiteral("字号"), m_fontSize);
    form->addRow(QStringLiteral("主题"), m_theme);
    form->addRow(QStringLiteral("同步目录"), pathRow);
    form->addRow(QString(), m_audio);
    form->addRow(QString(), m_showPhonetic);
    form->addRow(QString(), m_autoWrong);
    form->addRow(QString(), m_strictSpelling);
    root->addWidget(general);

    auto *types = new QGroupBox(QStringLiteral("题型偏好"), this);
    auto *typeLay = new QVBoxLayout(types);
    m_typeEnToCn = new QCheckBox(QStringLiteral("英文单词选中文释义"), types);
    m_typeCnToEn = new QCheckBox(QStringLiteral("中文释义拼写英文单词"), types);
    m_typeSimilar = new QCheckBox(QStringLiteral("相似词匹配词义"), types);
    m_typeCloze = new QCheckBox(QStringLiteral("选词填空"), types);
    typeLay->addWidget(m_typeEnToCn);
    typeLay->addWidget(m_typeCnToEn);
    typeLay->addWidget(m_typeSimilar);
    typeLay->addWidget(m_typeCloze);
    root->addWidget(types);

    auto *actions = new QHBoxLayout;
    auto *resetBtn = new QPushButton(QStringLiteral("恢复默认"), this);
    resetBtn->setProperty("secondary", true);
    auto *saveBtn = new QPushButton(QStringLiteral("保存设置"), this);
    actions->addStretch();
    actions->addWidget(resetBtn);
    actions->addWidget(saveBtn);
    root->addLayout(actions);
    root->addStretch();

    connect(browseBtn, &QPushButton::clicked, this, &SettingsPage::onBrowseSyncPath);
    connect(pwdBtn, &QPushButton::clicked, this, &SettingsPage::onChangePassword);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsPage::onSave);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsPage::onReset);

    refresh();
}

void SettingsPage::refresh() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    const auto u = ctx().users().findById(uid);
    if (u && m_accountLabel) {
        m_accountLabel->setText(QStringLiteral("当前账户：%1（%2）")
                                    .arg(u->displayName.isEmpty() ? u->username : u->displayName,
                                         u->username));
    }
    loadIntoWidgets(ctx().settingService().get(uid));
}

void SettingsPage::onChangePassword() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    const auto u = ctx().users().findById(uid);
    if (!u) return;
    ChangePasswordDialog dlg(ctx(), u->id, u->username, this);
    dlg.exec();
}

void SettingsPage::loadIntoWidgets(const Setting &s) {
    m_dailyCount->setValue(s.dailyWordCount);
    m_fontSize->setValue(s.fontSize);
    const int ti = m_theme->findData(s.theme);
    m_theme->setCurrentIndex(ti >= 0 ? ti : 0);
    m_syncPath->setText(s.syncPath);
    m_audio->setChecked(s.audioEnabled);
    m_showPhonetic->setChecked(s.showPhonetic);
    m_autoWrong->setChecked(s.autoAddWrong);
    m_strictSpelling->setChecked(s.checkSpellingStrict);
    const auto types = s.questionTypes;
    m_typeEnToCn->setChecked(types.contains(QuestionType::EnToCn));
    m_typeCnToEn->setChecked(types.contains(QuestionType::CnToEn));
    m_typeSimilar->setChecked(types.contains(QuestionType::SimilarMatch));
    m_typeCloze->setChecked(types.contains(QuestionType::Cloze));
}

void SettingsPage::onBrowseSyncPath() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择同步目录"),
                                                          m_syncPath->text().isEmpty() ? Paths::syncDir()
                                                                                       : m_syncPath->text());
    if (!dir.isEmpty()) m_syncPath->setText(dir);
}

void SettingsPage::onSave() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;

    Setting s = ctx().settingService().get(uid);
    s.dailyWordCount = m_dailyCount->value();
    s.fontSize = m_fontSize->value();
    s.theme = m_theme->currentData().toString();
    s.syncPath = m_syncPath->text().trimmed();
    s.audioEnabled = m_audio->isChecked();
    s.showPhonetic = m_showPhonetic->isChecked();
    s.autoAddWrong = m_autoWrong->isChecked();
    s.checkSpellingStrict = m_strictSpelling->isChecked();

    QList<QuestionType> types;
    if (m_typeEnToCn->isChecked()) types << QuestionType::EnToCn;
    if (m_typeCnToEn->isChecked()) types << QuestionType::CnToEn;
    if (m_typeSimilar->isChecked()) types << QuestionType::SimilarMatch;
    if (m_typeCloze->isChecked()) types << QuestionType::Cloze;
    if (types.isEmpty()) { ui::warn(this, QStringLiteral("请至少选择一种题型")); return; }
    s.questionTypes = types;

    const auto st = ctx().settingService().update(s);
    if (!st.isOk()) { ui::error(this, st.error()); return; }

    ctx().audio().setEnabled(s.audioEnabled);
    if (auto *app = qobject_cast<QApplication *>(QApplication::instance()))
        theme::apply(*app, s.theme);

    ui::info(this, QStringLiteral("设置已保存"));
    emit ctx().dataChanged();
}

void SettingsPage::onReset() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    if (!ui::confirm(this, QStringLiteral("确定恢复默认设置？"))) return;
    ctx().settingService().reset(uid);
    refresh();
    ui::info(this, QStringLiteral("已恢复默认设置"));
    emit ctx().dataChanged();
}

} // namespace vt
