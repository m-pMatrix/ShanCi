#include "core/Logger.h"
#include "domain/Rules.h"
#include "service/Services.h"

namespace vt {

// ---------------------------------------------------------------------------
// WordService
// ---------------------------------------------------------------------------
Result<Word> WordService::detail(qint64 id) {
    const auto w = m_repo.findById(id);
    if (!w) return Result<Word>::err(QStringLiteral("单词不存在"));
    return Result<Word>::ok(*w);
}

Result<Word> WordService::findBySpelling(const QString &spelling, const QString &book) {
    const auto w = m_repo.findBySpelling(spelling, book);
    if (!w) return Result<Word>::err(QStringLiteral("未找到单词：%1").arg(spelling));
    return Result<Word>::ok(*w);
}

QList<Word> WordService::search(const WordQuery &q, int *totalOut) { return m_repo.search(q, totalOut); }

QList<Word> WordService::suggest(const QString &keyword, const QString &book, int limit) {
    return m_repo.suggest(keyword, book, limit);
}

Status WordService::add(const Word &w, qint64 *idOut) {
    if (w.spelling.trimmed().isEmpty()) return Status::err(QStringLiteral("单词拼写不能为空"));
    Word copy = w;
    const qint64 id = m_repo.upsert(copy);
    if (id < 0) return Status::err(QStringLiteral("单词保存失败"));
    if (idOut) *idOut = id;
    return Status::ok();
}

Status WordService::update(const Word &w) {
    if (w.id <= 0) return Status::err(QStringLiteral("无效的单词 ID"));
    if (w.spelling.trimmed().isEmpty()) return Status::err(QStringLiteral("单词拼写不能为空"));
    return m_repo.update(w) ? Status::ok() : Status::err(QStringLiteral("单词更新失败"));
}

Status WordService::remove(qint64 id) {
    return m_repo.remove(id) ? Status::ok() : Status::err(QStringLiteral("删除失败"));
}

qint64 WordService::count(const QString &book) { return m_repo.countAll(book); }

ImportStats WordService::importJsonl(const QString &file, bool replaceExisting,
                                     const std::function<void(int, int)> &progress) {
    JsonlImporter importer(m_repo);
    return importer.importFile(file, replaceExisting, progress);
}

ImportStats WordService::importDirectory(const QString &dir, bool replaceExisting) {
    JsonlImporter importer(m_repo);
    return importer.importDirectory(dir, replaceExisting);
}

// ---------------------------------------------------------------------------
// WordBookService（多单词本/收藏夹）
// ---------------------------------------------------------------------------
Result<WordBook> WordBookService::defaultBook(qint64 userId) {
    const auto b = m_repo.findDefaultBook(userId);
    if (!b) return Result<WordBook>::err(QStringLiteral("单词本不存在"));
    return Result<WordBook>::ok(*b);
}

qint64 WordBookService::ensureDefaultBook(qint64 userId) { return m_auth.defaultBookId(userId); }

QList<WordBook> WordBookService::listBooks(qint64 userId) { return m_repo.listBooks(userId); }

Result<WordBook> WordBookService::createBook(qint64 userId, const QString &name, const QString &desc) {
    const QString n = name.trimmed();
    if (n.isEmpty()) return Result<WordBook>::err(QStringLiteral("单词本名称不能为空"));
    for (const auto &b : m_repo.listBooks(userId)) {
        if (b.name.compare(n, Qt::CaseInsensitive) == 0)
            return Result<WordBook>::err(QStringLiteral("已存在同名单词本：%1").arg(n));
    }
    WordBook b;
    b.userId = userId;
    b.name = n;
    b.description = desc;
    const qint64 id = m_repo.createBook(b);
    if (id < 0) return Result<WordBook>::err(QStringLiteral("创建单词本失败"));
    b.id = id;
    Logger::info(QStringLiteral("WordBook"), QStringLiteral("创建单词本: %1").arg(n));
    return Result<WordBook>::ok(b);
}

Status WordBookService::renameBook(qint64 bookId, const QString &name, const QString &desc) {
    if (bookId <= 0) return Status::err(QStringLiteral("无效的单词本"));
    const QString n = name.trimmed();
    if (n.isEmpty()) return Status::err(QStringLiteral("单词本名称不能为空"));
    return m_repo.renameBook(bookId, n, desc) ? Status::ok() : Status::err(QStringLiteral("重命名失败"));
}

Status WordBookService::removeBook(qint64 bookId) {
    if (bookId <= 0) return Status::err(QStringLiteral("无效的单词本"));
    return m_repo.deleteBook(bookId) ? Status::ok() : Status::err(QStringLiteral("删除单词本失败"));
}

Status WordBookService::addWord(qint64 bookId, qint64 wordId, EntrySource source) {
    if (bookId <= 0) return Status::err(QStringLiteral("无效的单词本"));
    if (m_repo.hasEntry(bookId, wordId)) return Status::err(QStringLiteral("该单词已在此单词本中"));
    return m_repo.addEntry(bookId, wordId, source, true) ? Status::ok()
                                                         : Status::err(QStringLiteral("加入单词本失败"));
}

Status WordBookService::removeWord(qint64 bookId, qint64 wordId) {
    return m_repo.removeEntry(bookId, wordId) ? Status::ok() : Status::err(QStringLiteral("移出单词本失败"));
}

Status WordBookService::removeEntryById(qint64 entryId) {
    return m_repo.removeEntryById(entryId) ? Status::ok() : Status::err(QStringLiteral("删除失败"));
}

bool WordBookService::contains(qint64 bookId, qint64 wordId) { return m_repo.hasEntry(bookId, wordId); }

QList<WordBook> WordBookService::booksContaining(qint64 userId, qint64 wordId) {
    QList<WordBook> out;
    for (const auto &b : m_repo.listBooks(userId)) {
        if (m_repo.hasEntry(b.id, wordId)) out << b;
    }
    return out;
}

QList<WordBookEntry> WordBookService::listEntries(qint64 bookId, const WordQuery &q, int *totalOut) {
    return m_repo.listEntries(bookId, q, totalOut);
}

qint64 WordBookService::count(qint64 bookId) { return m_repo.countEntries(bookId); }

qint64 WordBookService::totalEntries(qint64 userId) { return m_repo.countAllEntries(userId); }

Status WordBookService::setFamiliarity(qint64 entryId, int familiarity) {
    return m_repo.updateEntryFamiliarity(entryId, familiarity) ? Status::ok()
                                                              : Status::err(QStringLiteral("更新失败"));
}

void WordBookService::applyAnswerForWord(qint64 wordId, bool correct) {
    const auto entries = m_repo.findByWord(wordId);
    for (const auto &e : entries) {
        const int fam = rules::updateFamiliarity(e.familiarity, correct);
        const QDateTime next = rules::computeNextReview(fam, correct);
        m_repo.updateEntryProgress(e.wordBookId, wordId, fam, e.reviewCount + 1, next);
    }
}

int WordBookService::addAllDictionaryWords(qint64 userId, const QString &bookFilter) {
    const qint64 bookId = m_auth.defaultBookId(userId);
    if (bookId <= 0) return 0;
    const int added = m_repo.addAllWords(bookId, bookFilter, EntrySource::Manual);
    Logger::info(QStringLiteral("WordBook"),
                 QStringLiteral("批量加入单词本：新增 %1 条（词书过滤=%2）")
                     .arg(added)
                     .arg(bookFilter.isEmpty() ? QStringLiteral("全部") : bookFilter));
    return added;
}

// ---------------------------------------------------------------------------
// DictionaryService
// ---------------------------------------------------------------------------
QList<Word> DictionaryService::search(const QString &keyword, const QString &book, int limit) {
    WordQuery q;
    q.keyword = keyword;
    q.book = book;
    q.limit = limit;
    return m_words.search(q);
}

QList<Word> DictionaryService::suggest(const QString &keyword, const QString &book, int limit) {
    return m_words.suggest(keyword, book, limit);
}

Result<Word> DictionaryService::detail(qint64 wordId) { return m_words.detail(wordId); }

Status DictionaryService::addToWordBook(qint64 bookId, qint64 wordId) {
    return m_books.addWord(bookId, wordId, EntrySource::Manual);
}

} // namespace vt
