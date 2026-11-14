#pragma once
//
// QuestionStrategy.h - 题目生成策略（第四层：多种题型策略）
//
#include "domain/Entities.h"

#include <QList>

namespace vt {

// 题型策略接口
class IQuestionStrategy {
public:
    virtual ~IQuestionStrategy() = default;
    virtual QuestionType type() const = 0;
    // 该单词是否可生成此题型
    virtual bool canGenerate(const Word &w) const = 0;
    // 生成题目；pool 为同书词池（用于干扰项）
    virtual Question generate(const Word &target, const QList<Word> &pool, const Setting &setting) const = 0;
};

// 具体策略
class EnToCnStrategy : public IQuestionStrategy {
public:
    QuestionType type() const override { return QuestionType::EnToCn; }
    bool canGenerate(const Word &w) const override;
    Question generate(const Word &target, const QList<Word> &pool, const Setting &setting) const override;
};

class CnToEnStrategy : public IQuestionStrategy {
public:
    QuestionType type() const override { return QuestionType::CnToEn; }
    bool canGenerate(const Word &w) const override;
    Question generate(const Word &target, const QList<Word> &pool, const Setting &setting) const override;
};

class SimilarMatchStrategy : public IQuestionStrategy {
public:
    QuestionType type() const override { return QuestionType::SimilarMatch; }
    bool canGenerate(const Word &w) const override;
    Question generate(const Word &target, const QList<Word> &pool, const Setting &setting) const override;
};

class ClozeStrategy : public IQuestionStrategy {
public:
    QuestionType type() const override { return QuestionType::Cloze; }
    bool canGenerate(const Word &w) const override;
    Question generate(const Word &target, const QList<Word> &pool, const Setting &setting) const override;
};

// 策略注册表
class QuestionStrategyRegistry {
public:
    static QuestionStrategyRegistry &instance();

    const IQuestionStrategy *strategy(QuestionType t) const;

    // 依据题型生成题目；若该题型不可用则回退到可选策略
    Question generate(QuestionType t, const Word &target, const QList<Word> &pool,
                      const Setting &setting) const;

    // 从候选题型中挑选可用的
    QList<QuestionType> availableTypes(const Word &w, const QList<QuestionType> &preferred) const;

private:
    QuestionStrategyRegistry();
    QList<IQuestionStrategy *> m_strategies;
};

} // namespace vt
