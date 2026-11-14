#include "domain/Rules.h"

#include "core/Utils.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace vt {
namespace rules {

bool checkSpelling(const QString &userAnswer, const QString &correctAnswer, bool strict) {
    const QString u = util::normalizeSpelling(userAnswer);
    const QString c = util::normalizeSpelling(correctAnswer);
    if (u.isEmpty()) return false;
    if (u == c) return true;
    if (strict) return false;
    // 宽松模式：允许 1 个字符以内的编辑距离（长度>4 时允许 1，更长允许 2）
    const int allowed = c.size() > 8 ? 2 : (c.size() > 4 ? 1 : 0);
    return util::levenshtein(u, c) <= allowed;
}

bool checkAnswer(QuestionType type, const QString &userAnswer,
                 const QString &correctAnswer, bool strictSpelling) {
    switch (type) {
    case QuestionType::CnToEn:
        return checkSpelling(userAnswer, correctAnswer, strictSpelling);
    case QuestionType::EnToCn:
    case QuestionType::SimilarMatch:
    case QuestionType::Cloze:
    default: {
        // 选择题：先按完整文本比较，其次按选项字母/序号
        const QString u = userAnswer.trimmed();
        const QString c = correctAnswer.trimmed();
        if (u.isEmpty()) return false;
        if (u.compare(c, Qt::CaseInsensitive) == 0) return true;
        // 支持 "A"/"1" 形式
        if (u.size() == 1 && c.size() == 1) return u.compare(c, Qt::CaseInsensitive) == 0;
        return false;
    }
    }
}

double scoreOf(int correct, int total) {
    if (total <= 0) return 0.0;
    return std::round(static_cast<double>(correct) / total * 1000.0) / 10.0;
}

PaperStat summarize(const QList<PaperEntry> &entries) {
    PaperStat stat;
    stat.total = entries.size();
    for (const auto &e : entries) {
        if (!e.answered) continue;
        stat.totalTimeMs += e.answerTimeMs;
        if (e.isCorrect) ++stat.correct;
        else ++stat.wrong;
    }
    stat.score = scoreOf(stat.correct, stat.total);
    return stat;
}

int nextReviewIntervalDays(int familiarity, bool correct) {
    // 基础间隔表（天）：按熟悉度递增
    static const int base[] = {0, 1, 2, 4, 7, 15, 30};
    const int idx = std::clamp(familiarity, 0, 6);
    int days = base[idx];
    if (!correct) days = std::max(1, days / 2);
    return days;
}

QDateTime computeNextReview(int familiarity, bool correct, const QDateTime &from) {
    const QDateTime base = from.isValid() ? from : QDateTime::currentDateTime();
    return base.addDays(nextReviewIntervalDays(familiarity, correct));
}

int updateFamiliarity(int current, bool correct) {
    if (correct) return std::min(5, current + 1);
    return std::max(0, current - 1);
}

WrongPriority computePriority(int wrongCount) {
    if (wrongCount >= 3) return WrongPriority::High;
    if (wrongCount >= 2) return WrongPriority::Normal;
    return WrongPriority::Low;
}

WrongStatus computeStatus(int wrongCount, int correctCount, WrongStatus current) {
    if (correctCount >= kMasteryCorrectThreshold && correctCount > wrongCount)
        return WrongStatus::Mastered;
    if (current == WrongStatus::Mastered)
        return WrongStatus::Mastered;
    if (wrongCount > 0 && correctCount > 0)
        return WrongStatus::Reviewing;
    if (wrongCount > 0)
        return WrongStatus::New;
    return current;
}

bool isUsernameValid(const QString &username, QString *reason) {
    const QString u = username.trimmed();
    if (u.size() < 2) {
        if (reason) *reason = QStringLiteral("用户名至少 2 个字符");
        return false;
    }
    if (u.size() > 32) {
        if (reason) *reason = QStringLiteral("用户名过长");
        return false;
    }
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_\\x{4e00}-\\x{9fa5}]+$"));
    if (!re.match(u).hasMatch()) {
        if (reason) *reason = QStringLiteral("用户名只能包含字母、数字、下划线或中文");
        return false;
    }
    return true;
}

bool isPasswordValid(const QString &password, QString *reason) {
    if (password.size() < 4) {
        if (reason) *reason = QStringLiteral("密码至少 4 个字符");
        return false;
    }
    return true;
}

QString hashPassword(const QString &password, const QString &salt) {
    return util::sha256Hex(salt + QStringLiteral("::") + password + QStringLiteral("::vocab"));
}

} // namespace rules
} // namespace vt
