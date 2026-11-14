#include "ui/pages/PracticePage.h"

#include "core/AppContext.h"
#include "core/Utils.h"
#include "ui/Navigation.h"
#include "ui/UiHelpers.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace vt {

namespace {
const QString kSrcDaily = QStringLiteral("daily");
const QString kSrcBook = QStringLiteral("book");
const QString kSrcWrong = QStringLiteral("wrong");
const QString kSrcRandom = QStringLiteral("random");
} // namespace

PracticePage::PracticePage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("练习"), this);
    title->setObjectName(QStringLiteral("Header"));
    root->addWidget(title);

    auto *box = new QGroupBox(QStringLiteral("选择题目来源"), this);
    auto *bar = new QGridLayout(box);
    bar->setSpacing(8);

    m_sourceCombo = new QComboBox(box);
    m_sourceCombo->addItem(QStringLiteral("每日词表（今日背诵计划）"), kSrcDaily);
    m_sourceCombo->addItem(QStringLiteral("单词本（收藏夹）"), kSrcBook);
    m_sourceCombo->addItem(QStringLiteral("错题本"), kSrcWrong);
    m_sourceCombo->addItem(QStringLiteral("词库随机抽取"), kSrcRandom);
    m_sourceCombo->setMinimumWidth(220);

    m_bookLabel = new QLabel(QStringLiteral("单词本"), box);
    m_bookCombo = new QComboBox(box);
    m_bookCombo->setMinimumWidth(180);

    m_countSpin = new QSpinBox(box);
    m_countSpin->setRange(1, 300);
    m_countSpin->setValue(20);
    m_countSpin->setSuffix(QStringLiteral(" 题"));

    m_startBtn = new QPushButton(QStringLiteral("开始做题"), box);

    bar->addWidget(new QLabel(QStringLiteral("来源"), box), 0, 0);
    bar->addWidget(m_sourceCombo, 0, 1, 1, 3);
    bar->addWidget(m_bookLabel, 1, 0);
    bar->addWidget(m_bookCombo, 1, 1, 1, 3);
    bar->addWidget(new QLabel(QStringLiteral("题目数量"), box), 2, 0);
    bar->addWidget(m_countSpin, 2, 1);
    bar->setColumnStretch(4, 1);
    bar->addWidget(m_startBtn, 2, 3);
    root->addWidget(box);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setObjectName(QStringLiteral("SubHeader"));
    m_infoLabel->setWordWrap(true);
    root->addWidget(m_infoLabel);

    auto *tip = new QLabel(this);
    tip->setObjectName(QStringLiteral("SubHeader"));
    tip->setWordWrap(true);
    tip->setText(QStringLiteral(
        "说明：题型取自「个人设置 → 题型偏好」。\n"
        "作答后系统会自动评分；答错的题会记入错题本（可在「个人设置」中关闭自动记录）。"));
    root->addWidget(tip);
    root->addStretch();

    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PracticePage::onSourceChanged);
    connect(m_startBtn, &QPushButton::clicked, this, &PracticePage::onStart);
    connect(&ctx, &AppContext::dataChanged, this, &PracticePage::refresh);

    const Setting s = ctx.currentSetting();
    if (s.dailyWordCount > 0) m_countSpin->setValue(s.dailyWordCount);
    onSourceChanged();
}
QString PracticePage::currentSource() const { return m_sourceCombo->currentData().toString(); }

void PracticePage::onSourceChanged() {
    const QString src = currentSource();
    const bool bookMode = (src == kSrcBook);
    m_bookLabel->setVisible(bookMode);
    m_bookCombo->setVisible(bookMode);
    refresh();
}

