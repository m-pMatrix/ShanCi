//
// test_generator.cpp - 题目生成策略单元测试
//
#include "domain/Entities.h"
#include "generator/Distractors.h"
#include "generator/QuestionStrategy.h"

#include <QtTest/QtTest>

using namespace vt;

namespace {
Word makeWord(qint64 id, const QString &spelling, const QString &meaning, const QString &sentence = QString(),
              const QString &en = QString()) {
    Word w;
    w.id = id;
    w.spelling = spelling;
    w.book = BookType::CET4;
    w.wordRank = static_cast<int>(id);
    Translation t;
    t.pos = QStringLiteral("v");
    t.tranCn = meaning;
    t.tranEn = en;
    w.translations << t;
    if (!sentence.isEmpty()) {
        Sentence s;
        s.en = sentence;
        s.cn = QStringLiteral("例句翻译");
        s.tag = QStringLiteral("例句");
        w.sentences << s;
    }
    return w;
}

QList<Word> makePool() {
    return {makeWord(1, "absorb", "吸收"), makeWord(2, "abuse", "滥用"),
            makeWord(3, "abstract", "抽象的"), makeWord(4, "abrupt", "突然的"),
            makeWord(5, "absent", "缺席的")};
}
} // namespace

class TestGenerator : public QObject {
    Q_OBJECT
private slots:
    void enToCn() {
        const Word target = makeWord(99, "abandon", "放弃");
        const auto q = EnToCnStrategy().generate(target, makePool(), Setting{});
        QCOMPARE(q.type, QuestionType::EnToCn);
        QCOMPARE(q.options.size(), 4);
        QVERIFY(q.options.contains(q.answer));
        QCOMPARE(q.answer, QStringLiteral("放弃"));
        QVERIFY(!q.stem.isEmpty());
    }

    void cnToEn() {
        const Word target = makeWord(99, "abandon", "放弃");
        const auto q = CnToEnStrategy().generate(target, {}, Setting{});
        QCOMPARE(q.type, QuestionType::CnToEn);
        QVERIFY(q.options.isEmpty());
        QCOMPARE(q.answer, QStringLiteral("abandon"));
    }

    // 改进点：拼写题给出首字母 + 单词长度，消除同义/近义带来的答案不唯一
    void cnToEnGivesLetterAndLengthHint() {
        const Word target = makeWord(99, "abruptly", "突然地");
        const auto q = CnToEnStrategy().generate(target, {}, Setting{});
        QVERIFY2(q.stem.contains(QStringLiteral("首字母 a")), qPrintable(q.stem));
        QVERIFY2(q.stem.contains(QStringLiteral("8 个字母")), qPrintable(q.stem));
        QVERIFY2(q.stem.contains(QStringLiteral("a_______")), qPrintable(q.stem));
        QCOMPARE(q.answer, QStringLiteral("abruptly"));
    }

    // 改进点：中文释义模糊时补充英文释义
    void enToCnAddsEnglishHintWhenAmbiguous() {
        const Word target = makeWord(99, "adapt", QStringLiteral("适应"), QString(),
                                     QStringLiteral("to change to suit new conditions"));
        const auto q = EnToCnStrategy().generate(target, makePool(), Setting{});
        QCOMPARE(q.options.size(), 4);
        QVERIFY2(q.stem.contains(QStringLiteral("英释")), qPrintable(q.stem));
        QVERIFY2(q.stem.contains(QStringLiteral("to change to suit new conditions")), qPrintable(q.stem));
        QVERIFY(q.explanation.contains(QStringLiteral("英释")));
    }

    // 改进点：选词填空排除近义干扰项，避免多个选项都说得通
    void clozeExcludesSynonymDistractors() {
        const Word target = makeWord(99, "adapt", QStringLiteral("适应"),
                                     QStringLiteral("You must adapt to the new environment."));
        QList<Word> pool;
        pool << makeWord(1, "adopt", QStringLiteral("适应"))    // 近义，应被排除
             << makeWord(2, "adept", QStringLiteral("熟练的"))
             << makeWord(3, "adult", QStringLiteral("成年人"))
             << makeWord(4, "yacht", QStringLiteral("游艇"));
        const auto q = ClozeStrategy().generate(target, pool, Setting{});
        QVERIFY(!q.options.contains(QStringLiteral("adopt")));
        QVERIFY(q.options.contains(QStringLiteral("adapt")));
        QVERIFY(q.stem.contains(QStringLiteral("______")));
    }

    void similarMatch() {
        const Word target = makeWord(99, "abandon", "放弃");
        const auto q = SimilarMatchStrategy().generate(target, makePool(), Setting{});
        QCOMPARE(q.type, QuestionType::SimilarMatch);
        QCOMPARE(q.options.size(), 4);
        QVERIFY(q.options.contains(QStringLiteral("abandon")));
        QCOMPARE(q.answer, QStringLiteral("abandon"));
    }

