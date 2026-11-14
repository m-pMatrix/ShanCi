//
// test_repositories.cpp - 数据访问层测试（SQLite 往返）
//
#include "data/Database.h"
#include "data/Repositories.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace vt;

class TestRepositories : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        auto &db = Database::instance();
        QVERIFY(db.open(m_dir.filePath(QStringLiteral("test.db"))));
        m_words = new WordRepository(db);
        m_books = new WordBookRepository(db);
        m_wrong = new WrongBookRepository(db);
        m_users = new UserRepository(db);
    }

    void cleanupTestCase() {
        delete m_words; delete m_books; delete m_wrong; delete m_users;
        Database::instance().close();
    }

    void wordRoundTrip() {
        Word w;
        w.book = BookType::CET6;
        w.wordRank = 42;
        w.spelling = QStringLiteral("abandon");
        w.phoneticUk = QStringLiteral("ə'bændən");
        w.star = 2;
        Translation t; t.pos = QStringLiteral("v"); t.tranCn = QStringLiteral("放弃；抛弃");
        w.translations << t;
        Phrase p; p.content = QStringLiteral("abandon oneself to"); p.meaning = QStringLiteral("沉溺于");
        w.phrases << p;
        Sentence s; s.en = QStringLiteral("He abandoned the plan."); s.cn = QStringLiteral("他放弃了计划。");
        w.sentences << s;
        Synonym syn; syn.pos = QStringLiteral("v"); syn.tran = QStringLiteral("放弃");
        syn.words << QStringLiteral("desert") << QStringLiteral("quit");
        w.synonyms << syn;
        RelWord r; r.pos = QStringLiteral("n");
        r.words << qMakePair(QStringLiteral("abandonment"), QStringLiteral("放弃"));
        w.relWords << r;
        ExamItem e; e.question = QStringLiteral("He ___ the plan.");
        e.choices << QStringLiteral("a") << QStringLiteral("b"); e.rightIndex = 0;
        w.exams << e;

        const qint64 id = m_words->insert(w);
        QVERIFY(id > 0);

        const auto found = m_words->findById(id);
        QVERIFY(found.has_value());
        QCOMPARE(found->spelling, QStringLiteral("abandon"));
        QCOMPARE(found->book, BookType::CET6);
        QCOMPARE(found->phrases.size(), 1);
        QCOMPARE(found->phrases.first().content, QStringLiteral("abandon oneself to"));
        QCOMPARE(found->sentences.size(), 1);
        QCOMPARE(found->synonyms.first().words.size(), 2);
        QCOMPARE(found->relWords.first().words.first().first, QStringLiteral("abandonment"));
        QCOMPARE(found->exams.first().rightIndex, 0);
    }

    void upsertAndSearch() {
        Word a; a.book = BookType::CET4; a.spelling = QStringLiteral("absorb"); a.wordRank = 1;
        Translation ta; ta.tranCn = QStringLiteral("吸收"); a.translations << ta;
        const qint64 id1 = m_words->upsert(a);

        Word a2 = a; a2.wordRank = 5;
        Translation ta2; ta2.tranCn = QStringLiteral("吸收；吸引"); a2.translations.clear(); a2.translations << ta2;
        const qint64 id2 = m_words->upsert(a2);
        QCOMPARE(id1, id2); // 同一 (book, spelling) 应更新而非新增
        QCOMPARE(m_words->findById(id2)->translations.first().tranCn, QStringLiteral("吸收；吸引"));

        WordQuery q;
        q.keyword = QStringLiteral("吸");
        int total = 0;
        const auto results = m_words->search(q, &total);
        QVERIFY(total >= 1);
        QVERIFY(!results.isEmpty());
    }

    void wordBookEntries() {
        const qint64 uid = createUser(QStringLiteral("alice"));
        const qint64 bookId = m_books->createBook(WordBook{0, uid, QStringLiteral("默认"), QString()});
        QVERIFY(bookId > 0);

        Word w; w.book = BookType::CET4; w.spelling = QStringLiteral("abandon"); w.wordRank = 3;
        Translation t; t.tranCn = QStringLiteral("放弃"); w.translations << t;
        const qint64 wid = m_words->insert(w);

        QVERIFY(m_books->addEntry(bookId, wid, EntrySource::Manual));
        QVERIFY(m_books->hasEntry(bookId, wid));
        QCOMPARE(m_books->countEntries(bookId), 1LL);

        const auto entries = m_books->listEntries(bookId, WordQuery{});
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().word.spelling, QStringLiteral("abandon"));

        QVERIFY(m_books->updateEntryProgress(bookId, wid, 3, 2, QDateTime::currentDateTime()));
        QCOMPARE(m_books->findEntry(bookId, wid)->familiarity, 3);
    }

    void wrongBookEntries() {
        const qint64 uid = createUser(QStringLiteral("bob"));
        const qint64 wbId = m_wrong->ensureBook(uid);
        QVERIFY(wbId > 0);

        Word w; w.book = BookType::CET4; w.spelling = QStringLiteral("abuse"); w.wordRank = 7;
        Translation t; t.tranCn = QStringLiteral("滥用"); w.translations << t;
        const qint64 wid = m_words->insert(w);

        const qint64 eid = m_wrong->recordWrong(wbId, wid, QuestionType::EnToCn, QStringLiteral("B"));
        QVERIFY(eid > 0);
        auto e = m_wrong->findEntry(wbId, wid);
        QVERIFY(e.has_value());
        QCOMPARE(e->wrongCount, 1);
        QCOMPARE(e->priority, WrongPriority::Low);

        m_wrong->recordWrong(wbId, wid, QuestionType::EnToCn, QStringLiteral("C"));
        m_wrong->recordWrong(wbId, wid, QuestionType::EnToCn, QStringLiteral("D"));
        e = m_wrong->findEntry(wbId, wid);
        QCOMPARE(e->wrongCount, 3);
        QCOMPARE(e->priority, WrongPriority::High);

        QVERIFY(m_wrong->removeEntry(eid));
        QVERIFY(!m_wrong->findEntry(wbId, wid).has_value());
    }

    // 实时联想：前缀优先，最多 5 条
    void suggest() {
        auto r = m_words->suggest(QStringLiteral("ab"), QString(), 5);
        QVERIFY(!r.isEmpty());
        QVERIFY(r.size() <= 5);
        QVERIFY(r.first().spelling.startsWith(QStringLiteral("ab"), Qt::CaseInsensitive));

        auto exact = m_words->suggest(QStringLiteral("absorb"), QString(), 5);
        QVERIFY(!exact.isEmpty());
        QCOMPARE(exact.first().spelling, QStringLiteral("absorb"));

        // 中文释义联想
        auto cn = m_words->suggest(QStringLiteral("吸"), QString(), 5);
        QVERIFY(!cn.isEmpty());

        // 空关键词
        QVERIFY(m_words->suggest(QString(), QString(), 5).isEmpty());
    }

    void transactionRollback() {        auto &db = Database::instance();
        Word w; w.book = BookType::Custom; w.spelling = QStringLiteral("txword");
        Translation t; t.tranCn = QStringLiteral("事务");
        w.translations << t;
        QVERIFY(db.db().begin());
        const qint64 id = m_words->insert(w);
        QVERIFY(id > 0);
        QVERIFY(db.db().rollback());
        QVERIFY(!m_words->findById(id).has_value());
    }

private:
    qint64 createUser(const QString &name) {
        User u;
        u.username = name;
        u.salt = QStringLiteral("salt");
        u.passwordHash = QStringLiteral("hash");
        return m_users->create(u);
    }

    QTemporaryDir m_dir;
    WordRepository *m_words = nullptr;
    WordBookRepository *m_books = nullptr;
    WrongBookRepository *m_wrong = nullptr;
    UserRepository *m_users = nullptr;
};

QTEST_APPLESS_MAIN(TestRepositories)
#include "test_repositories.moc"
