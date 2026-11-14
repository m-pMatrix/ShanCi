#include "generator/Distractors.h"

#include "core/Utils.h"

#include <QSet>

#include <algorithm>

namespace vt {

namespace {
// 两个释义是否“语义冲突/重复”（互为子串即视为同义或包含关系，不能同时作为选项）
bool meaningsOverlap(const QString &a, const QString &b) {
    const QString na = util::normalizeSpelling(a);
    const QString nb = util::normalizeSpelling(b);
    if (na.isEmpty() || nb.isEmpty()) return true;
    return na.contains(nb) || nb.contains(na);
}

struct ScoredWord {
    double score;
    Word word;
};

QList<ScoredWord> rankBySimilarity(const Word &target, const QList<Word> &pool,
                                   bool samePosOnly, bool excludeSameMeaning) {
    const QString targetSpelling = util::normalizeSpelling(target.spelling);
    const QString targetMeaning = target.primaryMeaning();
    const QString targetPos = target.posOfFirst().trimmed().toLower();

    QList<ScoredWord> scored;
    for (const auto &w : pool) {
        if (w.id == target.id) continue;
        if (w.spelling.trimmed().isEmpty()) continue;
        if (samePosOnly && !targetPos.isEmpty() &&
            w.posOfFirst().trimmed().toLower() != targetPos)
            continue;
        if (excludeSameMeaning && !targetMeaning.isEmpty() &&
            meaningsOverlap(w.primaryMeaning(), targetMeaning))
            continue;

        const QString sp = util::normalizeSpelling(w.spelling);
        const double sim = util::similarity(targetSpelling, sp);
        const double pre = util::prefixSimilarity(targetSpelling, sp);
        scored.append({sim * 0.6 + pre * 0.4, w});
    }
    std::sort(scored.begin(), scored.end(),
              [](const ScoredWord &a, const ScoredWord &b) { return a.score > b.score; });
    return scored;
}
} // namespace

QStringList DistractorGenerator::pickMeanings(const Word &target, const QList<Word> &pool, int count) {
    QStringList out;
    const QString correct = target.primaryMeaning();

    QList<Word> shuffled = pool;
    util::shuffle(shuffled);
    for (const auto &w : shuffled) {
        if (out.size() >= count) break;
        if (w.id == target.id) continue;
        const QString m = w.primaryMeaning();
        if (m.trimmed().isEmpty()) continue;
        if (meaningsOverlap(m, correct)) continue;
        bool dup = false;
        for (const auto &x : out)
            if (meaningsOverlap(x, m)) { dup = true; break; }
        if (dup) continue;
        out << m;
    }
    if (out.size() < count) {
        for (const auto &s : syntheticMeanings(count - out.size())) out << s;
    }
    return out;
}

QStringList DistractorGenerator::pickSimilarMeanings(const Word &target, const QList<Word> &pool, int count) {
    QStringList out;
    const QString correct = target.primaryMeaning();

    // 1) 优先：形近词 + 释义不同（作为迷惑项最有价值）
    for (const auto &cand : rankBySimilarity(target, pool, false, true)) {
        if (out.size() >= count) break;
        const QString m = cand.word.primaryMeaning();
        if (m.trimmed().isEmpty()) continue;
        if (meaningsOverlap(m, correct)) continue;
        bool dup = false;
        for (const auto &x : out)
            if (meaningsOverlap(x, m)) { dup = true; break; }
        if (dup) continue;
        out << m;
    }
    // 2) 补齐：普通释义（仍保证不与正解/已选项重复）
    if (out.size() < count) {
        for (const auto &m : pickMeanings(target, pool, count * 2)) {
            if (out.size() >= count) break;
            if (meaningsOverlap(m, correct)) continue;
            bool dup = false;
            for (const auto &x : out)
                if (meaningsOverlap(x, m)) { dup = true; break; }
            if (dup) continue;
            out << m;
        }
    }
    // 3) 最后用合成干扰补齐
    int guard = 0;
    while (out.size() < count && guard++ < 50) {
        bool added = false;
        for (const auto &s : syntheticMeanings(count)) {
            if (out.size() >= count) break;
            if (meaningsOverlap(s, correct)) continue;
            bool dup = false;
            for (const auto &x : out)
                if (meaningsOverlap(x, s)) { dup = true; break; }
            if (dup) continue;
            out << s;
            added = true;
        }
        if (!added) break;
    }
    return out;
}

QStringList DistractorGenerator::pickSpellings(const Word &target, const QList<Word> &pool, int count) {
    QStringList out;
    QSet<QString> seen;
    seen.insert(util::normalizeSpelling(target.spelling));
    QList<Word> shuffled = pool;
    util::shuffle(shuffled);
    for (const auto &w : shuffled) {
        if (out.size() >= count) break;
        if (w.id == target.id) continue;
        const QString key = util::normalizeSpelling(w.spelling);
        if (key.isEmpty() || seen.contains(key)) continue;
        seen.insert(key);
        out << w.spelling;
    }
    return out;
}

QStringList DistractorGenerator::pickSimilarSpellings(const Word &target, const QList<Word> &pool, int count,
                                                      bool samePosOnly, bool excludeSameMeaning) {
    QStringList out;
    QSet<QString> seen;
    seen.insert(util::normalizeSpelling(target.spelling));

    auto take = [&](const QList<ScoredWord> &ranked) {
        for (const auto &cand : ranked) {
            if (out.size() >= count) break;
            const QString key = util::normalizeSpelling(cand.word.spelling);
            if (key.isEmpty() || seen.contains(key)) continue;
            seen.insert(key);
            out << cand.word.spelling;
        }
    };

    // 严格条件（同词性 + 释义不同）→ 逐步放宽，保证选项数量
    take(rankBySimilarity(target, pool, samePosOnly, excludeSameMeaning));
    if (out.size() < count)
        take(rankBySimilarity(target, pool, false, excludeSameMeaning));
    if (out.size() < count)
        take(rankBySimilarity(target, pool, false, false));
    if (out.size() < count) {
        for (const auto &s : pickSpellings(target, pool, count * 2)) {
            if (out.size() >= count) break;
            if (!seen.contains(util::normalizeSpelling(s))) {
                seen.insert(util::normalizeSpelling(s));
                out << s;
            }
        }
    }
    return out;
}

QList<Word> DistractorGenerator::pickSimilarWords(const Word &target, const QList<Word> &pool, int count) {
    QList<Word> out;
    for (const auto &c : rankBySimilarity(target, pool, false, false)) {
        if (out.size() >= count) break;
        out << c.word;
    }
    return out;
}

QStringList DistractorGenerator::buildOptions(const QString &correct, QStringList distractors, int *correctIndex) {
    QStringList items;
    const QString correctKey = util::normalizeSpelling(correct);
    for (const auto &d : distractors) {
        const QString key = util::normalizeSpelling(d);
        if (key.isEmpty() || key == correctKey) continue;
        bool dup = false;
        for (const auto &x : items)
            if (util::normalizeSpelling(x) == key) { dup = true; break; }
        if (dup) continue;
        items << d;
    }
    items << correct;

    QList<QString> list = items;
    util::shuffle(list);

    QStringList result;
    int ci = 0;
    for (int i = 0; i < list.size(); ++i) {
        result << list[i];
        if (list[i] == correct) ci = i;
    }
    if (correctIndex) *correctIndex = ci;
    return result;
}

QStringList DistractorGenerator::syntheticMeanings(int count) {
    static const QStringList pool = {
        QStringLiteral("n. 谨慎；小心"), QStringLiteral("v. 采纳；收养"), QStringLiteral("adj. 模糊的；含糊的"),
        QStringLiteral("adv. 显著地；明显地"), QStringLiteral("n. 争议；争论"), QStringLiteral("v. 减少；缩小"),
        QStringLiteral("adj. 连续的；持续的"), QStringLiteral("n. 趋势；倾向"), QStringLiteral("v. 分配；指派"),
        QStringLiteral("adj. 关键的；至关重要的"), QStringLiteral("n. 障碍；妨害"), QStringLiteral("v. 证明；证实"),
    };
    QStringList out;
    QList<QString> copy = pool;
    util::shuffle(copy);
    for (int i = 0; i < count && i < copy.size(); ++i) out << copy[i];
    return out;
}

} // namespace vt
