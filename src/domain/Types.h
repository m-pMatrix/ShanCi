#pragma once
//
// Types.h - 领域层枚举与枚举/字符串互转
//
#include <QString>
#include <QStringList>

namespace vt {

// 词表/词书来源
enum class BookType { CET4, CET6, Custom };

// 题型策略（第四层：题目生成策略）
enum class QuestionType {
    EnToCn,       // 英文选中文释义
    CnToEn,       // 中文释义拼写英文
    SimilarMatch, // 形近词/相似词匹配词义
    Cloze         // 选词填空
};

// 每日词表 / 题表条目的来源
enum class EntrySource {
    WordBook,   // 来自单词本（收藏夹）
    WrongBook,  // 来自错题本
    Manual,     // 手动加入
    New,        // 背诵计划：新词
    Review      // 背诵计划：复习
};

// 题表/试卷生成模式
enum class PaperMode {
    Random,       // 随机抽取
    WordBookOnly, // 仅从单词表选取
    WrongOnly,    // 仅从错题本选取
    Mixed,        // 混合错题本与单词表
    Plan          // 每日背诵计划
};

// 错题状态
enum class WrongStatus { New, Reviewing, Mastered };

// 错题优先级
enum class WrongPriority { Low, Normal, High };

// 同步方向
enum class SyncDirection { Export, Import };

// 同步状态
enum class SyncStatus { Success, Partial, Failed };

// 复习任务类型（结果页/错题本）
enum class ReviewAction { Keep, Mastered, Remove };

// ---------------------------------------------------------------------------
// 枚举 <-> 字符串
// ---------------------------------------------------------------------------
QString toKey(BookType v);
QString toKey(QuestionType v);
QString toKey(EntrySource v);
QString toKey(PaperMode v);
QString toKey(WrongStatus v);
QString toKey(WrongPriority v);
QString toKey(SyncDirection v);
QString toKey(SyncStatus v);

BookType bookTypeFromKey(const QString &k, BookType def = BookType::CET4);
QuestionType questionTypeFromKey(const QString &k, QuestionType def = QuestionType::EnToCn);
EntrySource entrySourceFromKey(const QString &k, EntrySource def = EntrySource::WordBook);
PaperMode paperModeFromKey(const QString &k, PaperMode def = PaperMode::Mixed);
WrongStatus wrongStatusFromKey(const QString &k, WrongStatus def = WrongStatus::New);
WrongPriority wrongPriorityFromKey(const QString &k, WrongPriority def = WrongPriority::Normal);
SyncDirection syncDirectionFromKey(const QString &k, SyncDirection def = SyncDirection::Export);
SyncStatus syncStatusFromKey(const QString &k, SyncStatus def = SyncStatus::Success);

// 中文显示名（用于 UI）
QString displayName(QuestionType v);
QString displayName(PaperMode v);
QString displayName(EntrySource v);
QString displayName(WrongStatus v);
QString displayName(SyncDirection v);
QString displayName(BookType v);

// "EnToCn,CnToEn" -> 列表
QStringList questionTypesToKeys(const QList<QuestionType> &types);
QList<QuestionType> questionTypesFromKeys(const QStringList &keys);

} // namespace vt
