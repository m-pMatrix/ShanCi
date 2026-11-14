#pragma once
//
// Rules.h - 领域核心规则（第三层：核心规则）
//   离线账户、答案校验、复习安排、错题自动管理、评分等纯逻辑
//
#include "domain/Entities.h"

#include <QString>

namespace vt {
namespace rules {

// ---- 答案校验（第四层：答案校验） ----
// 统一入口：按题型与严格程度判定用户答案是否正确
bool checkAnswer(QuestionType type, const QString &userAnswer,
                 const QString &correctAnswer, bool strictSpelling = true);

// 拼写题的宽松比较：忽略大小写/首尾空白，允许一个字符误差
bool checkSpelling(const QString &userAnswer, const QString &correctAnswer, bool strict);

// ---- 评分（第四层：评分） ----
double scoreOf(int correct, int total); // 百分制，四舍五入到 1 位小数
PaperStat summarize(const QList<PaperEntry> &entries);

// ---- 复习安排（第三层：复习安排） ----
// 艾宾浩斯式间隔：根据熟悉度(0..5)与本次是否正确，返回下次复习间隔天数
int nextReviewIntervalDays(int familiarity, bool correct);
QDateTime computeNextReview(int familiarity, bool correct, const QDateTime &from = QDateTime::currentDateTime());

// 根据答题结果更新单词本条目熟悉度（0..5）
int updateFamiliarity(int current, bool correct);

// ---- 错题自动管理（第三层：错题本自动管理） ----
// 优先级：错误次数越多越高
WrongPriority computePriority(int wrongCount);
// 状态：连续答对达到阈值视为已掌握
WrongStatus computeStatus(int wrongCount, int correctCount, WrongStatus current);
constexpr int kMasteryCorrectThreshold = 3;

// ---- 离线账户（第三层：离线账户） ----
bool isUsernameValid(const QString &username, QString *reason = nullptr);
bool isPasswordValid(const QString &password, QString *reason = nullptr);

QString hashPassword(const QString &password, const QString &salt);

} // namespace rules
} // namespace vt
