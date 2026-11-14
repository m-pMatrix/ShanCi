#include "core/Logger.h"
#include "core/Utils.h"
#include "domain/Rules.h"
#include "generator/QuestionStrategy.h"
#include "service/Services.h"

#include <QSet>

namespace vt {

Question QuestionService::generateQuestion(QuestionType type, const Word &target, const QList<Word> &pool,
                                           const Setting &setting) const {
    return QuestionStrategyRegistry::instance().generate(type, target, pool, setting);
}

QList<Word> QuestionService::fetchPool(const QList<BookType> &books, int n, const QList<qint64> &exclude) const {
    QList<Word> pool;
    QSet<qint64> seen;
    for (auto id : exclude) seen.insert(id);
    if (books.isEmpty()) {
        for (auto &w : m_words.randomByBook(QString(), n, exclude)) {
            if (!seen.contains(w.id)) { seen.insert(w.id); pool << w; }
        }
        return pool;
    }
    const int perBook = std::max(1, static_cast<int>(n / books.size() + 1));
    for (auto b : books) {
        for (auto &w : m_words.randomByBook(toKey(b), perBook, exclude)) {
            if (!seen.contains(w.id)) { seen.insert(w.id); pool << w; }
        }
    }
    return pool;
}

Result<Paper> QuestionService::generatePaper(qint64 userId, PaperMode mode, int count,
                                             const QList<QuestionType> &types, const QString &title) {
    if (count <= 0) return Result<Paper>::err(QStringLiteral("题目数量必须大于 0"));

    Setting setting = m_settings.load(userId);
    const QList<QuestionType> preferred = types.isEmpty() ? setting.questionTypes : types;

    const auto bookOpt = m_books.findDefaultBook(userId);
    const qint64 bookId = bookOpt ? bookOpt->id : -1;
    const qint64 wrongBookId = m_wrong.ensureBook(userId);

    QList<qint64> sourceIds;
    QList<Word> selected;

    auto collectWordBook = [&]() -> QList<qint64> {
        return bookId > 0 ? m_books.wordIdsOfBook(bookId) : QList<qint64>{};
    };
    auto collectWrong = [&]() -> QList<qint64> {
        return wrongBookId > 0 ? m_wrong.wordIdsOfBook(wrongBookId) : QList<qint64>{};
    };

    switch (mode) {
    case PaperMode::Random:
        selected = m_words.randomByBook(QString(), count);
        break;
    case PaperMode::WordBookOnly:
        sourceIds = collectWordBook();
        break;
    case PaperMode::WrongOnly:
        sourceIds = collectWrong();
        break;
    case PaperMode::Mixed: {
        sourceIds = collectWrong();
        const auto wb = collectWordBook();
        QSet<qint64> have;
        for (auto id : sourceIds) have.insert(id);
        for (auto id : wb) if (!have.contains(id)) sourceIds << id;
        break;
    }
    }

    if (mode != PaperMode::Random) {
        if (sourceIds.isEmpty())
            return Result<Paper>::err(
                QStringLiteral("单词本为空，请先在「单词本」页点击「从词库填充」，或先导入词表"));
        util::shuffle(sourceIds);
        if (sourceIds.size() > count) sourceIds = sourceIds.mid(0, count);
        // 分块读取
        QList<qint64> chunk;
        for (int i = 0; i < sourceIds.size(); ++i) {
            chunk << sourceIds[i];
            if (chunk.size() >= 400 || i == sourceIds.size() - 1) {
                selected << m_words.findByIds(chunk);
                chunk.clear();
            }
        }
    }

    if (selected.isEmpty()) return Result<Paper>::err(QStringLiteral("没有可用的单词"));
    return buildPaper(userId, selected, mode, preferred, setting, title);
}

Result<Paper> QuestionService::generatePaperFromWords(qint64 userId, const QList<qint64> &wordIds,
                                                      const QList<QuestionType> &types, const QString &title) {
    if (wordIds.isEmpty()) return Result<Paper>::err(QStringLiteral("没有可用的单词"));
    Setting setting = m_settings.load(userId);
    const QList<QuestionType> preferred = types.isEmpty() ? setting.questionTypes : types;

    QList<qint64> chunk;
    QList<Word> selected;
    for (int i = 0; i < wordIds.size(); ++i) {
        chunk << wordIds[i];
        if (chunk.size() >= 400 || i == wordIds.size() - 1) {
            selected << m_words.findByIds(chunk);
            chunk.clear();
        }
    }
    if (selected.isEmpty()) return Result<Paper>::err(QStringLiteral("没有可用的单词"));
    return buildPaper(userId, selected, PaperMode::Random, preferred, setting,
                      title.isEmpty() ? QStringLiteral("每日词表练习") : title);
}

Result<Paper> QuestionService::buildPaper(qint64 userId, QList<Word> selected, PaperMode mode,
                                          const QList<QuestionType> &preferred, const Setting &setting,
                                          const QString &title) {
    if (selected.isEmpty()) return Result<Paper>::err(QStringLiteral("没有可用的单词"));

    // 去重（按 id，保持顺序）
    QList<Word> unique;
    QSet<qint64> seenIds;
    for (const auto &w : selected) {
        if (seenIds.contains(w.id)) continue;
        seenIds.insert(w.id);
        unique << w;
    }
    selected = unique;

    // 词池（用于干扰项）
    QSet<BookType> bookSet;
    QList<qint64> excludeIds;
    for (const auto &w : selected) { bookSet.insert(w.book); excludeIds << w.id; }
    const QList<BookType> books(bookSet.begin(), bookSet.end());
    const int poolSize = std::max(40, static_cast<int>(selected.size()) * 2);
    QList<Word> pool = fetchPool(books, poolSize, excludeIds);

    // 创建题表
    Paper paper;
    paper.userId = userId;
    paper.mode = mode;
    paper.totalCount = selected.size();
    paper.title = title.isEmpty() ? QStringLiteral("%1 练习").arg(displayName(mode)) : title;
    paper.createdAt = QDateTime::currentDateTime();
    const qint64 paperId = m_papers.createPaper(paper);
    if (paperId < 0) return Result<Paper>::err(QStringLiteral("题表创建失败"));
    paper.id = paperId;

    const auto &registry = QuestionStrategyRegistry::instance();
    int seq = 0;
    QList<qint64> createdQuestions;
    for (const auto &w : selected) {
        QList<QuestionType> avail = registry.availableTypes(w, preferred);
        if (avail.isEmpty()) continue;
        const QuestionType type = avail.at(seq % avail.size());
        Question q = generateQuestion(type, w, pool, setting);
        if (!q.isValid()) continue;
        const qint64 qid = m_questions.insert(q);
        if (qid < 0) continue;
        createdQuestions << qid;
        PaperEntry e;
        e.paperId = paperId;
        e.questionId = qid;
        e.seq = seq;
        m_papers.addEntry(e);
        ++seq;
    }

    // 绝不留下长度为 0 的题表：生成失败则回滚已写入的题表与题目
    if (seq == 0) {
        m_papers.remove(paperId);
        for (auto qid : createdQuestions) m_questions.remove(qid);
        Logger::warn(QStringLiteral("Question"),
                     QStringLiteral("题目生成失败，已回滚空题表 #%1").arg(paperId));
        return Result<Paper>::err(QStringLiteral("题目生成失败（词条信息不足，无法生成任何题目）"));
    }
    paper.totalCount = seq;
    Stmt upd(Database::instance().db(), QStringLiteral("UPDATE papers SET total_count=? WHERE id=?"));
    upd.bind(1, seq).bind(2, paperId);
    upd.run();

    Logger::info(QStringLiteral("Question"),
                 QStringLiteral("生成题表 #%1 模式=%2 题数=%3").arg(paperId).arg(toKey(mode)).arg(seq));
    return Result<Paper>::ok(paper);
}

Result<QuestionService::AnswerResult> QuestionService::submitAnswer(qint64 entryId, const QString &userAnswer,
                                                                   qint64 timeMs) {
    const auto entryOpt = m_papers.findEntry(entryId);
    if (!entryOpt) return Result<AnswerResult>::err(QStringLiteral("作答记录不存在"));
    const PaperEntry &entry = *entryOpt;
    const auto paperOpt = m_papers.findPaper(entry.paperId);
    if (!paperOpt) return Result<AnswerResult>::err(QStringLiteral("题表不存在"));
    const qint64 userId = paperOpt->userId;
    const Setting setting = m_settings.load(userId);

    const Question &q = entry.question;
    const bool correct = rules::checkAnswer(q.type, userAnswer, q.answer, setting.checkSpellingStrict);
    if (!m_papers.updateEntryAnswer(entryId, userAnswer, correct, timeMs))
        return Result<AnswerResult>::err(QStringLiteral("作答保存失败"));

    // 单词本（收藏夹）条目的掌握度：更新所有包含该词的单词本
    if (q.wordId > 0) {
        for (const auto &we : m_books.findByWord(q.wordId)) {
            const int fam = rules::updateFamiliarity(we.familiarity, correct);
            m_books.updateEntryProgress(we.wordBookId, q.wordId, fam, we.reviewCount + 1,
                                        rules::computeNextReview(fam, correct));
        }
        // 背诵计划进度（新词/复习的掌握度与下次复习时间）
        for (const auto &pw : m_plans.planWordsForWord(userId, q.wordId)) {
            const int fam = rules::updateFamiliarity(pw.familiarity, correct);
            m_plans.recordAnswer(pw.planId, q.wordId, fam, pw.reviewCount + 1,
                                 rules::computeNextReview(fam, correct));
        }
    }

    // 错题本自动管理
    const qint64 wrongBookId = m_wrong.ensureBook(userId);
    if (wrongBookId > 0 && q.wordId > 0) {
        if (!correct) {
            if (setting.autoAddWrong)
                m_wrong.recordWrong(wrongBookId, q.wordId, q.type, userAnswer);
        } else {
            if (auto we = m_wrong.findEntry(wrongBookId, q.wordId)) {
                WrongEntry e = *we;
                e.correctCount += 1;
                e.lastReviewAt = QDateTime::currentDateTime();
                e.status = rules::computeStatus(e.wrongCount, e.correctCount, e.status);
                e.nextReviewAt = rules::computeNextReview(std::max(0, 5 - std::min(5, e.wrongCount - e.correctCount)), true);
                m_wrong.updateEntry(e);
            }
        }
    }

    AnswerResult r;
    r.correct = correct;
    r.correctAnswer = q.answer;
    r.explanation = q.explanation;
    return Result<AnswerResult>::ok(r);
}

Status QuestionService::finishPaper(qint64 paperId) {
    const auto entries = m_papers.listEntries(paperId);
    const PaperStat s = rules::summarize(entries);
    if (!m_papers.finishPaper(paperId, s.correct, s.total, s.score))
        return Status::err(QStringLiteral("交卷失败"));
    Logger::info(QStringLiteral("Question"),
                 QStringLiteral("交卷 #%1 得分=%2 正确=%3/%4").arg(paperId).arg(s.score).arg(s.correct).arg(s.total));
    return Status::ok();
}

QList<PaperEntry> QuestionService::entries(qint64 paperId) { return m_papers.listEntries(paperId); }

PaperStat QuestionService::stat(qint64 paperId) {
    return rules::summarize(m_papers.listEntries(paperId));
}

QList<Paper> QuestionService::history(qint64 userId, int limit) { return m_papers.listPapers(userId, limit); }

} // namespace vt
