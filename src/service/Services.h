#pragma once
//
// Services.h - 应用服务层（第二层：业务功能）
//
#include "core/Result.h"
#include "data/Repositories.h"
#include "domain/Entities.h"
#include "importer/JsonlImporter.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace vt {

// ===========================================================================
// 认证服务
// ===========================================================================
class AuthService {
public:
    AuthService(UserRepository &users, SettingRepository &settings,
                WordBookRepository &books, WrongBookRepository &wrong)
        : m_users(users), m_settings(settings), m_books(books), m_wrong(wrong) {}

    Status createAccount(const QString &username, const QString &password,
                         const QString &displayName = QString(), qint64 *userIdOut = nullptr);
    Result<User> login(const QString &username, const QString &password);
    QList<User> listAccounts();
    bool accountExists(const QString &username);

    // 修改/重置密码（需校验原密码）
    Status changePassword(qint64 userId, const QString &oldPassword, const QString &newPassword);

    // “记住登录状态”：基于用户口令散列派生的令牌，配置文件中不保存明文密码
    static QString rememberToken(const User &u);
    Result<User> loginWithRememberToken(qint64 userId, const QString &token);

    // 保证用户的默认数据存在
    void ensureUserScaffold(qint64 userId);
    qint64 defaultBookId(qint64 userId);

private:
    UserRepository &m_users;
    SettingRepository &m_settings;
    WordBookRepository &m_books;
    WrongBookRepository &m_wrong;
};

// ===========================================================================
// 设置服务
// ===========================================================================
class SettingService {
public:
    explicit SettingService(SettingRepository &repo) : m_repo(repo) {}
    Setting get(qint64 userId);
    Status update(const Setting &s);
    Status reset(qint64 userId);

    static QJsonObject toJson(const Setting &s);
    static Setting fromJson(qint64 userId, const QJsonObject &o, const Setting &fallback = Setting{});

private:
    SettingRepository &m_repo;
};

// ===========================================================================
// 单词服务
// ===========================================================================
class WordService {
public:
    WordService(WordRepository &repo, Database &db) : m_repo(repo), m_db(db) {}

    Result<Word> detail(qint64 id);
    Result<Word> findBySpelling(const QString &spelling, const QString &book = QString());
    QList<Word> search(const WordQuery &q, int *totalOut = nullptr);
    // 实时联想：返回最可能匹配的若干单词（前缀 > 包含 > 拼写相近 > 中文释义）
    QList<Word> suggest(const QString &keyword, const QString &book = QString(), int limit = 5);
    Status add(const Word &w, qint64 *idOut = nullptr);
    Status update(const Word &w);
    Status remove(qint64 id);
    qint64 count(const QString &book = QString());

    ImportStats importJsonl(const QString &file, bool replaceExisting = false,
                            const std::function<void(int, int)> &progress = {});
    ImportStats importDirectory(const QString &dir, bool replaceExisting = false);

private:
    WordRepository &m_repo;
    Database &m_db;
};

// ===========================================================================
// 单词本服务（可创建多个单词本/收藏夹，手动添加单词）
// ===========================================================================
class WordBookService {
public:
    WordBookService(WordBookRepository &repo, AuthService &auth, Database &db)
        : m_repo(repo), m_auth(auth), m_db(db) {}

    // ---- 单词本（收藏夹）管理 ----
    Result<WordBook> defaultBook(qint64 userId);
    qint64 ensureDefaultBook(qint64 userId);
    QList<WordBook> listBooks(qint64 userId);
    Result<WordBook> createBook(qint64 userId, const QString &name, const QString &desc = QString());
    Status renameBook(qint64 bookId, const QString &name, const QString &desc = QString());
    Status removeBook(qint64 bookId);

    // ---- 条目管理 ----
    Status addWord(qint64 bookId, qint64 wordId, EntrySource source = EntrySource::Manual);
    Status removeWord(qint64 bookId, qint64 wordId);
    Status removeEntryById(qint64 entryId);
    bool contains(qint64 bookId, qint64 wordId);
    // 该单词所在的所有单词本
    QList<WordBook> booksContaining(qint64 userId, qint64 wordId);

