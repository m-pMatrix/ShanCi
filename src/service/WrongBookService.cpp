#include "core/Logger.h"
#include "domain/Rules.h"
#include "service/Services.h"

namespace vt {

Result<WrongBook> WrongBookService::book(qint64 userId) {
    const qint64 id = m_repo.ensureBook(userId);
    if (id < 0) return Result<WrongBook>::err(QStringLiteral("无法创建错题本"));
    const auto b = m_repo.findBook(id);
    if (!b) return Result<WrongBook>::err(QStringLiteral("错题本不存在"));
    return Result<WrongBook>::ok(*b);
}

Status WrongBookService::recordWrong(qint64 userId, qint64 wordId, QuestionType type,
                                     const QString &myAnswer) {
    const qint64 id = m_repo.recordWrong(bookId(userId), wordId, type, myAnswer);
    return id >= 0 ? Status::ok() : Status::err(QStringLiteral("加入错题本失败"));
}

void WrongBookService::recordReview(qint64 userId, qint64 wordId, bool correct) {
    const qint64 bid = bookId(userId);
    auto entry = m_repo.findEntry(bid, wordId);
    if (!entry) return;
    WrongEntry e = *entry;
    e.lastReviewAt = QDateTime::currentDateTime();
    if (correct) {
        ++e.correctCount;
    } else {
        ++e.wrongCount;
        e.myAnswer = QStringLiteral("(复习答错)");
    }
    e.priority = rules::computePriority(e.wrongCount);
    e.status = rules::computeStatus(e.wrongCount, e.correctCount, e.status);
    const int familiarity = std::max(0, e.wrongCount - e.correctCount);
    e.nextReviewAt = rules::computeNextReview(std::max(0, 5 - std::min(5, familiarity)), correct);
    m_repo.updateEntry(e);
}

QList<WrongEntry> WrongBookService::list(qint64 userId, const WordQuery &q, int *totalOut) {
    return m_repo.listEntries(bookId(userId), q, totalOut);
}

Status WrongBookService::update(const WrongEntry &e) {
    return m_repo.updateEntry(e) ? Status::ok() : Status::err(QStringLiteral("错题更新失败"));
}

Status WrongBookService::remove(qint64 entryId) {
    return m_repo.removeEntry(entryId) ? Status::ok() : Status::err(QStringLiteral("移除失败"));
}

Status WrongBookService::removeByWord(qint64 userId, qint64 wordId) {
    return m_repo.removeEntryByWord(bookId(userId), wordId) ? Status::ok()
                                                            : Status::err(QStringLiteral("移除失败"));
}

Status WrongBookService::markMastered(qint64 userId, qint64 wordId) {
    const qint64 bid = bookId(userId);
    auto entry = m_repo.findEntry(bid, wordId);
    if (!entry) return Status::err(QStringLiteral("错题不存在"));
    WrongEntry e = *entry;
    e.status = WrongStatus::Mastered;
    e.nextReviewAt = QDateTime();
    return m_repo.updateEntry(e) ? Status::ok() : Status::err(QStringLiteral("操作失败"));
}

qint64 WrongBookService::count(qint64 userId) { return m_repo.countEntries(bookId(userId)); }

QList<qint64> WrongBookService::dueWordIds(qint64 userId, int limit) {
    const auto entries = m_repo.dueForReview(bookId(userId), QDateTime::currentDateTime(), limit);
    QList<qint64> ids;
    for (const auto &e : entries) ids << e.wordId;
    return ids;
}

} // namespace vt