    void cloze() {
        const Word target = makeWord(99, "abandon", "放弃",
                                     QStringLiteral("He decided to abandon the plan."));
        QVERIFY(ClozeStrategy().canGenerate(target));
        const auto q = ClozeStrategy().generate(target, makePool(), Setting{});
        QCOMPARE(q.type, QuestionType::Cloze);
        QVERIFY(q.stem.contains(QStringLiteral("______")));
        QCOMPARE(q.answer, QStringLiteral("abandon"));
    }

    void clozeUnavailableWithoutSentence() {
        const Word target = makeWord(99, "abandon", "放弃");
        QVERIFY(!ClozeStrategy().canGenerate(target));
    }

    void registryFallback() {
        const Word target = makeWord(99, "abandon", "放弃");
        // Cloze 不可用时应回退到其它策略
        const auto q = QuestionStrategyRegistry::instance().generate(QuestionType::Cloze, target,
                                                                     makePool(), Setting{});
        QVERIFY(q.isValid());
        QVERIFY(q.type != QuestionType::Cloze);
    }

    // 改进点：英文选释义的干扰项应优先取自“形近词”
    void enToCnPrefersSimilarWords() {
        const Word target = makeWord(99, "adapt", QStringLiteral("适应"));
        QList<Word> pool;
        pool << makeWord(1, "adopt", QStringLiteral("采用"))
             << makeWord(2, "adept", QStringLiteral("熟练的"))
             << makeWord(3, "adult", QStringLiteral("成年人"))
             << makeWord(4, "zebra", QStringLiteral("斑马"))
             << makeWord(5, "yacht", QStringLiteral("游艇"));

        const auto q = EnToCnStrategy().generate(target, pool, Setting{});
        QCOMPARE(q.options.size(), 4);
        QVERIFY(q.options.contains(q.answer));

        int similarHits = 0;
        if (q.options.contains(QStringLiteral("采用"))) ++similarHits;
        if (q.options.contains(QStringLiteral("熟练的"))) ++similarHits;
        if (q.options.contains(QStringLiteral("成年人"))) ++similarHits;
        QVERIFY2(similarHits >= 2, qPrintable(QStringLiteral("形近词释义命中数=%1").arg(similarHits)));
    }

    // 展示并校验四种题型的实际题干（便于人工核对去歧义效果）
    void sampleStems() {
        Word w = makeWord(1, "abruptly", QStringLiteral("突然地"),
                          QStringLiteral("The path ends off abruptly."),
                          QStringLiteral("in a sudden and unexpected way"));
        w.phoneticUk = QStringLiteral("ə'brʌptli");

        const auto en2cn = EnToCnStrategy().generate(w, makePool(), Setting{});
        const auto cn2en = CnToEnStrategy().generate(w, {}, Setting{});
        const auto similar = SimilarMatchStrategy().generate(w, makePool(), Setting{});
        const auto cloze = ClozeStrategy().generate(w, makePool(), Setting{});

        qInfo("\n[英文选中文释义]\n%s\n选项: %s", qPrintable(en2cn.stem),
              qPrintable(en2cn.options.join(QStringLiteral(" | "))));
        qInfo("\n[中文拼写英文]\n%s", qPrintable(cn2en.stem));
        qInfo("\n[形近词匹配]\n%s\n选项: %s", qPrintable(similar.stem),
              qPrintable(similar.options.join(QStringLiteral(" | "))));
        qInfo("\n[选词填空]\n%s\n选项: %s", qPrintable(cloze.stem),
              qPrintable(cloze.options.join(QStringLiteral(" | "))));

        // 释义过短 → 必须补充英释
        QVERIFY(en2cn.stem.contains(QStringLiteral("英释")));
        QVERIFY(similar.stem.contains(QStringLiteral("英释")));
        // 拼写题必须给出首字母与长度
        QVERIFY(cn2en.stem.contains(QStringLiteral("首字母 a")));
        // 各题型都应有 ≥4 个选项（拼写题除外）
        QVERIFY(en2cn.options.size() >= 4);
        QVERIFY(similar.options.size() >= 4);
        QVERIFY(cloze.options.size() >= 4);
    }

    void distractorOptionsAreUnique() {        const Word target = makeWord(99, "abandon", "放弃");
        QStringList distractors{QStringLiteral("放弃"), QStringLiteral("吸收"), QStringLiteral("吸收")};
        int ci = -1;
        const auto options = DistractorGenerator::buildOptions(QStringLiteral("放弃"), distractors, &ci);
        QCOMPARE(options.size(), 2); // “放弃”重复被去除
        QCOMPARE(options[ci], QStringLiteral("放弃"));
    }
};

QTEST_APPLESS_MAIN(TestGenerator)
#include "test_generator.moc"
