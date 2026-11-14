#pragma once
//
// Repositories.h - Repository/DAO 汇总声明（第六层）
//
#include "data/Database.h"
#include "domain/Entities.h"

#include <QList>
#include <QString>
#include <optional>

namespace vt {

// ---------------------------------------------------------------------------
// 用户
// ---------------------------------------------------------------------------
class UserRepository {
public:
    explicit UserRepository(Database &db) : m_db(db) {}
    std::optional<User> findById(qint64 id);
    std::optional<User> findByUsername(const QString &username);
    QList<User> listAll();
    qint64 create(const User &u);          // 成功返回新 id, 失败返回 -1
    bool updateLastLogin(qint64 id);
    bool updatePassword(qint64 id, const QString &passwordHash, const QString &salt);
    bool exists(const QString &username);

private:
    Database &m_db;
};

// ---------------------------------------------------------------------------
// 个人设置
// ---------------------------------------------------------------------------
class SettingRepository {
public:
    explicit SettingRepository(Database &db) : m_db(db) {}
    Setting load(qint64 userId);
    bool save(const Setting &s);
    bool remove(qint64 userId);
    bool exists(qint64 userId);

private:
    Database &m_db;
};

// ---------------------------------------------------------------------------
// 单词
// ---------------------------------------------------------------------------
class WordRepository {
public:
    explicit WordRepository(Database &db) : m_db(db) {}

    qint64 insert(const Word &w);
    bool update(const Word &w);
    // 按 (book, spelling) 更新或插入，返回 id
    qint64 upsert(const Word &w);

    std::optional<Word> findById(qint64 id);
    std::optional<Word> findBySpelling(const QString &spelling, const QString &book = QString());

    QList<Word> search(const WordQuery &q, int *totalOut = nullptr);
    // 实时联想：最可能匹配的单词（前缀 > 包含 > 拼写相近 > 中文释义）
    QList<Word> suggest(const QString &keyword, const QString &book, int limit = 5);
    QList<Word> findByIds(const QList<qint64> &ids);
    QList<Word> randomByBook(const QString &book, int n, const QList<qint64> &excludeIds = {});
    QList<Word> findSimilar(const QString &spelling, const QString &book, int n);
    QList<Word> findFormSimilar(const QString &spelling, int n);

    qint64 countAll(const QString &book = QString());
    bool remove(qint64 id);
    bool removeByBook(const QString &book);

    Database &database() { return m_db; }

private:
    Database &m_db;
};

// ---------------------------------------------------------------------------
// 单词本
// ---------------------------------------------------------------------------
class WordBookRepository {
public:
    explicit WordBookRepository(Database &db) : m_db(db) {}

    qint64 createBook(const WordBook &b);
    std::optional<WordBook> findBook(qint64 id);
    std::optional<WordBook> findDefaultBook(qint64 userId);
    QList<WordBook> listBooks(qint64 userId);
    bool renameBook(qint64 id, const QString &name, const QString &desc);
    bool deleteBook(qint64 id);

    bool addEntry(qint64 bookId, qint64 wordId, EntrySource source, bool ignoreIfExists = true);
    // 批量加入：把 words 表中（可选按词书过滤）的全部单词加入单词本，返回新增条数
    int addAllWords(qint64 bookId, const QString &bookFilter, EntrySource source);
    bool removeEntry(qint64 bookId, qint64 wordId);
    bool removeEntryById(qint64 entryId);
    bool hasEntry(qint64 bookId, qint64 wordId);
    bool updateEntryProgress(qint64 bookId, qint64 wordId, int familiarity, int reviewCount,
                             const QDateTime &nextReview);
    bool updateEntryFamiliarity(qint64 entryId, int familiarity);

    QList<WordBookEntry> listEntries(qint64 bookId, const WordQuery &q, int *totalOut = nullptr);
    std::optional<WordBookEntry> findEntry(qint64 bookId, qint64 wordId);
    // 某个单词在用户所有单词本中的条目
    QList<WordBookEntry> findByWord(qint64 wordId);
    qint64 countEntries(qint64 bookId);
    // 用户所有单词本的条目总数
    qint64 countAllEntries(qint64 userId);
    QList<qint64> wordIdsOfBook(qint64 bookId);
    QList<WordBookEntry> dueForReview(qint64 bookId, const QDateTime &before, int limit);

private:
    Database &m_db;
};

// ---------------------------------------------------------------------------
// 错题本
// ---------------------------------------------------------------------------
class WrongBookRepository {
public:
    explicit WrongBookRepository(Database &db) : m_db(db) {}

    qint64 ensureBook(qint64 userId); // 每个用户一个默认错题本
    std::optional<WrongBook> findBook(qint64 id);
    std::optional<WrongBook> findBookByUser(qint64 userId);

    // 记录答错：存在则累加，否则插入
    qint64 recordWrong(qint64 bookId, qint64 wordId, QuestionType type, const QString &myAnswer,
                       const QString &remark = QString());

