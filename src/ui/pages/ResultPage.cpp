#include "ui/pages/ResultPage.h"

#include "core/AppContext.h"
#include "ui/Navigation.h"
#include "ui/UiHelpers.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace vt {

ResultPage::ResultPage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 20, 28, 20);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("练习结果"), this);
    title->setObjectName(QStringLiteral("Header"));
    root->addWidget(title);

    m_score = new QLabel(QStringLiteral("--"), this);
    m_score->setStyleSheet(QStringLiteral("font-size:48px;font-weight:700;color:#2F6FED;"));
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("SubHeader"));
    root->addWidget(m_score);
    root->addWidget(m_summary);

    m_table = ui::makeTable({QStringLiteral("#"), QStringLiteral("题目"), QStringLiteral("你的答案"),
                             QStringLiteral("正确答案"), QStringLiteral("结果")},
                            this);
    root->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *addWrongBtn = new QPushButton(QStringLiteral("错题加入错题本"), this);
    addWrongBtn->setProperty("secondary", true);
    auto *retakeBtn = new QPushButton(QStringLiteral("重做"), this);
    auto *homeBtn = new QPushButton(QStringLiteral("返回主界面"), this);
    actions->addWidget(addWrongBtn);
    actions->addStretch();
    actions->addWidget(retakeBtn);
    actions->addWidget(homeBtn);
    root->addLayout(actions);

    connect(addWrongBtn, &QPushButton::clicked, this, &ResultPage::onAddWrong);
    connect(retakeBtn, &QPushButton::clicked, this, &ResultPage::onRetake);
    connect(homeBtn, &QPushButton::clicked, this, &ResultPage::onHome);
}

void ResultPage::showResult(qint64 paperId) {
    m_paperId = paperId;
    refresh();
}

void ResultPage::refresh() {
    if (m_paperId <= 0) return;
    const auto entries = ctx().questionService().entries(m_paperId);
    const PaperStat stat = ctx().questionService().stat(m_paperId);

    m_score->setText(QStringLiteral("%1 分").arg(stat.score, 0, 'f', 1));
    m_summary->setText(QStringLiteral("共 %1 题　正确 %2　错误 %3　用时 %4 秒")
                           .arg(stat.total).arg(stat.correct).arg(stat.wrong)
                           .arg(stat.totalTimeMs / 1000.0, 0, 'f', 1));

    ui::clearTable(m_table);
    m_table->setRowCount(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        const QString stem = e.question.stem.simplified();
        const QString shown = stem.size() > 60 ? stem.left(60) + QStringLiteral("…") : stem;
        ui::setRow(m_table, i,
                   {QString::number(i + 1), shown,
                    e.answered ? e.userAnswer : QStringLiteral("(未作答)"), e.question.answer,
                    e.isCorrect ? QStringLiteral("✔ 正确") : QStringLiteral("✘ 错误")});
        if (!e.isCorrect && e.answered)
            m_table->item(i, 4)->setForeground(QColor(QStringLiteral("#D64545")));
        else if (e.isCorrect)
            m_table->item(i, 4)->setForeground(QColor(QStringLiteral("#2E9E5B")));
    }
    m_table->resizeColumnsToContents();
    if (m_table->columnCount() > 1) m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void ResultPage::onAddWrong() {
    const qint64 uid = ctx().currentUserId();
    const auto entries = ctx().questionService().entries(m_paperId);
    int added = 0;
    for (const auto &e : entries) {
        if (e.answered && !e.isCorrect && e.question.wordId > 0) {
            if (ctx().wrongBookService().recordWrong(uid, e.question.wordId, e.question.type, e.userAnswer).isOk())
                ++added;
        }
    }
    ui::info(this, QStringLiteral("已将 %1 道错题加入错题本").arg(added));
    emit ctx().dataChanged();
}

void ResultPage::onRetake() {
    const qint64 uid = ctx().currentUserId();
    const Setting s = ctx().currentSetting();
    const auto entries = ctx().questionService().entries(m_paperId);
    QList<qint64> wordIds;
    for (const auto &e : entries)
        if (e.question.wordId > 0) wordIds << e.question.wordId;
    if (wordIds.isEmpty()) { ui::warn(this, QStringLiteral("没有可重做的单词")); return; }
    auto paper = ctx().questionService().generatePaperFromWords(uid, wordIds, s.questionTypes,
                                                                QStringLiteral("重做练习"));
    if (!paper) { ui::warn(this, paper.error()); return; }
    nav()->startQuiz(paper.value().id);
}

void ResultPage::onHome() { nav()->navigateTo(QStringLiteral("home")); }

} // namespace vt
