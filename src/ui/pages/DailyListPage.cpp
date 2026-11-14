#include "ui/pages/DailyListPage.h"

#include "core/AppContext.h"
#include "core/Utils.h"
#include "ui/Navigation.h"
#include "ui/UiHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace vt {

DailyListPage::DailyListPage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("每日词表"), this);
    title->setObjectName(QStringLiteral("Header"));
    auto *sub = new QLabel(QStringLiteral("自定义背诵计划：背诵天数、每天背多少新词、复习多少词"), this);
    sub->setObjectName(QStringLiteral("SubHeader"));
    header->addWidget(title);
    header->addSpacing(12);
    header->addWidget(sub);
    header->addStretch();
    root->addLayout(header);

    // ---- 计划选择 ----
    auto *pickBar = new QHBoxLayout;
    pickBar->addWidget(new QLabel(QStringLiteral("背诵计划"), this));
    m_planCombo = new QComboBox(this);
    m_planCombo->setMinimumWidth(200);
    auto *newBtn = new QPushButton(QStringLiteral("新建计划"), this);
    newBtn->setProperty("flat", true);
    auto *saveBtn = new QPushButton(QStringLiteral("保存计划"), this);
    m_deleteBtn = new QPushButton(QStringLiteral("删除计划"), this);
    m_deleteBtn->setProperty("danger", true);
    auto *genBtn = new QPushButton(QStringLiteral("生成今日词表"), this);
    pickBar->addWidget(m_planCombo);
    pickBar->addWidget(newBtn);
    pickBar->addWidget(saveBtn);
    pickBar->addWidget(m_deleteBtn);
    pickBar->addStretch();
    pickBar->addWidget(genBtn);
    root->addLayout(pickBar);

    // ---- 计划参数 ----
    auto *planBox = new QGroupBox(QStringLiteral("计划参数"), this);
    auto *planBar = new QGridLayout(planBox);
    planBar->setSpacing(8);

    m_bookCombo = new QComboBox(planBox);
    m_bookCombo->addItem(QStringLiteral("全部词书"), QString());
    m_bookCombo->addItem(QStringLiteral("四级"), QStringLiteral("CET4"));
    m_bookCombo->addItem(QStringLiteral("六级"), QStringLiteral("CET6"));

    m_daysSpin = new QSpinBox(planBox);
    m_daysSpin->setRange(1, 999);
    m_daysSpin->setSuffix(QStringLiteral(" 天"));
    m_daysSpin->setValue(30);

    m_newSpin = new QSpinBox(planBox);
    m_newSpin->setRange(0, 300);
    m_newSpin->setSuffix(QStringLiteral(" 词/天"));
    m_newSpin->setValue(20);

    m_reviewSpin = new QSpinBox(planBox);
    m_reviewSpin->setRange(0, 300);
    m_reviewSpin->setSuffix(QStringLiteral(" 词/天"));
    m_reviewSpin->setValue(20);

    m_includeWrong = new QCheckBox(QStringLiteral("复习包含错题本"), planBox);
    m_includeWrong->setChecked(true);

    planBar->addWidget(new QLabel(QStringLiteral("背诵词书"), planBox), 0, 0);
    planBar->addWidget(m_bookCombo, 0, 1);
    planBar->addWidget(new QLabel(QStringLiteral("背诵天数"), planBox), 0, 2);
    planBar->addWidget(m_daysSpin, 0, 3);
    planBar->addWidget(new QLabel(QStringLiteral("每天背新词"), planBox), 1, 0);
    planBar->addWidget(m_newSpin, 1, 1);
    planBar->addWidget(new QLabel(QStringLiteral("每天复习"), planBox), 1, 2);
    planBar->addWidget(m_reviewSpin, 1, 3);
    planBar->addWidget(m_includeWrong, 2, 0, 1, 3);
    planBar->setColumnStretch(4, 1);
    root->addWidget(planBox);

    // ---- 进度 / 历史 ----
    auto *infoBar = new QHBoxLayout;
    m_progressLabel = new QLabel(this);
    m_progressLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
    m_progressLabel->setWordWrap(true);
    m_historyCombo = new QComboBox(this);
    m_historyCombo->setMinimumWidth(180);
    m_infoLabel = new QLabel(this);
    m_infoLabel->setObjectName(QStringLiteral("SubHeader"));
    infoBar->addWidget(m_progressLabel, 1);
    infoBar->addWidget(new QLabel(QStringLiteral("历史"), this));
    infoBar->addWidget(m_historyCombo);
    infoBar->addWidget(m_infoLabel);
    root->addLayout(infoBar);

    m_table = ui::makeTable({QStringLiteral("序号"), QStringLiteral("单词"), QStringLiteral("释义"),
                             QStringLiteral("类型"), QStringLiteral("完成")},
                            this);
    root->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *toggleBtn = new QPushButton(QStringLiteral("切换完成状态"), this);
    toggleBtn->setProperty("secondary", true);
    actions->addStretch();
    actions->addWidget(toggleBtn);
    root->addLayout(actions);

    connect(m_planCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &DailyListPage::onPlanChanged);
    connect(newBtn, &QPushButton::clicked, this, &DailyListPage::onNewPlan);
    connect(saveBtn, &QPushButton::clicked, this, &DailyListPage::onSavePlan);
    connect(m_deleteBtn, &QPushButton::clicked, this, &DailyListPage::onDeletePlan);
    connect(genBtn, &QPushButton::clicked, this, &DailyListPage::onGenerateToday);
    connect(toggleBtn, &QPushButton::clicked, this, &DailyListPage::onToggleCompleted);
    connect(m_historyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &DailyListPage::onHistoryChanged);
    connect(&ctx, &AppContext::dataChanged, this, &DailyListPage::refresh);
}