    bool updateEntry(const WrongEntry &e);
    // 以 (wrong_book_id, word_id) 为键插入或整体覆盖（同步导入使用）
    qint64 upsertEntry(const WrongEntry &e);
    bool removeEntry(qint64 entryId);
    bool removeEntryByWord(qint64 bookId, qint64 wordId);
    std::optional<WrongEntry> findEntry(qint64 bookId, qint64 wordId);

    QList<WrongEntry> listEntries(qint64 bookId, const WordQuery &q, int *totalOut = nullptr);
    qint64 countEntries(qint64 bookId);
    QList<qint64> wordIdsOfBook(qint64 bookId);
    QList<WrongEntry> dueForReview(qint64 bookId, const QDateTime &before, int limit);

private:
    Database &m_db;
};

// ---------------------------------------------------------------------------
// 题目
// ---------------------------------------------------------------------------
class QuestionRepository {
public:
    explicit QuestionRepository(Database &db) : m_db(db) {}
    qint64 insert(const Question &q);
    std::optional<Question> findById(qint64 id);
    QList<Question> findByIds(const QList<qint64> &ids);
    bool remove(qint64 id);

private:
    Database &m_db;
};

// ---------------------------------------------------------------------------
// 题表（试卷）/ 作答明细
// ---------------------------------------------------------------------------
class PaperRepository {
public:
    explicit PaperRepository(Database &db) : m_db(db) {}

    qint64 createPaper(const Paper &p);
    bool finishPaper(qint64 paperId, int correct, int total, double score,
                     const QDateTime &finishedAt = QDateTime::currentDateTime());
    std::optional<Paper> findPaper(qint64 id);
    QList<Paper> listPapers(qint64 userId, int limit = 50);
    bool remove(qint64 paperId);

    qint64 addEntry(const PaperEntry &e);
    bool updateEntryAnswer(qint64 entryId, const QString &userAnswer, bool isCorrect, qint64 timeMs);
    std::optional<PaperEntry> findEntry(qint64 entryId);
    QList<PaperEntry> listEntries(qint64 paperId);

private:
    Database &m_db;
};

// ---------------------------------------------------------------------------
// 每日词表
// ---------------------------------------------------------------------------
class DailyListRepository {
public:
    explicit DailyListRepository(Database &db) : m_db(db) {}

    std::optional<DailyList> findByDate(qint64 userId, const QString &date);
    std::optional<DailyList> findById(qint64 id);
    QList<DailyList> listRecent(qint64 userId, int limit = 30);

    // 删除某天的词表（重新生成时使用）
    bool removeByDate(qint64 userId, const QString &date);

    qint64 createList(const DailyList &list);
    qint64 addEntry(const DailyEntry &e);
    bool setEntryCompleted(qint64 entryId, bool completed);
    bool updateProgress(qint64 listId, int doneCount, bool completed,
                        const QDateTime &completedAt = QDateTime());
    QList<DailyEntry> listEntries(qint64 listId);

private:
    Database &m_db;
};

// ---------------------------------------------------------------------------
// 同步日志
// ---------------------------------------------------------------------------
class SyncLogRepository {
public:
    explicit SyncLogRepository(Database &db) : m_db(db) {}
    qint64 add(const SyncLog &log);
    QList<SyncLog> listRecent(qint64 userId, int limit = 100);
    bool clear(qint64 userId);

private:
    Database &m_db;
};

// ---------------------------------------------------------------------------
// 背诵计划（每日词表）
// ---------------------------------------------------------------------------
class StudyPlanRepository {
public:
    explicit StudyPlanRepository(Database &db) : m_db(db) {}

    qint64 save(const StudyPlan &p);                       // 新增或更新（按 id）
    std::optional<StudyPlan> findById(qint64 id);
    std::optional<StudyPlan> activePlan(qint64 userId);     // 当前启用的计划
    QList<StudyPlan> listPlans(qint64 userId);
    bool remove(qint64 id);
    bool setActive(qint64 userId, qint64 planId);

    // 尚未学习的新词（按词库顺序），返回 word_id
    QList<qint64> nextNewWordIds(qint64 planId, const QString &book, int limit);
    // 已学习且到期的复习词
    QList<qint64> dueReviewWordIds(qint64 planId, const QDateTime &before, int limit);
    // 把新词纳入计划
    bool addPlanWords(qint64 planId, const QList<qint64> &wordIds, const QString &date);
    // 记录答题结果，更新计划内该词的掌握度与下次复习时间
    bool recordAnswer(qint64 planId, qint64 wordId, int familiarity, int reviewCount,
                      const QDateTime &nextReview);
    // 某单词在用户启用计划中的进度记录
    QList<PlanWord> planWordsForWord(qint64 userId, qint64 wordId);

    qint64 countLearned(qint64 planId);
    qint64 countInPlan(qint64 planId);

private:
    Database &m_db;
};

} // namespace vt
