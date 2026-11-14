#include "ui/pages/HomePage.h"

#include "core/AppContext.h"
#include "core/Utils.h"
#include "ui/Navigation.h"
#include "ui/UiHelpers.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace vt {

namespace {
QFrame *makeCard(const QString &title, QLabel **valueLabel, QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("StatCard"));
    card->setMinimumHeight(110);
    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(18, 16, 18, 16);
    lay->setSpacing(8);
    auto *t = new QLabel(title, card);
    t->setStyleSheet(QStringLiteral("color:#6C7484;font-size:13px;"));
    auto *v = new QLabel(QStringLiteral("0"), card);
    v->setStyleSheet(QStringLiteral("font-size:28px;font-weight:700;color:#3B6DF6;"));
    v->setMinimumHeight(40);
    lay->addWidget(t);
    lay->addWidget(v);
    *valueLabel = v;
    return card;
}
} // namespace

HomePage::HomePage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    // 内容放入滚动区域，避免窗口较矮时控件重叠
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);
    scroll->setWidget(content);

    auto *title = new QLabel(QStringLiteral("概览"), content);
    title->setObjectName(QStringLiteral("Header"));
    root->addWidget(title);

    auto *cards = new QGridLayout;
    cards->setSpacing(14);
    cards->addWidget(makeCard(QStringLiteral("词库单词"), &m_wordsLabel, content), 0, 0);
    cards->addWidget(makeCard(QStringLiteral("单词本"), &m_bookLabel, content), 0, 1);
    cards->addWidget(makeCard(QStringLiteral("错题本"), &m_wrongLabel, content), 1, 0);
    cards->addWidget(makeCard(QStringLiteral("今日词表"), &m_dailyLabel, content), 1, 1);
    root->addLayout(cards);

    auto *actions = new QLabel(QStringLiteral("快捷操作"), content);
    actions->setObjectName(QStringLiteral("Header"));
    root->addWidget(actions);

    auto *grid = new QGridLayout;
    grid->setSpacing(12);
    struct Item {
        QString text;
        QString page;
    };
    const QList<Item> items = {
        {QStringLiteral("每日词表"), QStringLiteral("dailylist")},
        {QStringLiteral("练习"), QStringLiteral("practice")},
        {QStringLiteral("错题本"), QStringLiteral("wrongbook")},
        {QStringLiteral("单词本"), QStringLiteral("wordbook")},
        {QStringLiteral("查询字典"), QStringLiteral("dictionary")},
        {QStringLiteral("个人设置"), QStringLiteral("settings")},
        {QStringLiteral("手动同步"), QStringLiteral("sync")},
    };
    int r = 0, c = 0;
    for (const auto &it : items) {
        auto *b = new QPushButton(it.text, content);
        b->setFixedHeight(50);
        b->setProperty("secondary", true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, it] { this->nav()->navigateTo(it.page); });
        grid->addWidget(b, r, c);
        if (++c == 2) { c = 0; ++r; }
    }
    root->addLayout(grid);

    auto *tip = new QLabel(content);
    tip->setObjectName(QStringLiteral("SubHeader"));
    tip->setWordWrap(true);
    tip->setText(QStringLiteral(
        "提示：本系统为离线单用户软件，数据保存在本地 SQLite 数据库；错题本与个人设置可通过「手动同步」导出/导入。\n"
        "用法：在「查询字典」中查询生词 → 加入你自建的单词本（可建多个）；在「每日词表」中设置背诵天数与每日词量，按计划背单词。"));
    root->addWidget(tip);
    root->addStretch();

    connect(&ctx, &AppContext::dataChanged, this, &HomePage::refresh);
    refresh();
}

void HomePage::refresh() {
    const qint64 uid = ctx().currentUserId();
    const qint64 total = ctx().wordService().count();
    m_wordsLabel->setText(QString::number(total));
    m_bookLabel->setText(uid > 0 ? QString::number(ctx().wordBookService().totalEntries(uid)) : QStringLiteral("0"));
    m_wrongLabel->setText(uid > 0 ? QString::number(ctx().wrongBookService().count(uid)) : QStringLiteral("0"));
    if (uid > 0) {
        auto today = ctx().dailyListService().today(uid);
        m_dailyLabel->setText(today ? QStringLiteral("%1/%2").arg(today.value().doneCount).arg(today.value().totalCount)
                                    : QStringLiteral("未生成"));
    } else {
        m_dailyLabel->setText(QStringLiteral("未登录"));
    }
}

} // namespace vt