// 重新载入计划下拉框（保持当前选择）
void DailyListPage::reloadPlans() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;

    qint64 activeId = 0;
    if (auto ap = ctx().dailyListService().activePlan(uid)) activeId = ap.value().id;
    const qint64 keep = m_currentPlanId > 0 ? m_currentPlanId : activeId;

    m_planCombo->blockSignals(true);
    m_planCombo->clear();
    const auto plans = ctx().dailyListService().listPlans(uid);
    for (const auto &p : plans) {
        const QString label = QStringLiteral("%1%2（%3 天 · 新 %4 / 复 %5）")
                                  .arg(p.name)
                                  .arg(p.active ? QStringLiteral(" ★当前") : QString())
                                  .arg(p.totalDays)
                                  .arg(p.newPerDay)
                                  .arg(p.reviewPerDay);
        m_planCombo->addItem(label, p.id);
    }
    m_planCombo->blockSignals(false);

    int idx = m_planCombo->findData(keep);
    if (idx < 0 && m_planCombo->count() > 0) idx = 0;
    if (idx >= 0) {
        m_planCombo->blockSignals(true);
        m_planCombo->setCurrentIndex(idx);
        m_planCombo->blockSignals(false);
        m_currentPlanId = m_planCombo->itemData(idx).toLongLong();
    } else {
        m_currentPlanId = 0;
    }
    m_deleteBtn->setEnabled(m_currentPlanId > 0);
    loadPlanWidgets();
}

void DailyListPage::loadPlanWidgets() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    if (m_currentPlanId <= 0) return;
    const auto p = ctx().studyPlans().findById(m_currentPlanId);
    if (!p) return;
    const int bi = m_bookCombo->findData(p->book);
    m_bookCombo->setCurrentIndex(bi >= 0 ? bi : 0);
    m_daysSpin->setValue(p->totalDays);
    m_newSpin->setValue(p->newPerDay);
    m_reviewSpin->setValue(p->reviewPerDay);
    m_includeWrong->setChecked(p->includeWrong);
}