void PracticePage::refresh() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;

    // 单词本列表
    {
        const qint64 keep = m_bookCombo->currentData().toLongLong();
        m_bookCombo->blockSignals(true);
        m_bookCombo->clear();
        for (const auto &b : ctx().wordBookService().listBooks(uid))
            m_bookCombo->addItem(QStringLiteral("%1（%2）").arg(b.name).arg(b.entryCount), b.id);
        m_bookCombo->blockSignals(false);
        const int idx = m_bookCombo->findData(keep);
        if (idx >= 0) m_bookCombo->setCurrentIndex(idx);
    }

    const QString src = currentSource();
    QString info;
    if (src == kSrcDaily) {
        auto today = ctx().dailyListService().today(uid);
        if (today) {
            const auto entries = ctx().dailyListService().entries(today.value().id);
            int nNew = 0, nRev = 0;
            for (const auto &e : entries) {
                if (e.sourceType == EntrySource::New) ++nNew;
                else ++nRev;
            }
            info = QStringLiteral("今日词表共 %1 词（新词 %2、复习 %3）。若尚未生成，请先到「每日词表」生成。")
                       .arg(entries.size()).arg(nNew).arg(nRev);
        } else {
            info = QStringLiteral("今日尚未生成词表，请先到「每日词表」页面生成。");
        }
    } else if (src == kSrcBook) {
        const qint64 bookId = m_bookCombo->currentData().toLongLong();
        info = bookId > 0
                   ? QStringLiteral("当前单词本共 %1 词。").arg(ctx().wordBookService().count(bookId))
                   : QStringLiteral("尚无单词本，请先到「单词本」页面新建。");
    } else if (src == kSrcWrong) {
        info = QStringLiteral("错题本共 %1 道错题（未掌握的会优先出现）。").arg(ctx().wrongBookService().count(uid));
    } else {
        info = QStringLiteral("从词库（共 %1 词）中随机抽取题目。").arg(ctx().wordService().count());
    }
    m_infoLabel->setText(info);
}

void PracticePage::onStart() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    const Setting s = ctx().currentSetting();
    const int count = m_countSpin->value();
    const QString src = currentSource();

    // 词库随机：直接按模式生成
    if (src == kSrcRandom) {
        auto paper = ctx().questionService().generatePaper(uid, PaperMode::Random, count, s.questionTypes,
                                                           QStringLiteral("随机练习"));
        if (!paper) { ui::warn(this, paper.error()); return; }
        nav()->startQuiz(paper.value().id);
        return;
    }

    QList<qint64> ids;
    QString title;
    if (src == kSrcDaily) {
        auto today = ctx().dailyListService().today(uid);
        if (!today) {
            ui::warn(this, QStringLiteral("今日尚未生成词表，请先到「每日词表」页面生成。"));
            return;
        }
        const auto entries = ctx().dailyListService().entries(today.value().id);
        for (const auto &e : entries) ids << e.wordId;
        title = QStringLiteral("每日词表练习");
    } else if (src == kSrcBook) {
        const qint64 bookId = m_bookCombo->currentData().toLongLong();
        if (bookId <= 0) { ui::warn(this, QStringLiteral("请先在「单词本」中新建单词本并添加单词。")); return; }
        ids = ctx().wordBooks().wordIdsOfBook(bookId);
        title = QStringLiteral("单词本练习");
    } else { // wrong
        const auto b = ctx().wrongBookService().book(uid);
        if (!b) { ui::warn(this, QStringLiteral("错题本为空，做错题后会自动收录。")); return; }
        ids = ctx().wrongBooks().wordIdsOfBook(b.value().id);
        title = QStringLiteral("错题复习");
    }

    if (ids.isEmpty()) {
        ui::warn(this, QStringLiteral("该来源暂无可练习的单词。"));
        return;
    }
    if (ids.size() > count) {
        util::shuffle(ids);
        ids = ids.mid(0, count);
    }
    auto paper = ctx().questionService().generatePaperFromWords(uid, ids, s.questionTypes, title);
    if (!paper) { ui::warn(this, paper.error()); return; }
    nav()->startQuiz(paper.value().id);
}

} // namespace vt
