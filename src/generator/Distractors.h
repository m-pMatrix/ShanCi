#pragma once
//
// Distractors.h - 干扰项生成（第四层：干扰项生成）
//
#include "domain/Entities.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace vt {

class DistractorGenerator {
public:
    // 从词池中挑选 count 个与目标词不同的中文释义（去重）
    static QStringList pickMeanings(const Word &target, const QList<Word> &pool, int count);

    // 优先挑选“形近词”的释义作为干扰项（更贴近真实考试的辨析题）
    static QStringList pickSimilarMeanings(const Word &target, const QList<Word> &pool, int count);

    // 从词池中挑选 count 个与目标词不同的拼写
    static QStringList pickSpellings(const Word &target, const QList<Word> &pool, int count);

    // 优先挑选形近词的拼写作为干扰项（用于选词填空 / 相似词匹配）
    // samePosOnly: 仅取与目标词同词性的词（避免靠语法排除）；excludeSameMeaning: 排除近义词
    static QStringList pickSimilarSpellings(const Word &target, const QList<Word> &pool, int count,
                                            bool samePosOnly = false,
                                            bool excludeSameMeaning = false);

    // 挑选 count 个形近词（优先词形相近者）
    static QList<Word> pickSimilarWords(const Word &target, const QList<Word> &pool, int count);

    // 把正确答案与干扰项混合：返回打乱后的选项列表，并在 correctIndex 中写回正确项下标
    static QStringList buildOptions(const QString &correct, QStringList distractors, int *correctIndex);

    // 为答案生成占位干扰（词池不足时使用）
    static QStringList syntheticMeanings(int count);
};

} // namespace vt
