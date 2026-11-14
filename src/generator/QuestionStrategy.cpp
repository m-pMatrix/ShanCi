#include "generator/QuestionStrategy.h"

#include "core/Logger.h"
#include "core/Utils.h"
#include "generator/Distractors.h"

#include <QRegularExpression>
#include <QSet>

namespace vt {

namespace {
// 依据单词信息估算难度（1..3）
int difficultyOf(const Word &w) {
    if (w.wordRank > 0 && w.wordRank <= 500) return 1;
    if (w.wordRank > 0 && w.wordRank <= 1500) return 2;
    return 3;
}

QString phoneticText(const Word &w) {
    const QString uk = w.phoneticUk.trimmed();
    const QString us = w.phoneticUs.trimmed();
    if (!uk.isEmpty()) return QStringLiteral("/%1/").arg(uk);
    if (!us.isEmpty()) return QStringLiteral("/%1/").arg(us);
    return QString();
}

// 英文释义（用于消除中文释义的歧义）
QString englishDefinition(const Word &w) {
    for (const auto &t : w.translations) {
        const QString s = util::stripHtml(t.tranEn).simplified();
        if (!s.isEmpty()) return s;
    }
    return QString();
}

QString truncate(const QString &s, int maxChars) {
    return s.size() <= maxChars ? s : s.left(maxChars) + QStringLiteral("…");
}

// 中文释义是否“可能产生歧义”：过短、多义、或词池中存在同义的其他单词
bool meaningIsAmbiguous(const Word &target, const QList<Word> &pool) {
    const QString cn = util::normalizeSpelling(target.primaryMeaning());
    if (cn.isEmpty() || cn.size() <= 4) return true;
    if (target.translations.size() > 1) return true;
    for (const auto &w : pool) {
        if (w.id == target.id) continue;
        if (util::normalizeSpelling(w.primaryMeaning()) == cn) return true;
    }
    return false;
}

int letterCount(const QString &s) {
    int n = 0;
    for (const QChar c : s)
        if (c.isLetter()) ++n;
    return n;
}

// 生成字母占位提示：保留首字母，其余用下划线，空格/连字符原样保留
QString maskedSpelling(const QString &spelling) {
    QString out;
    int letterIdx = 0;
    for (const QChar c : spelling) {
        if (c == QLatin1Char(' ') || c == QLatin1Char('-') || c == QLatin1Char('\'')) {
            out += c;
        } else if (letterIdx == 0) {
            out += QString(c).toLower();
            ++letterIdx;
        } else {
            out += QLatin1Char('_');
            ++letterIdx;
        }
    }
    return out;
}

// 返回可用于填空的例句（含目标词），并给出挖空后的句子
bool makeClozeSentence(const Word &w, QString *masked, QString *originCn) {
    for (const auto &s : w.sentences) {
        const QString plain = util::stripHtml(s.en);
        if (plain.trimmed().isEmpty()) continue;
        QRegularExpression re(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(w.spelling)),
                              QRegularExpression::CaseInsensitiveOption);
        if (!re.match(plain).hasMatch()) continue;
        QString out = plain;
        out.replace(re, QStringLiteral("______"));
        if (!out.contains(QStringLiteral("______"))) continue;
        *masked = out;
        if (originCn) *originCn = s.cn;
        return true;
    }
    return false;
}
} // namespace

// ---------------------------------------------------------------------------
// EnToCn：英文单词 → 中文释义
//   歧义消解：中文释义模糊时补充英文释义；干扰项优先取形近词的释义
// ---------------------------------------------------------------------------
bool EnToCnStrategy::canGenerate(const Word &w) const {
    return !w.primaryMeaning().trimmed().isEmpty();
}

