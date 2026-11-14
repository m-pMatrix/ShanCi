#pragma once
//
// Entities.h - 领域层实体（第三层）
//
#include "domain/Types.h"

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace vt {

// 音标/释义条目（词性 + 中文 + 英文）
struct Translation {
    QString pos;        // 词性 v./n./adj....
    QString tranCn;     // 中文释义
    QString tranEn;     // 英文释义
};

struct Phrase {
    QString content;    // 短语
    QString meaning;    // 中文
};

struct Sentence {
    QString en;         // 英文例句
    QString cn;         // 中文翻译
    QString audio;      // 语音
    QString tag;        // "例句"/"真题例句"
};

struct Synonym {
    QString pos;
    QString tran;
    QStringList words;
};

struct RelWord {
    QString pos;
    QList<QPair<QString, QString>> words; // hwd, tran
};

struct ExamItem {
    QString question;
    QStringList choices;
    int rightIndex = 0;
    QString explain;
};

// ---------------------------------------------------------------------------
// 用户 / 个人设置
// ---------------------------------------------------------------------------
struct User {
    qint64 id = 0;
    QString username;
    QString passwordHash;
    QString salt;
    QString displayName;
    QDateTime createdAt;
    QDateTime lastLoginAt;

    bool isValid() const { return id > 0; }
};

struct Setting {
    qint64 userId = 0;
    int dailyWordCount = 20;
    QList<QuestionType> questionTypes{QuestionType::EnToCn, QuestionType::CnToEn,
                                      QuestionType::SimilarMatch, QuestionType::Cloze};
    QString theme = QStringLiteral("light");
    QString syncPath;
    int fontSize = 13;
    bool audioEnabled = true;
    bool showPhonetic = true;
    bool autoAddWrong = true;
    bool checkSpellingStrict = true;
    QDateTime updatedAt;
};

// ---------------------------------------------------------------------------
// 单词
// ---------------------------------------------------------------------------
struct Word {
    qint64 id = 0;
    BookType book = BookType::CET4;
    int wordRank = 0;
    QString spelling;
    QString phoneticUk;
    QString phoneticUs;
    QString audioUk;
    QString audioUs;
    int star = 0;
    QList<Translation> translations;
    QList<Phrase> phrases;
    QList<Sentence> sentences;
    QList<Synonym> synonyms;
    QList<RelWord> relWords;
    QList<ExamItem> exams;
    QString memMethod;   // 记忆方法
    QString source;      // 来源（真题年份等）
    QDateTime createdAt;

    bool isValid() const { return id > 0 && !spelling.isEmpty(); }

    QString primaryMeaning() const {
        if (!translations.isEmpty() && !translations.first().tranCn.isEmpty())
            return translations.first().tranCn;
        return QString();
    }
    QString posOfFirst() const {
        return translations.isEmpty() ? QString() : translations.first().pos;
    }
    QString meaningWithPos() const {
        if (translations.isEmpty()) return QString();
        const auto &t = translations.first();
        return t.pos.isEmpty() ? t.tranCn : QStringLiteral("%1. %2").arg(t.pos, t.tranCn);
    }
    // 合并所有中文释义，用于搜索
    QString allChinese() const {
        QStringList parts;
        for (const auto &t : translations) if (!t.tranCn.isEmpty()) parts << t.tranCn;
        return parts.join(QStringLiteral("；"));
    }
};

// ---------------------------------------------------------------------------
// 单词本 / 单词本条目
// ---------------------------------------------------------------------------
struct WordBook {
    qint64 id = 0;
    qint64 userId = 0;
    QString name;
    QString description;
    QDateTime createdAt;
    QDateTime updatedAt;
    int entryCount = 0; // 冗余统计（查询时填充）
};

struct WordBookEntry {
    qint64 id = 0;
    qint64 wordBookId = 0;
    qint64 wordId = 0;
    int familiarity = 0;      // 熟悉度 0..5
    int reviewCount = 0;      // 复习次数
    QDateTime nextReviewAt;   // 下次复习时间
    QDateTime addedAt;
    EntrySource source = EntrySource::Manual;

    Word word; // join 填充
};

// ---------------------------------------------------------------------------
// 错题本 / 错题条目
// ---------------------------------------------------------------------------
struct WrongBook {
    qint64 id = 0;
    qint64 userId = 0;
    QString name;
    QDateTime createdAt;
    int entryCount = 0;
};

struct WrongEntry {
    qint64 id = 0;
    qint64 wrongBookId = 0;
    qint64 wordId = 0;
    QuestionType questionType = QuestionType::EnToCn;
    int wrongCount = 0;
    int correctCount = 0;
    QDateTime lastWrongAt;
    QDateTime lastReviewAt;
    QDateTime nextReviewAt;
    WrongPriority priority = WrongPriority::Normal;
    WrongStatus status = WrongStatus::New;
    QString myAnswer;
    QString remark;
    QDateTime updatedAt;

    Word word; // join 填充
};

// ---------------------------------------------------------------------------
// 题目 / 题表（试卷）/ 题表条目 / 作答明细
// ---------------------------------------------------------------------------
struct Question {
    qint64 id = 0;
    QuestionType type = QuestionType::EnToCn;
    QString stem;             // 题干
    QStringList options;      // 选项（拼写题为空）
    QString answer;           // 正确答案
    QString explanation;      // 解析
    int difficulty = 1;
    qint64 wordId = 0;
    QDateTime createdAt;

    bool isValid() const { return !stem.isEmpty() && !answer.isEmpty(); }
};

struct PaperEntry {
    qint64 id = 0;
    qint64 paperId = 0;
    qint64 questionId = 0;
    int seq = 0;
    QString userAnswer;
    bool answered = false;
    bool isCorrect = false;
    qint64 answerTimeMs = 0;
    QDateTime answeredAt;

    Question question; // join 填充
};

struct Paper {
    qint64 id = 0;
    qint64 userId = 0;
    QString title;
    PaperMode mode = PaperMode::Mixed;
    int totalCount = 0;
    int correctCount = 0;
    double score = 0.0;
    QDateTime createdAt;
    QDateTime finishedAt;

    bool finished() const { return finishedAt.isValid(); }
};

// ---------------------------------------------------------------------------
// 每日词表 / 每日词表条目
// ---------------------------------------------------------------------------
struct DailyEntry {
    qint64 id = 0;
    qint64 dailyListId = 0;
    qint64 wordId = 0;
    EntrySource sourceType = EntrySource::WordBook;
    int seq = 0;
    bool isCompleted = false;
    Word word; // join 填充
};

struct DailyList {
    qint64 id = 0;
    qint64 userId = 0;
    QString listDate;   // yyyy-MM-dd
    PaperMode mode = PaperMode::Mixed;
    int totalCount = 0;
    int doneCount = 0;
    QDateTime createdAt;
    QDateTime completedAt;

    bool completed() const { return completedAt.isValid(); }
};

// ---------------------------------------------------------------------------
// 同步日志
// ---------------------------------------------------------------------------
struct SyncLog {
    qint64 id = 0;
    qint64 userId = 0;
    SyncDirection direction = SyncDirection::Export;
    QString filePath;
    int itemCount = 0;
    int conflictCount = 0;
    SyncStatus status = SyncStatus::Success;
    QString message;
    QDateTime createdAt;
};

// ---------------------------------------------------------------------------
// 背诵计划（每日词表的配置）
// ---------------------------------------------------------------------------
struct StudyPlan {
    qint64 id = 0;
    qint64 userId = 0;
    QString name = QStringLiteral("我的背诵计划");
    QString book;              // 空 = 全部词书；CET4 / CET6
    int totalDays = 30;        // 背诵天数
    int newPerDay = 20;        // 每天背多少新词
    int reviewPerDay = 20;     // 每天复习多少词
    bool includeWrong = true;  // 复习时是否包含错题本
    QString startDate;         // yyyy-MM-dd
    QDateTime createdAt;
    bool active = true;

    bool isValid() const { return id > 0; }

    // 今天是计划的第几天（1 起算）
    int dayIndex(const QString &today) const {
        const QDate s = QDate::fromString(startDate, QStringLiteral("yyyy-MM-dd"));
        const QDate t = QDate::fromString(today, QStringLiteral("yyyy-MM-dd"));
        if (!s.isValid() || !t.isValid()) return 1;
        return static_cast<int>(s.daysTo(t)) + 1;
    }
};

// ---------------------------------------------------------------------------
// 背诵计划（每日词表）
// ---------------------------------------------------------------------------
struct PlanWord {
    qint64 id = 0;
    qint64 planId = 0;
    qint64 wordId = 0;
    QString firstDate;
    bool learned = false;
    int familiarity = 0;
    int reviewCount = 0;
    QDateTime nextReviewAt;
    QDateTime lastReviewAt;
};

// ---------------------------------------------------------------------------
// 查询辅助
// ---------------------------------------------------------------------------
struct WordQuery {
    QString keyword;                 // 模糊搜索：拼写/中文
    QString book;                    // 空=全部
    int minStar = -1;                // -1 表示不限
    int familiarityMin = -1;
    QString wrongStatus;             // 空=全部
    int limit = 200;
    int offset = 0;
    QString orderBy = QStringLiteral("word_rank");
};

struct PaperStat {
    int total = 0;
    int correct = 0;
    int wrong = 0;
    double score = 0.0;
    qint64 totalTimeMs = 0;
};

} // namespace vt
