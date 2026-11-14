#include "core/Logger.h"
#include "core/Utils.h"
#include "service/Services.h"

#include <QSet>

namespace vt {

// ---------------------------------------------------------------------------
// 背诵计划
// ---------------------------------------------------------------------------
Result<StudyPlan> DailyListService::activePlan(qint64 userId) {
    const auto p = m_plans.activePlan(userId);
    if (!p) return Result<StudyPlan>::err(QStringLiteral("尚未设置背诵计划"));
    return Result<StudyPlan>::ok(*p);
}

QList<StudyPlan> DailyListService::listPlans(qint64 userId) { return m_plans.listPlans(userId); }

Result<StudyPlan> DailyListService::savePlan(const StudyPlan &plan) {
    StudyPlan p = plan;
    if (p.totalDays <= 0) return Result<StudyPlan>::err(QStringLiteral("背诵天数必须大于 0"));
    if (p.newPerDay <= 0 && p.reviewPerDay <= 0)
        return Result<StudyPlan>::err(QStringLiteral("每天新词与复习数量不能同时为 0"));
    if (p.newPerDay < 0 || p.reviewPerDay < 0)
        return Result<StudyPlan>::err(QStringLiteral("数量不能为负数"));
    if (p.name.trimmed().isEmpty()) p.name = QStringLiteral("我的背诵计划");
    p.active = true;

    // 新建计划：首次创建时记录开始日期；更新计划：保留原开始日期
    if (p.id <= 0) p.startDate = util::todayDate();

    const qint64 id = m_plans.save(p);
    if (id < 0) return Result<StudyPlan>::err(QStringLiteral("背诵计划保存失败"));

    // 保存后设为当前计划（同一用户同时只有一个启用计划）
    m_plans.setActive(p.userId, id);

    const auto saved = m_plans.findById(id);
    if (!saved) return Result<StudyPlan>::err(QStringLiteral("背诵计划保存失败"));
    Logger::info(QStringLiteral("Plan"),
                 QStringLiteral("保存背诵计划 #%1「%2」天数=%3 新词/天=%4 复习/天=%5 词书=%6")
                     .arg(id).arg(saved->name).arg(saved->totalDays).arg(saved->newPerDay)
                     .arg(saved->reviewPerDay)
                     .arg(saved->book.isEmpty() ? QStringLiteral("全部") : saved->book));
    return Result<StudyPlan>::ok(*saved);
}

Status DailyListService::setActivePlan(qint64 userId, qint64 planId) {
    const auto p = m_plans.findById(planId);
    if (!p || p->userId != userId) return Status::err(QStringLiteral("计划不存在"));
    return m_plans.setActive(userId, planId) ? Status::ok() : Status::err(QStringLiteral("切换计划失败"));
}

Status DailyListService::removePlan(qint64 userId, qint64 planId) {
    const auto p = m_plans.findById(planId);
    if (!p || p->userId != userId) return Status::err(QStringLiteral("计划不存在"));
    const bool wasActive = p->active;
    if (!m_plans.remove(planId)) return Status::err(QStringLiteral("删除计划失败"));
    if (wasActive) {
        // 若删除的是当前计划，自动启用剩余的最新计划
        const auto rest = m_plans.listPlans(userId);
        if (!rest.isEmpty()) m_plans.setActive(userId, rest.first().id);
    }
    return Status::ok();
}

// ---------------------------------------------------------------------------
// 今日词表
// ---------------------------------------------------------------------------
Result<DailyList> DailyListService::generateToday(qint64 userId, bool force) {
    const QString date = util::todayDate();

    const auto existing = m_daily.findByDate(userId, date);
    if (existing && !force) return Result<DailyList>::ok(*existing);

    const auto planOpt = m_plans.activePlan(userId);
    if (!planOpt)
        return Result<DailyList>::err(QStringLiteral("尚未设置背诵计划，请先新建或选择一个背诵计划"));
    const StudyPlan &plan = *planOpt;

    const QDateTime now = QDateTime::currentDateTime();

    // 新词：词库中尚未纳入计划的单词，按词书顺序取
    QList<qint64> newIds = m_plans.nextNewWordIds(plan.id, plan.book, plan.newPerDay);

    // 复习：计划内已学/未完成的到期单词
    QList<qint64> reviewIds = m_plans.dueReviewWordIds(plan.id, now, plan.reviewPerDay);

    // 复习：可选的错题本到期单词
    QList<qint64> wrongIds;
    if (plan.includeWrong) {
        const qint64 wrongBookId = m_wrong.ensureBook(userId);
        const auto due = m_wrong.dueForReview(wrongBookId, now, plan.reviewPerDay);
        for (const auto &e : due) wrongIds << e.wordId;
    }

    QSet<qint64> have;
    QList<qint64> newFinal, reviewFinal, wrongFinal;
    for (auto id : newIds) {
        if (have.contains(id)) continue;
        have.insert(id);
        newFinal << id;
    }
    for (auto id : wrongIds) {
        if (have.contains(id)) continue;
        have.insert(id);
        wrongFinal << id;
    }
    for (auto id : reviewIds) {
        if (have.contains(id)) continue;
        have.insert(id);
        reviewFinal << id;
    }

    // 不允许生成长度为 0 的词表（也不要因此删掉已有的今日词表）
    if (newFinal.isEmpty() && reviewFinal.isEmpty() && wrongFinal.isEmpty())
        return Result<DailyList>::err(
            QStringLiteral("今日没有可安排的内容（词库已学完且无到期复习）。"
                           "可调整背诵计划或关闭「复习包含错题本」后重试。"));

    // 确认有新内容后再替换旧的今日词表
    if (existing) m_daily.removeByDate(userId, date);

    DailyList list;
    list.userId = userId;
    list.listDate = date;
    list.mode = PaperMode::Plan;
    list.totalCount = newFinal.size() + wrongFinal.size() + reviewFinal.size();

    const qint64 listId = m_daily.createList(list);
    if (listId < 0) return Result<DailyList>::err(QStringLiteral("每日词表创建失败"));
    list.id = listId;

    int seq = 0;
    auto addEntries = [&](const QList<qint64> &ids, EntrySource source) {
        for (auto id : ids) {
            DailyEntry e;
            e.dailyListId = listId;
            e.wordId = id;
            e.sourceType = source;
            e.seq = seq++;
            m_daily.addEntry(e);
        }
    };
    addEntries(newFinal, EntrySource::New);
    addEntries(wrongFinal, EntrySource::WrongBook);
    addEntries(reviewFinal, EntrySource::Review);

    // 新词纳入计划，之后不再作为“新词”出现
    m_plans.addPlanWords(plan.id, newFinal, date);

    Logger::info(QStringLiteral("Daily"),
                 QStringLiteral("生成今日词表 %1（计划「%2」）：新词 %3，错题复习 %4，到期复习 %5")
                     .arg(date).arg(plan.name).arg(newFinal.size()).arg(wrongFinal.size())
                     .arg(reviewFinal.size()));
    return Result<DailyList>::ok(list);
}

Result<DailyList> DailyListService::today(qint64 userId) {
    if (auto l = m_daily.findByDate(userId, util::todayDate())) return Result<DailyList>::ok(*l);
    return Result<DailyList>::err(QStringLiteral("今日尚未生成词表"));
}

QList<DailyList> DailyListService::recent(qint64 userId, int limit) { return m_daily.listRecent(userId, limit); }

QList<DailyEntry> DailyListService::entries(qint64 listId) { return m_daily.listEntries(listId); }

Status DailyListService::markEntryCompleted(qint64 entryId, bool completed) {
    if (!m_daily.setEntryCompleted(entryId, completed))
        return Status::err(QStringLiteral("更新完成状态失败"));
    return Status::ok();
}

Status DailyListService::completeList(qint64 listId, bool completed) {
    const auto entries = m_daily.listEntries(listId);
    int done = 0;
    for (const auto &e : entries)
        if (e.isCompleted) ++done;
    m_daily.updateProgress(listId, done, completed);
    return Status::ok();
}

DailyListService::PlanProgress DailyListService::progress(qint64 userId) {
    PlanProgress pr;
    const auto planOpt = m_plans.activePlan(userId);
    if (!planOpt) return pr;

    const StudyPlan &plan = *planOpt;
    const QString today = util::todayDate();
    pr.dayIndex = plan.dayIndex(today);
    pr.totalDays = plan.totalDays;
    pr.learned = m_plans.countLearned(plan.id);
    pr.inPlan = m_plans.countInPlan(plan.id);
    pr.totalWords = m_words.countAll(plan.book);
    pr.remaining = pr.totalWords > pr.inPlan ? pr.totalWords - pr.inPlan : 0;

    // 覆盖全部词库所需天数
    if (plan.newPerDay > 0 && pr.totalWords > 0) {
        pr.neededDays = static_cast<int>((pr.totalWords + plan.newPerDay - 1) / plan.newPerDay);
        pr.coversAll = plan.totalDays >= pr.neededDays;
    }

    if (auto l = m_daily.findByDate(userId, today)) {
        for (const auto &e : m_daily.listEntries(l->id)) {
            if (e.sourceType == EntrySource::New) ++pr.todayNew;
            else if (e.sourceType == EntrySource::Review || e.sourceType == EntrySource::WrongBook)
                ++pr.todayReview;
        }
    }
    return pr;
}

} // namespace vt