Question EnToCnStrategy::generate(const Word &target, const QList<Word> &pool, const Setting &) const {
    Question q;
    q.type = QuestionType::EnToCn;
    q.wordId = target.id;
    q.difficulty = difficultyOf(target);
    q.answer = target.primaryMeaning();

    const QString ph = phoneticText(target);
    const QString en = englishDefinition(target);

    QString stem = QStringLiteral("选择与单词 “%1” %2 对应的中文释义").arg(target.spelling, ph);
    if (!en.isEmpty() && meaningIsAmbiguous(target, pool))
        stem += QStringLiteral("\n英释：%1").arg(truncate(en, 150));
    q.stem = stem;

    // 干扰项优先取形近词（且释义不重复/不包含）
    const QStringList distractors = DistractorGenerator::pickSimilarMeanings(target, pool, 3);
    int correctIndex = 0;
    q.options = DistractorGenerator::buildOptions(q.answer, distractors, &correctIndex);

    QStringList parts;
    parts << QStringLiteral("“%1” 意为：%2").arg(target.spelling, q.answer);
    if (!en.isEmpty()) parts << QStringLiteral("英释：%1").arg(en);
    if (!ph.isEmpty()) parts << QStringLiteral("音标：%1").arg(ph);
    QStringList others;
    for (const auto &t : target.translations) {
        const QString cn = t.tranCn.trimmed();
        if (cn.isEmpty() || cn == q.answer) continue;
        others << cn;
    }
    if (!others.isEmpty()) parts << QStringLiteral("其他释义：%1").arg(others.join(QStringLiteral("；")));
    q.explanation = parts.join(QStringLiteral("\n"));
    return q;
}

// ---------------------------------------------------------------------------
// CnToEn：中文释义 → 拼写英文单词
//   歧义消解：给出首字母 + 单词长度 + 字母占位提示
// ---------------------------------------------------------------------------
bool CnToEnStrategy::canGenerate(const Word &w) const {
    return !w.primaryMeaning().trimmed().isEmpty() && !w.spelling.trimmed().isEmpty();
}

Question CnToEnStrategy::generate(const Word &target, const QList<Word> &, const Setting &) const {
    Question q;
    q.type = QuestionType::CnToEn;
    q.wordId = target.id;
    q.difficulty = difficultyOf(target);
    q.answer = target.spelling;

    const int n = letterCount(target.spelling);
    const QString first = target.spelling.isEmpty() ? QString()
                                                   : QString(target.spelling.at(0)).toLower();
    q.stem = QStringLiteral("根据释义拼写英文单词：%1\n提示：首字母 %2 · 共 %3 个字母（%4）")
                 .arg(target.primaryMeaning(), first, QString::number(n),
                      maskedSpelling(target.spelling));

    const QString en = englishDefinition(target);
    const QString ph = phoneticText(target);
    QStringList parts;
    parts << QStringLiteral("正确答案：%1 %2").arg(target.spelling, ph);
    if (!en.isEmpty()) parts << QStringLiteral("英释：%1").arg(en);
    if (!target.sentences.isEmpty() && !target.sentences.first().en.isEmpty())
        parts << QStringLiteral("例句：%1").arg(util::stripHtml(target.sentences.first().en));
    q.explanation = parts.join(QStringLiteral("\n"));
    return q;
}

// ---------------------------------------------------------------------------
// SimilarMatch：形近词 → 匹配中文释义
//   歧义消解：排除与正解同义/近义的干扰词
// ---------------------------------------------------------------------------
bool SimilarMatchStrategy::canGenerate(const Word &w) const {
    return !w.primaryMeaning().trimmed().isEmpty() && !w.spelling.trimmed().isEmpty();
}