void DailyListPage::refresh() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;

    reloadPlans();

    const qint64 keep = m_currentListId;
    m_historyCombo->blockSignals(true);
    m_historyCombo->clear();
    const auto lists = ctx().dailyListService().recent(uid, 30);
    for (const auto &l : lists)
        m_historyCombo->addItem(QStringLiteral("%1（%2/%3）").arg(l.listDate).arg(l.doneCount).arg(l.totalCount),
                                l.id);
    m_historyCombo->blockSignals(false);

    qint64 target = keep;
    if (target <= 0 && !lists.isEmpty()) target = lists.first().id;
    if (target > 0) {
        const int idx = m_historyCombo->findData(target);
        if (idx >= 0) {
            m_historyCombo->blockSignals(true);
            m_historyCombo->setCurrentIndex(idx);
            m_historyCombo->blockSignals(false);
        }
        loadList(target);
    } else {
        m_currentListId = 0;
        ui::clearTable(m_table);
        m_infoLabel->setText(QStringLiteral("尚未生成今日词表"));
    }
    updateProgressLabel();
}

void DailyListPage::updateProgressLabel() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    const auto pr = ctx().dailyListService().progress(uid);
    if (pr.totalDays <= 0) {
        m_progressLabel->setText(QStringLiteral("尚未设置背诵计划"));
        return;
    }
    QString text = QStringLiteral("第 %1 / %2 天　·　计划已学 %3 / %4 词　·　今日 新词 %5、复习 %6")
                       .arg(qMin(pr.dayIndex, pr.totalDays))
                       .arg(pr.totalDays)
                       .arg(pr.learned)
                       .arg(pr.totalWords)
                       .arg(pr.todayNew)
                       .arg(pr.todayReview);
    if (pr.neededDays > 0) {
        if (pr.coversAll) {
            text += QStringLiteral("　·　约 %1 天可覆盖全部 %2 词 ✔").arg(pr.neededDays).arg(pr.totalWords);
        } else {
            text += QStringLiteral("　·　⚠ 按每天新词数需 %1 天才能覆盖全部 %2 词，当前计划仅 %3 天")
                        .arg(pr.neededDays)
                        .arg(pr.totalWords)
                        .arg(pr.totalDays);
        }
    }
    m_progressLabel->setText(text);
}

void DailyListPage::loadList(qint64 listId) {
    m_currentListId = listId;
    const auto entries = ctx().dailyListService().entries(listId);
    ui::clearTable(m_table);
    m_table->setRowCount(entries.size());
    int done = 0;
    for (int i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        if (e.isCompleted) ++done;
        ui::setRow(m_table, i,
                   {QString::number(e.seq + 1), e.word.spelling, ui::shortMeaning(e.word, 34),
                    displayName(e.sourceType), e.isCompleted ? QStringLiteral("✔") : QString()});
        m_table->item(i, 1)->setData(Qt::UserRole, e.id);
    }
    m_table->resizeColumnsToContents();
    if (m_table->columnCount() > 2)
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_infoLabel->setText(QStringLiteral("共 %1 词，已完成 %2").arg(entries.size()).arg(done));
}

void DailyListPage::onPlanChanged() {
    const qint64 uid = ctx().currentUserId();
    const qint64 planId = m_planCombo->currentData().toLongLong();
    if (uid <= 0 || planId <= 0) return;
    m_currentPlanId = planId;
    m_deleteBtn->setEnabled(true);
    const auto st = ctx().dailyListService().setActivePlan(uid, planId);
    if (!st.isOk()) { ui::warn(this, st.error()); return; }
    loadPlanWidgets();
    updateProgressLabel();
}