    QList<WordBookEntry> listEntries(qint64 bookId, const WordQuery &q, int *totalOut = nullptr);
    qint64 count(qint64 bookId);
    // 该用户所有单词本的条目总数
    qint64 totalEntries(qint64 userId);
    Status setFamiliarity(qint64 entryId, int familiarity);

    // 答题后更新所有包含该词的单词本条目的掌握度
    void applyAnswerForWord(qint64 wordId, bool correct);

    // 把词库中的全部单词批量加入默认单词本（bookFilter 为空表示全部词书）
    int addAllDictionaryWords(qint64 userId, const QString &bookFilter = QString());

private:
    WordBookRepository &m_repo;
    AuthService &m_auth;
    Database &m_db;
};

// ===========================================================================
// 错题服务
// ===========================================================================
class WrongBookService {
public:
    explicit WrongBookService(WrongBookRepository &repo) : m_repo(repo) {}

    Result<WrongBook> book(qint64 userId);
    Status recordWrong(qint64 userId, qint64 wordId, QuestionType type, const QString &myAnswer);
    void recordReview(qint64 userId, qint64 wordId, bool correct);

    QList<WrongEntry> list(qint64 userId, const WordQuery &q, int *totalOut = nullptr);
    Status update(const WrongEntry &e);
    Status remove(qint64 entryId);
    Status removeByWord(qint64 userId, qint64 wordId);
    Status markMastered(qint64 userId, qint64 wordId);
    qint64 count(qint64 userId);
    QList<qint64> dueWordIds(qint64 userId, int limit = 20);

private:
    WrongBookRepository &m_repo;
    qint64 bookId(qint64 userId) { return m_repo.ensureBook(userId); }
};

// ===========================================================================
// 题目 / 题表服务
// ===========================================================================
class QuestionService {
public:
    QuestionService(QuestionRepository &questions, PaperRepository &papers, WordRepository &words,
                    WordBookRepository &books, WrongBookRepository &wrong, SettingRepository &settings,
                    StudyPlanRepository &plans)
        : m_questions(questions), m_papers(papers), m_words(words), m_books(books),
          m_wrong(wrong), m_settings(settings), m_plans(plans) {}

    // 单题生成
    Question generateQuestion(QuestionType type, const Word &target, const QList<Word> &pool,
                              const Setting &setting) const;

    // 生成题表/试卷
    Result<Paper> generatePaper(qint64 userId, PaperMode mode, int count,
                                const QList<QuestionType> &types, const QString &title = QString());

    // 由指定的单词集合生成题表（每日词表使用）
    Result<Paper> generatePaperFromWords(qint64 userId, const QList<qint64> &wordIds,
                                         const QList<QuestionType> &types,
                                         const QString &title = QString());

    struct AnswerResult {
        bool correct = false;
        QString correctAnswer;
        QString explanation;
    };
    // 提作答（同时按设置自动维护错题本与单词本）
    Result<AnswerResult> submitAnswer(qint64 entryId, const QString &userAnswer, qint64 timeMs = 0);

    Status finishPaper(qint64 paperId);
    QList<PaperEntry> entries(qint64 paperId);
    PaperStat stat(qint64 paperId);
    QList<Paper> history(qint64 userId, int limit = 50);

private:
    QList<Word> fetchPool(const QList<BookType> &books, int n, const QList<qint64> &exclude) const;
    Result<Paper> buildPaper(qint64 userId, QList<Word> selected, PaperMode mode,
                             const QList<QuestionType> &preferred, const Setting &setting,
                             const QString &title);

    QuestionRepository &m_questions;
    PaperRepository &m_papers;
    WordRepository &m_words;
    WordBookRepository &m_books;
    WrongBookRepository &m_wrong;
    SettingRepository &m_settings;
    StudyPlanRepository &m_plans;
};