Question SimilarMatchStrategy::generate(const Word &target, const QList<Word> &pool, const Setting &) const {
    Question q;
    q.type = QuestionType::SimilarMatch;
    q.wordId = target.id;
    q.difficulty = 2;
    q.answer = target.spelling;

    const QString en = englishDefinition(target);
    QString stem = QStringLiteral("下列哪个单词的释义是“%1”？").arg(target.primaryMeaning());
    if (!en.isEmpty() && meaningIsAmbiguous(target, pool))
        stem += QStringLiteral("\n英释：%1").arg(truncate(en, 150));
    q.stem = stem;

    // 形近词作干扰项，并排除近义（避免两个正确答案）
    const QStringList distractors = DistractorGenerator::pickSimilarSpellings(target, pool, 3, false, true);
    int ci = 0;
    q.options = DistractorGenerator::buildOptions(q.answer, distractors, &ci);

    QStringList parts;
    parts << QStringLiteral("“%1” 释义为：%2").arg(target.spelling, target.primaryMeaning());
    if (!en.isEmpty()) parts << QStringLiteral("英释：%1").arg(en);
    q.explanation = parts.join(QStringLiteral("\n"));
    return q;
}

// ---------------------------------------------------------------------------
// Cloze：选词填空
//   歧义消解：干扰项取形近词、同词性、且不同义
// ---------------------------------------------------------------------------
bool ClozeStrategy::canGenerate(const Word &w) const {
    if (w.spelling.trimmed().isEmpty()) return false;
    QString masked, cn;
    return makeClozeSentence(w, &masked, &cn);
}

Question ClozeStrategy::generate(const Word &target, const QList<Word> &pool, const Setting &) const {
    Question q;
    q.type = QuestionType::Cloze;
    q.wordId = target.id;
    q.difficulty = difficultyOf(target);
    q.answer = target.spelling;

    QString masked, cn;
    if (makeClozeSentence(target, &masked, &cn)) {
        q.stem = QStringLiteral("选词填空：%1").arg(masked);
    } else {
        q.stem = QStringLiteral("选词填空：%1（释义：%2）")
                     .arg(QStringLiteral("______"), target.primaryMeaning());
    }

    // 同词性 + 不同义的形近词，避免靠语法或语义排除
    const QStringList distractors = DistractorGenerator::pickSimilarSpellings(target, pool, 3, true, true);
    int ci = 0;
    q.options = DistractorGenerator::buildOptions(q.answer, distractors, &ci);

    QStringList parts;
    parts << QStringLiteral("正确答案：%1").arg(target.spelling);
    if (!cn.isEmpty()) parts << QStringLiteral("译文：%1").arg(cn);
    const QString en = englishDefinition(target);
    if (!en.isEmpty()) parts << QStringLiteral("英释：%1").arg(en);
    q.explanation = parts.join(QStringLiteral("\n"));
    return q;
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------
QuestionStrategyRegistry &QuestionStrategyRegistry::instance() {
    static QuestionStrategyRegistry reg;
    return reg;
}

QuestionStrategyRegistry::QuestionStrategyRegistry() {
    m_strategies << new EnToCnStrategy << new CnToEnStrategy << new SimilarMatchStrategy << new ClozeStrategy;
}

const IQuestionStrategy *QuestionStrategyRegistry::strategy(QuestionType t) const {
    for (auto *s : m_strategies)
        if (s->type() == t) return s;
    return nullptr;
}

QList<QuestionType> QuestionStrategyRegistry::availableTypes(const Word &w,
                                                             const QList<QuestionType> &preferred) const {
    QList<QuestionType> out;
    for (auto t : preferred) {
        const auto *s = strategy(t);
        if (s && s->canGenerate(w)) out << t;
    }
    if (out.isEmpty()) {
        for (auto *s : m_strategies)
            if (s->canGenerate(w)) out << s->type();
    }
    return out;
}

Question QuestionStrategyRegistry::generate(QuestionType t, const Word &target, const QList<Word> &pool,
                                            const Setting &setting) const {
    const auto *s = strategy(t);
    if (s && s->canGenerate(target)) return s->generate(target, pool, setting);
    const QList<QuestionType> fallback = availableTypes(target, {QuestionType::EnToCn, QuestionType::CnToEn,
                                                                 QuestionType::SimilarMatch, QuestionType::Cloze});
    if (!fallback.isEmpty()) {
        const auto *fs = strategy(fallback.first());
        if (fs) return fs->generate(target, pool, setting);
    }
    return Question{};
}

} // namespace vt