void DailyListPage::onNewPlan() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;

    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建背诵计划"),
                                               QStringLiteral("计划名称："), QLineEdit::Normal,
                                               QStringLiteral("我的背诵计划"), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    StudyPlan plan;
    plan.userId = uid;
    plan.name = name.trimmed();
    plan.book = m_bookCombo->currentData().toString();
    plan.totalDays = m_daysSpin->value();
    plan.newPerDay = m_newSpin->value();
    plan.reviewPerDay = m_reviewSpin->value();
    plan.includeWrong = m_includeWrong->isChecked();

    const auto saved = ctx().dailyListService().savePlan(plan);
    if (!saved) { ui::warn(this, saved.error()); return; }
    m_currentPlanId = saved.value().id;
    refresh();
    ui::info(this, QStringLiteral("已创建计划「%1」并设为当前计划").arg(saved.value().name));
    emit ctx().dataChanged();
}

void DailyListPage::onSavePlan() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    if (m_currentPlanId <= 0) { ui::warn(this, QStringLiteral("请先新建一个背诵计划")); return; }

    const auto existing = ctx().studyPlans().findById(m_currentPlanId);
    if (!existing) { ui::warn(this, QStringLiteral("计划不存在，请刷新后重试")); return; }

    StudyPlan plan = *existing;
    plan.book = m_bookCombo->currentData().toString();
    plan.totalDays = m_daysSpin->value();
    plan.newPerDay = m_newSpin->value();
    plan.reviewPerDay = m_reviewSpin->value();
    plan.includeWrong = m_includeWrong->isChecked();

    const auto saved = ctx().dailyListService().savePlan(plan);
    if (!saved) { ui::warn(this, saved.error()); return; }
    refresh();
    ui::info(this, QStringLiteral("计划「%1」已保存：%2 天，每天新词 %3、复习 %4")
                      .arg(saved.value().name)
                      .arg(saved.value().totalDays)
                      .arg(saved.value().newPerDay)
                      .arg(saved.value().reviewPerDay));
    emit ctx().dataChanged();
}

void DailyListPage::onDeletePlan() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0 || m_currentPlanId <= 0) return;
    if (!ui::confirm(this, QStringLiteral("确定删除当前背诵计划？已生成的历史词表会保留。"))) return;
    const auto st = ctx().dailyListService().removePlan(uid, m_currentPlanId);
    if (!st.isOk()) { ui::warn(this, st.error()); return; }
    m_currentPlanId = 0;
    refresh();
    emit ctx().dataChanged();
}

void DailyListPage::onGenerateToday() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    if (m_currentPlanId <= 0) { ui::warn(this, QStringLiteral("请先新建或选择一个背诵计划")); return; }

    // 确保生成的是“当前选中”的计划
    ctx().dailyListService().setActivePlan(uid, m_currentPlanId);

    bool force = false;
    if (auto existing = ctx().dailyListService().today(uid)) {
        if (!ui::confirm(this, QStringLiteral("今日已生成词表（%1 词），是否按当前计划重新生成？")
                                   .arg(existing.value().totalCount)))
            return;
        force = true;
    }
    auto list = ctx().dailyListService().generateToday(uid, force);
    if (!list) { ui::warn(this, list.error()); return; }
    m_currentListId = list.value().id;
    refresh();
    emit ctx().dataChanged();
}

void DailyListPage::onHistoryChanged() {
    const qint64 id = m_historyCombo->currentData().toLongLong();
    if (id > 0) {
        loadList(id);
        updateProgressLabel();
    }
}

void DailyListPage::onToggleCompleted() {
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) { ui::warn(this, QStringLiteral("请先选择一个单词")); return; }
    const int row = rows.first().row();
    const qint64 entryId = m_table->item(row, 1)->data(Qt::UserRole).toLongLong();
    const bool nowDone = m_table->item(row, 4)->text().isEmpty();
    ctx().dailyListService().markEntryCompleted(entryId, nowDone);
    if (m_currentListId > 0) ctx().dailyListService().completeList(m_currentListId, false);
    loadList(m_currentListId);
    updateProgressLabel();
    emit ctx().dataChanged();
}

} // namespace vt
