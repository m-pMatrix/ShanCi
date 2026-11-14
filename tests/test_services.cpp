//
// test_services.cpp - 服务层集成测试（导入 → 单词本 → 出题 → 作答 → 错题 → 同步）
//
#include "core/Utils.h"
#include "data/Database.h"
#include "data/Repositories.h"
#include "importer/JsonlImporter.h"
#include "service/Services.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace vt;

class TestServices : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void importJsonl();
    void paperFlowAndWrongBook();
    void syncRoundTrip();
    void studyPlanFlow();
    void multiWordBook();
    void planCoversWholeVocabulary();
    void questionGenerationCreatesNoWrongEntries();
    void passwordChangeAndRememberToken();
    void multiplePlans();
    void noEmptyDailyListOrPaper();

private:
    QString writeSampleJsonl(int count);

    QTemporaryDir m_dir;
    Database *m_db = nullptr;
    std::unique_ptr<UserRepository> users;
    std::unique_ptr<SettingRepository> settings;
    std::unique_ptr<WordRepository> words;
    std::unique_ptr<WordBookRepository> bookRepo;
    std::unique_ptr<WrongBookRepository> wrongRepo;
    std::unique_ptr<QuestionRepository> questions;
    std::unique_ptr<PaperRepository> papers;
    std::unique_ptr<DailyListRepository> daily;
    std::unique_ptr<SyncLogRepository> syncLogs;
    std::unique_ptr<StudyPlanRepository> plans;

    std::unique_ptr<AuthService> auth;
    std::unique_ptr<SettingService> settingService;
    std::unique_ptr<WordService> wordService;
    std::unique_ptr<WordBookService> bookService;
    std::unique_ptr<WrongBookService> wrongService;
    std::unique_ptr<QuestionService> questionService;
    std::unique_ptr<DailyListService> dailyService;
    std::unique_ptr<SyncService> syncService;

    qint64 userId = 0;
};

void TestServices::initTestCase() {
    QVERIFY(m_dir.isValid());
    m_db = &Database::instance();
    QVERIFY(m_db->open(m_dir.filePath(QStringLiteral("svc.db"))));

    users = std::make_unique<UserRepository>(*m_db);
    settings = std::make_unique<SettingRepository>(*m_db);
    words = std::make_unique<WordRepository>(*m_db);
    bookRepo = std::make_unique<WordBookRepository>(*m_db);
    wrongRepo = std::make_unique<WrongBookRepository>(*m_db);
    questions = std::make_unique<QuestionRepository>(*m_db);
    papers = std::make_unique<PaperRepository>(*m_db);
    daily = std::make_unique<DailyListRepository>(*m_db);
    syncLogs = std::make_unique<SyncLogRepository>(*m_db);
    plans = std::make_unique<StudyPlanRepository>(*m_db);

    auth = std::make_unique<AuthService>(*users, *settings, *bookRepo, *wrongRepo);
    settingService = std::make_unique<SettingService>(*settings);
    wordService = std::make_unique<WordService>(*words, *m_db);
    bookService = std::make_unique<WordBookService>(*bookRepo, *auth, *m_db);
    wrongService = std::make_unique<WrongBookService>(*wrongRepo);
    questionService = std::make_unique<QuestionService>(*questions, *papers, *words, *bookRepo, *wrongRepo,
                                                        *settings, *plans);
    dailyService = std::make_unique<DailyListService>(*daily, *plans, *bookRepo, *wrongRepo, *words,
                                                      *settings);
    syncService = std::make_unique<SyncService>(*wrongRepo, *settings, *syncLogs, *users);
}

void TestServices::cleanupTestCase() {
    syncService.reset(); dailyService.reset(); questionService.reset(); wrongService.reset();
    bookService.reset(); wordService.reset(); settingService.reset(); auth.reset();
    plans.reset(); syncLogs.reset(); daily.reset(); papers.reset(); questions.reset(); wrongRepo.reset();
    bookRepo.reset(); words.reset(); settings.reset(); users.reset();
    Database::instance().close();
}

