#include "ui/MainWindow.h"

#include "core/AppContext.h"
#include "core/Config.h"
#include "ui/Page.h"
#include "ui/UiHelpers.h"
#include "ui/pages/DailyListPage.h"
#include "ui/pages/DictionaryPage.h"
#include "ui/pages/HomePage.h"
#include "ui/pages/PracticePage.h"
#include "ui/pages/QuizPage.h"
#include "ui/pages/ResultPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/SyncPage.h"
#include "ui/pages/WordBookPage.h"
#include "ui/pages/WrongBookPage.h"

#include <QButtonGroup>
#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace vt {

namespace {
struct NavItem {
    QString id;
    QString text;
};
} // namespace

MainWindow::MainWindow(AppContext &ctx, QWidget *parent) : QMainWindow(parent), m_ctx(ctx) {
    setWindowTitle(QStringLiteral("闪词 · 四六级单词辅助背诵"));
    setMinimumSize(760, 520);
    // 适配屏幕：避免在小屏/高 DPI 下窗口被系统压缩导致控件被裁切
    if (QScreen *screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        resize(qMax(760, qMin(1100, avail.width() - 40)),
               qMax(520, qMin(740, avail.height() - 40)));
    } else {
        resize(1100, 740);
    }

    auto *central = new QWidget(this);
    auto *rootLay = new QHBoxLayout(central);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    buildSidebar(rootLay);

    m_stack = new QStackedWidget(central);
    m_stack->setObjectName(QStringLiteral("Content"));
    rootLay->addWidget(m_stack, 1);
    setCentralWidget(central);

    // 创建页面
    m_pages.insert(QStringLiteral("home"), new HomePage(m_ctx, this, this));
    m_pages.insert(QStringLiteral("wordbook"), new WordBookPage(m_ctx, this, this));
    m_pages.insert(QStringLiteral("wrongbook"), new WrongBookPage(m_ctx, this, this));
    m_pages.insert(QStringLiteral("dailylist"), new DailyListPage(m_ctx, this, this));
    m_pages.insert(QStringLiteral("practice"), new PracticePage(m_ctx, this, this));
    m_pages.insert(QStringLiteral("dictionary"), new DictionaryPage(m_ctx, this, this));
    m_pages.insert(QStringLiteral("settings"), new SettingsPage(m_ctx, this, this));
    m_pages.insert(QStringLiteral("sync"), new SyncPage(m_ctx, this, this));

    m_quiz = new QuizPage(m_ctx, this, this);
    m_result = new ResultPage(m_ctx, this, this);
    m_pages.insert(QStringLiteral("quiz"), m_quiz);
    m_pages.insert(QStringLiteral("result"), m_result);

    for (auto *p : m_pages) m_stack->addWidget(p);

    connect(&m_ctx, &AppContext::dataChanged, this, [this] {
        if (auto *p = page(m_stack->currentWidget()->property("pageId").toString())) p->refresh();
    });

    navigateTo(QStringLiteral("home"));
}

MainWindow::~MainWindow() = default;

void MainWindow::buildSidebar(QHBoxLayout *rootLay) {
    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(210);
    auto *lay = new QVBoxLayout(sidebar);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    auto *logo = new QLabel(QStringLiteral("⚡ 闪词"), sidebar);
    logo->setObjectName(QStringLiteral("SidebarLogo"));
    lay->addWidget(logo);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    const QList<NavItem> items = {
        {QStringLiteral("home"), QStringLiteral("🏠  概览")},
        {QStringLiteral("wordbook"), QStringLiteral("📖  单词本")},
        {QStringLiteral("wrongbook"), QStringLiteral("❌  错题本")},
        {QStringLiteral("dailylist"), QStringLiteral("📅  每日词表")},
        {QStringLiteral("practice"), QStringLiteral("✏️  练习")},
        {QStringLiteral("dictionary"), QStringLiteral("🔍  查询字典")},
        {QStringLiteral("settings"), QStringLiteral("⚙️  个人设置")},
        {QStringLiteral("sync"), QStringLiteral("🔄  手动同步")},
    };
    for (const auto &it : items) {
        auto *btn = new QPushButton(it.text, sidebar);
        btn->setObjectName(QStringLiteral("NavButton"));
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("pageId", it.id);
        connect(btn, &QPushButton::clicked, this, [this, it] { navigateTo(it.id); });
        lay->addWidget(btn);
        m_navGroup->addButton(btn);
        m_navButtons.insert(it.id, btn);
    }

    lay->addStretch();

    m_userLabel = new QLabel(sidebar);
    m_userLabel->setObjectName(QStringLiteral("SubHeader"));
    m_userLabel->setContentsMargins(16, 0, 16, 0);
    lay->addWidget(m_userLabel);

    auto *logout = new QPushButton(QStringLiteral("退出登录"), sidebar);
    logout->setProperty("secondary", true);
    logout->setContentsMargins(16, 0, 16, 0);
    connect(logout, &QPushButton::clicked, this, &MainWindow::onLogout);
    auto *logoutWrap = new QWidget(sidebar);
    auto *lw = new QVBoxLayout(logoutWrap);
    lw->setContentsMargins(12, 6, 12, 14);
    lw->addWidget(logout);
    lay->addWidget(logoutWrap);

    rootLay->addWidget(sidebar);

    // 用户信息
    const qint64 uid = m_ctx.currentUserId();
    const auto u = m_ctx.users().findById(uid);
    m_userLabel->setText(u ? QStringLiteral("当前账户：%1").arg(u->displayName.isEmpty() ? u->username : u->displayName)
                           : QStringLiteral("未登录"));
}

Page *MainWindow::page(const QString &id) const { return m_pages.value(id, nullptr); }

void MainWindow::navigateTo(const QString &pageId) {
    auto *p = page(pageId);
    if (!p) return;
    m_stack->setCurrentWidget(p);
    p->setProperty("pageId", pageId);
    if (auto *btn = m_navButtons.value(pageId, nullptr)) btn->setChecked(true);
    p->refresh();
}

void MainWindow::startQuiz(qint64 paperId) {
    if (m_quiz) {
        m_quiz->beginQuiz(paperId);
        m_stack->setCurrentWidget(m_quiz);
        m_quiz->setProperty("pageId", QStringLiteral("quiz"));
        if (auto *btn = m_navButtons.value(QStringLiteral("dailylist"), nullptr)) btn->setChecked(false);
    }
}

void MainWindow::showResult(qint64 paperId) {
    if (m_result) {
        m_result->showResult(paperId);
        m_stack->setCurrentWidget(m_result);
        m_result->setProperty("pageId", QStringLiteral("result"));
    }
}

void MainWindow::onLogout() {
    if (!ui::confirm(this, QStringLiteral("确定退出登录？"))) return;

    // 主动登出时清除“保持登录状态”，避免下次直接进入
    Config &cfg = Config::instance();
    cfg.set(QStringLiteral("rememberUserId"), QString());
    cfg.set(QStringLiteral("rememberToken"), QString());
    cfg.set(QStringLiteral("rememberEnabled"), false);
    cfg.save();

    m_ctx.clearCurrentUser();
    emit loggedOut();
    close();
}

} // namespace vt