// ===========================================================================
// 每日词表服务（基于“背诵计划”：背诵天数 / 每天新词 / 每天复习）
// ===========================================================================
class DailyListService {
public:
    DailyListService(DailyListRepository &daily, StudyPlanRepository &plans, WordBookRepository &books,
                     WrongBookRepository &wrong, WordRepository &words, SettingRepository &settings)
        : m_daily(daily), m_plans(plans), m_books(books), m_wrong(wrong), m_words(words),
          m_settings(settings) {}

    // ---- 背诵计划 ----
    QList<StudyPlan> listPlans(qint64 userId);
    Result<StudyPlan> activePlan(qint64 userId);
    // 新建（id<=0）或更新计划；保存后自动设为当前计划
    Result<StudyPlan> savePlan(const StudyPlan &plan);
    Status setActivePlan(qint64 userId, qint64 planId);
    Status removePlan(qint64 userId, qint64 planId);

    // ---- 今日词表 ----
    // 依据计划生成今日词表（当天已生成则直接返回；force=true 重建）
    Result<DailyList> generateToday(qint64 userId, bool force = false);
    Result<DailyList> today(qint64 userId);
    QList<DailyList> recent(qint64 userId, int limit = 30);
    QList<DailyEntry> entries(qint64 listId);
    Status markEntryCompleted(qint64 entryId, bool completed = true);
    Status completeList(qint64 listId, bool completed = true);

    // ---- 计划进度 ----
    struct PlanProgress {
        int dayIndex = 1;
        int totalDays = 0;
        qint64 learned = 0;      // 计划中已学单词数
        qint64 inPlan = 0;       // 计划中已排入的单词数（含未作答）
        qint64 totalWords = 0;   // 词库总词数（按计划词书）
        int todayNew = 0;
        int todayReview = 0;
        // 覆盖全部词库所需的预计天数（按每天新词数），0 表示无法估算
        int neededDays = 0;
        // 剩余尚未排入计划的单词数
        qint64 remaining = 0;
        bool coversAll = true;   // 当前计划天数是否足以覆盖全部词库
    };
    PlanProgress progress(qint64 userId);

private:
    DailyListRepository &m_daily;
    StudyPlanRepository &m_plans;
    WordBookRepository &m_books;
    WrongBookRepository &m_wrong;
    WordRepository &m_words;
    SettingRepository &m_settings;
};

// ===========================================================================
// 字典服务
// ===========================================================================
class DictionaryService {
public:
    DictionaryService(WordService &words, WordBookService &books, WrongBookService &wrong)
        : m_words(words), m_books(books), m_wrong(wrong) {}

    QList<Word> search(const QString &keyword, const QString &book = QString(), int limit = 50);
    QList<Word> suggest(const QString &keyword, const QString &book = QString(), int limit = 5);
    Result<Word> detail(qint64 wordId);
    Status addToWordBook(qint64 bookId, qint64 wordId);

private:
    WordService &m_words;
    WordBookService &m_books;
    WrongBookService &m_wrong;
};

// ===========================================================================
// 同步服务
// ===========================================================================
class SyncService {
public:
    enum class ConflictPolicy { PreferLocal, PreferRemote, PreferNewer, Merge };

    struct ImportResult {
        int added = 0;
        int updated = 0;
        int conflicts = 0;
        QStringList details;
    };

    SyncService(WrongBookRepository &wrong, SettingRepository &settings, SyncLogRepository &logs,
                UserRepository &users)
        : m_wrong(wrong), m_settings(settings), m_logs(logs), m_users(users) {}

    Result<QString> exportTo(const QString &path, qint64 userId);
    Result<ImportResult> importFrom(const QString &path, qint64 userId,
                                    ConflictPolicy policy = ConflictPolicy::PreferNewer);
    QList<SyncLog> logs(qint64 userId, int limit = 100);
    Status clearLogs(qint64 userId);

private:
    WrongBookRepository &m_wrong;
    SettingRepository &m_settings;
    SyncLogRepository &m_logs;
    UserRepository &m_users;
};

} // namespace vt