QString TestServices::writeSampleJsonl(int count) {
    QString path = m_dir.filePath(QStringLiteral("sample.jsonl"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return QString();
    for (int i = 0; i < count; ++i) {
        QJsonObject root;
        root.insert(QStringLiteral("wordRank"), i + 1);
        const QString spelling = QStringLiteral("word%1").arg(i + 1);
        root.insert(QStringLiteral("headWord"), spelling);

        QJsonObject inner; // content.word.content
        QJsonArray trans;
        QJsonObject t;
        t.insert(QStringLiteral("tranCn"), QStringLiteral("释义%1").arg(i + 1));
        t.insert(QStringLiteral("pos"), QStringLiteral("v"));
        trans.append(t);
        inner.insert(QStringLiteral("trans"), trans);
        inner.insert(QStringLiteral("ukphone"), QStringLiteral("phon%1").arg(i + 1));

        QJsonObject sentenceWrap;
        QJsonArray sentences;
        QJsonObject s;
        s.insert(QStringLiteral("sContent"), QStringLiteral("This is %1 example.").arg(spelling));
        s.insert(QStringLiteral("sCn"), QStringLiteral("这是%1的例句。").arg(spelling));
        sentences.append(s);
        sentenceWrap.insert(QStringLiteral("sentences"), sentences);
        inner.insert(QStringLiteral("sentence"), sentenceWrap);

        QJsonObject wordObj;
        wordObj.insert(QStringLiteral("wordHead"), spelling);
        wordObj.insert(QStringLiteral("wordId"), QStringLiteral("CET4_1_%1").arg(i + 1));
        wordObj.insert(QStringLiteral("content"), inner);

        QJsonObject contentWrap;
        contentWrap.insert(QStringLiteral("word"), wordObj);
        root.insert(QStringLiteral("content"), contentWrap);
        root.insert(QStringLiteral("bookId"), QStringLiteral("CET4_1"));

        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.write("\n");
    }
    f.close();
    return path;
}

void TestServices::importJsonl() {
    // 账户
    qint64 id = 0;
    QVERIFY(auth->createAccount(QStringLiteral("tester"), QStringLiteral("pass123"), QString(), &id).isOk());
    userId = id;
    QVERIFY(userId > 0);
    QVERIFY(auth->accountExists(QStringLiteral("tester")));
    QVERIFY(!auth->createAccount(QStringLiteral("tester"), QStringLiteral("x"), QString()).isOk());

    // 登录校验
    auto ok = auth->login(QStringLiteral("tester"), QStringLiteral("pass123"));
    QVERIFY(ok);
    auto bad = auth->login(QStringLiteral("tester"), QStringLiteral("wrong"));
    QVERIFY(bad.isErr());

    // 导入
    const QString path = writeSampleJsonl(8);
    QVERIFY(!path.isEmpty());
    JsonlImporter importer(*words);
    const auto stats = importer.importFile(path, false);
    QCOMPARE(stats.inserted, 8);
    QCOMPARE(stats.failed, 0);
    QCOMPARE(wordService->count(), 8LL);

    // 再次导入应为更新
    const auto stats2 = importer.importFile(path, false);
    QCOMPARE(stats2.updated, 8);

    // 带进度回调（与界面导入相同的形式）也应正常，且不产生重入问题
    int callbackCalls = 0;
    const auto stats3 = importer.importFile(path, false, [&](int, int) {
        ++callbackCalls;
        QCoreApplication::processEvents();
    });
    QCOMPARE(stats3.updated, 8);
    QVERIFY(callbackCalls >= 1);
}

void TestServices::paperFlowAndWrongBook() {
    // 全部加入默认单词本（单词本 = 收藏夹）
    const qint64 bookId = auth->defaultBookId(userId);
    const auto all = words->search(WordQuery{});
    QCOMPARE(all.size(), 8);
    for (const auto &w : all) QVERIFY(bookService->addWord(bookId, w.id, EntrySource::Manual).isOk());
    QCOMPARE(bookService->count(bookId), 8LL);

    // 生成题表
    Setting s = settingService->get(userId);
    auto paper = questionService->generatePaper(userId, PaperMode::WordBookOnly, 5, s.questionTypes);
    QVERIFY(paper);
    const qint64 paperId = paper.value().id;
    QVERIFY(paperId > 0);

    auto entries = questionService->entries(paperId);
    QVERIFY(!entries.isEmpty());

    // 第 1 题答对，第 2 题答错
    int idx = 0;
    for (const auto &e : entries) {
        const QString answer = (idx == 0) ? e.question.answer : QStringLiteral("###wrong###");
        auto res = questionService->submitAnswer(e.id, answer, 1000);
        QVERIFY(res);
        QCOMPARE(res.value().correct, idx == 0);
        ++idx;
    }
    QVERIFY(questionService->finishPaper(paperId).isOk());

    const PaperStat stat = questionService->stat(paperId);
    QCOMPARE(stat.total, entries.size());
    QCOMPARE(stat.correct, 1);
    QCOMPARE(stat.wrong, entries.size() - 1);

    // 错题本应自动记录答错的题
    QCOMPARE(wrongService->count(userId), entries.size() - 1);
    const auto wrongList = wrongService->list(userId, WordQuery{});
    QCOMPARE(wrongList.size(), entries.size() - 1);

    // 移除一条错题
    QVERIFY(wrongService->remove(wrongList.first().id).isOk());
    QCOMPARE(wrongService->count(userId), entries.size() - 2);
}

void TestServices::syncRoundTrip() {
    const QString path = m_dir.filePath(QStringLiteral("sync.json"));
    auto exp = syncService->exportTo(path, userId);
    QVERIFY(exp);
    QVERIFY(QFile::exists(path));

    const int before = static_cast<int>(wrongService->count(userId));

    // 导入同一文件（无冲突的新增场景下应保持稳定）
    auto imp = syncService->importFrom(path, userId, SyncService::ConflictPolicy::PreferNewer);
    QVERIFY(imp);
    QVERIFY(imp.value().added >= 0);

    // 日志应有导出 + 导入两条
    const auto logs = syncService->logs(userId);
    QVERIFY(logs.size() >= 2);
    QCOMPARE(before, static_cast<int>(wrongService->count(userId)));
}

// 背诵计划：按天数 / 每天新词 / 每天复习生成每日词表，并在作答后更新计划进度
void TestServices::studyPlanFlow() {
    StudyPlan plan;
    plan.userId = userId;
    plan.name = QStringLiteral("测试计划");
    plan.book = QString();       // 全部词书
    plan.totalDays = 3;
    plan.newPerDay = 5;
    plan.reviewPerDay = 5;
    plan.includeWrong = false;   // 排除错题本以保证可预期
    const auto saved = dailyService->savePlan(plan);
    QVERIFY(saved);
    QVERIFY(saved.value().id > 0);
    QCOMPARE(saved.value().totalDays, 3);

    auto list = dailyService->generateToday(userId, true);
    QVERIFY(list);
    QCOMPARE(list.value().totalCount, 5); // 第一天：5 个新词，0 复习

    const auto entries = dailyService->entries(list.value().id);
    QCOMPARE(entries.size(), 5);
    for (const auto &e : entries) QCOMPARE(e.sourceType, EntrySource::New);

    // 新词已纳入计划
    QCOMPARE(plans->countInPlan(saved.value().id), 5LL);
    QCOMPARE(plans->countLearned(saved.value().id), 0LL);

    // 同日再次生成应返回同一词表
    auto again = dailyService->generateToday(userId, false);
    QVERIFY(again);
    QCOMPARE(again.value().id, list.value().id);

    // 完成状态
    QVERIFY(dailyService->markEntryCompleted(entries.first().id, true).isOk());
    QVERIFY(dailyService->completeList(list.value().id, false).isOk());

    // 做一题后，计划进度应更新为已学
    const Setting s = settingService->get(userId);
    QList<qint64> ids;
    for (const auto &e : entries) ids << e.wordId;
    auto paper = questionService->generatePaperFromWords(userId, ids, s.questionTypes,
                                                         QStringLiteral("计划练习"));
    QVERIFY(paper);
    const auto pes = questionService->entries(paper.value().id);
    QVERIFY(!pes.isEmpty());
    auto ans = questionService->submitAnswer(pes.first().id, pes.first().question.answer, 100);
    QVERIFY(ans);
    QVERIFY(ans.value().correct);

    const auto pw = plans->planWordsForWord(userId, pes.first().question.wordId);
    QVERIFY(!pw.isEmpty());
    QVERIFY(pw.first().learned);
    QCOMPARE(plans->countLearned(saved.value().id), 1LL);

    // 进度信息
    const auto pr = dailyService->progress(userId);
    QCOMPARE(pr.totalDays, 3);
    QCOMPARE(pr.todayNew, 5);
    QCOMPARE(pr.learned, 1LL);
    QCOMPARE(pr.totalWords, 8LL);
}

// 单词本 = 可多建的收藏夹，手动添加；与词库分离
void TestServices::multiWordBook() {
    qint64 uid = 0;
    QVERIFY(auth->createAccount(QStringLiteral("collector"), QStringLiteral("pass123"), QString(), &uid).isOk());
    QCOMPARE(bookService->listBooks(uid).size(), 1); // 默认单词本

    auto b1 = bookService->createBook(uid, QStringLiteral("考研核心"));
    QVERIFY(b1);
    auto b2 = bookService->createBook(uid, QStringLiteral("写作常用"));
    QVERIFY(b2);
    QVERIFY(bookService->createBook(uid, QStringLiteral("考研核心")).isErr()); // 重名
    QCOMPARE(bookService->listBooks(uid).size(), 3);

    const auto all = words->search(WordQuery{});
    QVERIFY(all.size() >= 2);
    QVERIFY(bookService->addWord(b1.value().id, all[0].id, EntrySource::Manual).isOk());
    QVERIFY(bookService->addWord(b1.value().id, all[1].id, EntrySource::Manual).isOk());
    QVERIFY(bookService->addWord(b2.value().id, all[0].id, EntrySource::Manual).isOk());
    QVERIFY(bookService->addWord(b1.value().id, all[0].id).isErr()); // 重复添加

    QCOMPARE(bookService->count(b1.value().id), 2LL);
    QCOMPARE(bookService->count(b2.value().id), 1LL);
    QCOMPARE(bookService->totalEntries(uid), 3LL);
    QCOMPARE(bookService->booksContaining(uid, all[0].id).size(), 2);

    // 重命名 / 删除
    QVERIFY(bookService->renameBook(b2.value().id, QStringLiteral("写作高级")).isOk());
    QVERIFY(bookService->removeBook(b2.value().id).isOk());
    QCOMPARE(bookService->totalEntries(uid), 2LL);
    QCOMPARE(bookService->listBooks(uid).size(), 2);

    // 查询字典（词库）与单词本相互独立
    QCOMPARE(wordService->count(), 8LL);
    QCOMPARE(bookService->count(b1.value().id), 2LL);
}

// 每日词表是否覆盖全部词库：反复取新词应能取完；且未作答的单词会继续作为复习出现
void TestServices::planCoversWholeVocabulary() {
    qint64 uid = 0;
    QVERIFY(auth->createAccount(QStringLiteral("coverage"), QStringLiteral("pass123"), QString(), &uid).isOk());

    StudyPlan plan;
    plan.userId = uid;
    plan.book = QString();
    plan.totalDays = 2;      // 故意设置得远小于所需天数
    plan.newPerDay = 3;
    plan.reviewPerDay = 100;
    plan.includeWrong = false;
    const auto saved = dailyService->savePlan(plan);
    QVERIFY(saved);
    const qint64 planId = saved.value().id;

    const qint64 total = wordService->count();
    QVERIFY(total > 0);

    // 第一批新词：未作答（learned=0）
    const auto batch1 = plans->nextNewWordIds(planId, QString(), 3);
    QCOMPARE(batch1.size(), 3);
    QVERIFY(plans->addPlanWords(planId, batch1, QStringLiteral("2026-01-01")));
    QCOMPARE(plans->countLearned(planId), 0LL);

    // 关键回归：已排入但未作答的单词，必须重新出现在复习列表中，否则会永久漏背
    const auto due = plans->dueReviewWordIds(planId, QDateTime::currentDateTime(), 100);
    for (auto id : batch1) QVERIFY2(due.contains(id), "未作答的计划单词未回到复习列表");

    // 反复取新词直到取完 —— 证明覆盖全部词库
    qint64 seen = batch1.size();
    int guard = 0;
    while (guard++ < 1000) {
        const auto ids = plans->nextNewWordIds(planId, QString(), 3);
        if (ids.isEmpty()) break;
        seen += ids.size();
        QVERIFY(plans->addPlanWords(planId, ids, QStringLiteral("2026-01-01")));
    }
    QCOMPARE(seen, total);
    QCOMPARE(plans->countInPlan(planId), total);
    QVERIFY(plans->nextNewWordIds(planId, QString(), 3).isEmpty()); // 再无新词

    // 计划页据此给出覆盖提示：2 天 × 3 词/天 远不能覆盖全部词库
    const auto pr = dailyService->progress(uid);
    QCOMPARE(pr.totalDays, 2);
    QCOMPARE(pr.totalWords, total);
    QVERIFY(pr.neededDays > pr.totalDays);
    QVERIFY(!pr.coversAll);
}

// 题目生成不会产生错题；只有答错才会
void TestServices::questionGenerationCreatesNoWrongEntries() {
    qint64 uid = 0;
    QVERIFY(auth->createAccount(QStringLiteral("quizzer"), QStringLiteral("pass123"), QString(), &uid).isOk());
    const qint64 before = wrongService->count(uid);
    QCOMPARE(before, 0LL);

    const Setting s = settingService->get(uid);
    auto paper = questionService->generatePaper(uid, PaperMode::Random, 5, s.questionTypes);
    QVERIFY(paper);

    // 仅生成题表/题目，不应产生任何错题
    QCOMPARE(wrongService->count(uid), before);
    const auto entries = questionService->entries(paper.value().id);
    QVERIFY(!entries.isEmpty());
    QCOMPARE(wrongService->count(uid), before);

    // 答错之后才产生错题
    auto res = questionService->submitAnswer(entries.first().id, QStringLiteral("###wrong###"), 100);
    QVERIFY(res);
    QVERIFY(!res.value().correct);
    QCOMPARE(wrongService->count(uid), before + 1);

    // 答对不会新增错题
    const qint64 after = wrongService->count(uid);
    QVERIFY(entries.size() > 1);
    auto ok = questionService->submitAnswer(entries[1].id, entries[1].question.answer, 100);
    QVERIFY(ok);
    QVERIFY(ok.value().correct);
    QCOMPARE(wrongService->count(uid), after);
}

// 修改密码 + “记住登录状态”令牌
void TestServices::passwordChangeAndRememberToken() {
    qint64 uid = 0;
    QVERIFY(auth->createAccount(QStringLiteral("pwduser"), QStringLiteral("oldpass"), QString(), &uid).isOk());
    const auto u = users->findById(uid);
    QVERIFY(u.has_value());
    const QString token = AuthService::rememberToken(*u);
    QVERIFY(!token.isEmpty());

    // 令牌可自动登录；伪造令牌失败
    auto autoLogin = auth->loginWithRememberToken(uid, token);
    QVERIFY(autoLogin);
    QCOMPARE(autoLogin.value().id, uid);
    QVERIFY(auth->loginWithRememberToken(uid, QStringLiteral("bogus")).isErr());

    // 原密码错误 → 拒绝
    QVERIFY(auth->changePassword(uid, QStringLiteral("wrong"), QStringLiteral("newpass")).isErr());
    // 新密码过短 → 拒绝
    QVERIFY(auth->changePassword(uid, QStringLiteral("oldpass"), QStringLiteral("12")).isErr());
    // 正常修改
    QVERIFY(auth->changePassword(uid, QStringLiteral("oldpass"), QStringLiteral("newpass")).isOk());
    QVERIFY(auth->login(QStringLiteral("pwduser"), QStringLiteral("newpass")));
    QVERIFY(auth->login(QStringLiteral("pwduser"), QStringLiteral("oldpass")).isErr());

    // 密码变更后旧令牌失效
    QVERIFY(auth->loginWithRememberToken(uid, token).isErr());
    const auto u2 = users->findById(uid);
    QVERIFY(auth->loginWithRememberToken(uid, AuthService::rememberToken(*u2)));
}

// 多个自定义背诵计划：新建 / 选择 / 用所选计划生成 / 删除
void TestServices::multiplePlans() {
    qint64 uid = 0;
    QVERIFY(auth->createAccount(QStringLiteral("planner"), QStringLiteral("pass123"), QString(), &uid).isOk());

    StudyPlan pa;
    pa.userId = uid;
    pa.name = QStringLiteral("计划A");
    pa.totalDays = 10;
    pa.newPerDay = 2;
    pa.reviewPerDay = 1;
    pa.includeWrong = false;
    const auto sa = dailyService->savePlan(pa);
    QVERIFY(sa);

    StudyPlan pb;
    pb.userId = uid;
    pb.name = QStringLiteral("计划B");
    pb.totalDays = 20;
    pb.newPerDay = 3;
    pb.reviewPerDay = 2;
    pb.includeWrong = false;
    const auto sb = dailyService->savePlan(pb);
    QVERIFY(sb);

    QVERIFY(sa.value().id != sb.value().id);            // 新建而不是覆盖
    QCOMPARE(dailyService->listPlans(uid).size(), 2);   // 两个计划并存

    // 保存后当前计划为 B
    QCOMPARE(dailyService->activePlan(uid).value().id, sb.value().id);

    // 切换到 A，并用 A 生成今日词表（A：每天 2 个新词）
    QVERIFY(dailyService->setActivePlan(uid, sa.value().id).isOk());
    QCOMPARE(dailyService->activePlan(uid).value().id, sa.value().id);
    auto list = dailyService->generateToday(uid, true);
    QVERIFY(list);
    QCOMPARE(list.value().totalCount, 2);
    QCOMPARE(dailyService->entries(list.value().id).size(), 2);

    // 删除当前计划后自动启用剩余计划
    QVERIFY(dailyService->removePlan(uid, sa.value().id).isOk());
    QCOMPARE(dailyService->listPlans(uid).size(), 1);
    QCOMPARE(dailyService->activePlan(uid).value().id, sb.value().id);
}

// 任何生成结果都不能是 0 长度（词表 / 题表）
void TestServices::noEmptyDailyListOrPaper() {
    qint64 uid = 0;
    QVERIFY(auth->createAccount(QStringLiteral("guard"), QStringLiteral("pass123"), QString(), &uid).isOk());
    const QString today = util::todayDate();

    // 1) 没有计划 → 失败，且不产生词表
    QVERIFY(dailyService->generateToday(uid, true).isErr());
    QVERIFY(!daily->findByDate(uid, today).has_value());

    // 2) 只复习不背新词、且无到期复习 → 仍失败，不产生 0 长度词表
    StudyPlan p;
    p.userId = uid;
    p.name = QStringLiteral("只复习");
    p.totalDays = 7;
    p.newPerDay = 0;
    p.reviewPerDay = 5;
    p.includeWrong = false;
    QVERIFY(dailyService->savePlan(p));
    QVERIFY(dailyService->generateToday(uid, true).isErr());
    QVERIFY(!daily->findByDate(uid, today).has_value());

    // 3) 题目信息不足时，不残留 0 长度的题表
    Word blank;
    blank.book = BookType::Custom;
    blank.spelling = QStringLiteral("zzzblankword");
    const qint64 wid = words->insert(blank);
    QVERIFY(wid > 0);
    const int before = papers->listPapers(uid, 1000).size();
    auto paper = questionService->generatePaperFromWords(uid, {wid}, {});
    QVERIFY(paper.isErr());
    QCOMPARE(papers->listPapers(uid, 1000).size(), before);
    words->remove(wid);
}

QTEST_APPLESS_MAIN(TestServices)
#include "test_services.moc"