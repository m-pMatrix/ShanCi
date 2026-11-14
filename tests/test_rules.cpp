//
// test_rules.cpp - 领域规则单元测试
//
#include "domain/Rules.h"
#include "core/Utils.h"

#include <QtTest/QtTest>

using namespace vt;

class TestRules : public QObject {
    Q_OBJECT
private slots:
    void spellingCheck_data() {
        QTest::addColumn<QString>("user");
        QTest::addColumn<QString>("correct");
        QTest::addColumn<bool>("strict");
        QTest::addColumn<bool>("expected");

        QTest::newRow("exact") << "apple" << "apple" << true << true;
        QTest::newRow("case") << "Apple" << "apple" << true << true;
        QTest::newRow("space") << "  apple " << "apple" << true << true;
        QTest::newRow("strict-typo") << "aple" << "apple" << true << false;
        QTest::newRow("loose-typo") << "aple" << "apple" << false << true;
        QTest::newRow("empty") << "" << "apple" << false << false;
    }

    void spellingCheck() {
        QFETCH(QString, user);
        QFETCH(QString, correct);
        QFETCH(bool, strict);
        QFETCH(bool, expected);
        QCOMPARE(rules::checkSpelling(user, correct, strict), expected);
    }

    void choiceCheck() {
        QCOMPARE(rules::checkAnswer(QuestionType::EnToCn, "A", "A", true), true);
        QCOMPARE(rules::checkAnswer(QuestionType::EnToCn, "a", "A", true), true);
        QCOMPARE(rules::checkAnswer(QuestionType::EnToCn, "B", "A", true), false);
        QCOMPARE(rules::checkAnswer(QuestionType::EnToCn, "突然地", "突然地", true), true);
    }

    void scoring() {
        QCOMPARE(rules::scoreOf(0, 0), 0.0);
        QCOMPARE(rules::scoreOf(1, 2), 50.0);
        QCOMPARE(rules::scoreOf(2, 3), 66.7);
    }

    void summarize() {
        QList<PaperEntry> entries;
        PaperEntry a; a.answered = true; a.isCorrect = true; a.answerTimeMs = 1000;
        PaperEntry b; b.answered = true; b.isCorrect = false; b.answerTimeMs = 2000;
        PaperEntry c; c.answered = false;
        entries << a << b << c;
        const auto s = rules::summarize(entries);
        QCOMPARE(s.total, 3);
        QCOMPARE(s.correct, 1);
        QCOMPARE(s.wrong, 1);
        QCOMPARE(s.totalTimeMs, 3000LL);
        QCOMPARE(s.score, 33.3);
    }

    void reviewSchedule() {
        QVERIFY(rules::nextReviewIntervalDays(0, true) >= 0);
        QVERIFY(rules::nextReviewIntervalDays(5, true) > rules::nextReviewIntervalDays(1, true));
        QVERIFY(rules::nextReviewIntervalDays(3, false) <= rules::nextReviewIntervalDays(3, true));
    }

    void familiarity() {
        QCOMPARE(rules::updateFamiliarity(0, true), 1);
        QCOMPARE(rules::updateFamiliarity(5, true), 5);
        QCOMPARE(rules::updateFamiliarity(2, false), 1);
        QCOMPARE(rules::updateFamiliarity(0, false), 0);
    }

    void wrongPriorityAndStatus() {
        QCOMPARE(rules::computePriority(1), WrongPriority::Low);
        QCOMPARE(rules::computePriority(2), WrongPriority::Normal);
        QCOMPARE(rules::computePriority(5), WrongPriority::High);

        QCOMPARE(rules::computeStatus(1, 0, WrongStatus::New), WrongStatus::New);
        QCOMPARE(rules::computeStatus(2, 1, WrongStatus::New), WrongStatus::Reviewing);
        QCOMPARE(rules::computeStatus(1, 3, WrongStatus::Reviewing), WrongStatus::Mastered);
    }

    void accountValidation() {
        QVERIFY(rules::isUsernameValid("alice"));
        QVERIFY(rules::isUsernameValid("张三"));
        QVERIFY(!rules::isUsernameValid("a"));
        QVERIFY(!rules::isUsernameValid("bad name!"));
        QVERIFY(rules::isPasswordValid("1234"));
        QVERIFY(!rules::isPasswordValid("12"));
    }

    void passwordHashStable() {
        const QString salt = "abc123";
        QCOMPARE(rules::hashPassword("secret", salt), rules::hashPassword("secret", salt));
        QVERIFY(rules::hashPassword("secret", salt) != rules::hashPassword("secret2", salt));
    }

    void levenshtein() {
        QCOMPARE(util::levenshtein("apple", "apple"), 0);
        QCOMPARE(util::levenshtein("apple", "aple"), 1);
        QCOMPARE(util::levenshtein("", "abc"), 3);
    }

    // 回归：Qt6 中 QRandomGenerator::global()->seed() 会 qFatal，必须使用独立生成器
    void randomHelpers() {
        util::seedRandom(12345);
        for (int i = 0; i < 50; ++i) {
            const int v = util::randomInt(1, 10);
            QVERIFY(v >= 1 && v <= 10);
        }
        QList<int> list{1, 2, 3, 4, 5};
        util::shuffle(list);
        QCOMPARE(list.size(), 5);

        util::seedRandom(999);
        const int a = util::randomInt(0, 100000);
        util::seedRandom(999);
        const int b = util::randomInt(0, 100000);
        QCOMPARE(a, b); // 相同种子应产生相同序列

        const auto sample = util::sample(QList<int>{1, 2, 3, 4, 5, 6, 7}, 3);
        QCOMPARE(sample.size(), 3);
    }
};

QTEST_APPLESS_MAIN(TestRules)
#include "test_rules.moc"
