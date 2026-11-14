#include "importer/JsonlImporter.h"

#include "core/Logger.h"
#include "core/Utils.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace vt {

BookType JsonlImporter::inferBook(const QString &hint) {
    const QString h = hint.toLower();
    if (h.contains(QStringLiteral("cet6")) || h.contains(QStringLiteral("六级")) ||
        h.contains(QStringLiteral("6")))
        return BookType::CET6;
    return BookType::CET4;
}

namespace {
QString jstr(const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString(); }

QString difficultyLess(const QString &s) { return util::stripHtml(s).trimmed(); }

// 合并同一单词的多个词条（同一 headWord 可能有多条记录）
void mergeWord(Word &dst, const Word &src) {
    if (dst.spelling.isEmpty()) dst.spelling = src.spelling;
    if (dst.wordRank == 0) dst.wordRank = src.wordRank;
    if (dst.phoneticUk.isEmpty()) dst.phoneticUk = src.phoneticUk;
    if (dst.phoneticUs.isEmpty()) dst.phoneticUs = src.phoneticUs;
    if (dst.audioUk.isEmpty()) dst.audioUk = src.audioUk;
    if (dst.audioUs.isEmpty()) dst.audioUs = src.audioUs;
    if (dst.memMethod.isEmpty()) dst.memMethod = src.memMethod;
    if (dst.source.isEmpty()) dst.source = src.source;
    dst.star = std::max(dst.star, src.star);

    auto hasTranslation = [&](const Translation &t) {
        for (const auto &x : dst.translations)
            if (x.pos == t.pos && x.tranCn == t.tranCn) return true;
        return false;
    };
    for (const auto &t : src.translations)
        if (!t.tranCn.isEmpty() && !hasTranslation(t)) dst.translations << t;

    auto hasPhrase = [&](const Phrase &p) {
        for (const auto &x : dst.phrases) if (x.content == p.content) return true;
        return false;
    };
    for (const auto &p : src.phrases)
        if (!hasPhrase(p)) dst.phrases << p;

    auto hasSentence = [&](const Sentence &s) {
        for (const auto &x : dst.sentences) if (x.en == s.en) return true;
        return false;
    };
    for (const auto &s : src.sentences)
        if (!s.en.isEmpty() && !hasSentence(s)) dst.sentences << s;

    for (const auto &syn : src.synonyms) {
        bool merged = false;
        for (auto &x : dst.synonyms) {
            if (x.pos == syn.pos && x.tran == syn.tran) {
                for (const auto &w : syn.words)
                    if (!x.words.contains(w)) x.words << w;
                merged = true;
                break;
            }
        }
        if (!merged) dst.synonyms << syn;
    }

    for (const auto &r : src.relWords) {
        bool merged = false;
        for (auto &x : dst.relWords) {
            if (x.pos == r.pos) {
                for (const auto &p : r.words) {
                    bool found = false;
                    for (const auto &q : x.words) if (q.first == p.first) { found = true; break; }
                    if (!found) x.words << p;
                }
                merged = true;
                break;
            }
        }
        if (!merged) dst.relWords << r;
    }

    for (const auto &e : src.exams) {
        bool dup = false;
        for (const auto &x : dst.exams) if (x.question == e.question) { dup = true; break; }
        if (!dup) dst.exams << e;
    }
}
} // namespace

Word JsonlImporter::parseLine(const QByteArray &line, BookType book, bool *ok) {
    Word w;
    if (ok) *ok = false;

    QByteArray data = line;
    if (data.startsWith("\xEF\xBB\xBF")) data.remove(0, 3); // 去 BOM
    if (data.trimmed().isEmpty()) return w;

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return w;

    const QJsonObject root = doc.object();
    const QJsonObject wordObj = root.value(QStringLiteral("content")).toObject()
                                   .value(QStringLiteral("word")).toObject();
    if (wordObj.isEmpty()) return w;

    const QString head = wordObj.value(QStringLiteral("wordHead")).toString();
    if (head.trimmed().isEmpty()) return w;

    const QString bookId = root.value(QStringLiteral("bookId")).toString();
    if (bookId.contains(QStringLiteral("CET6"), Qt::CaseInsensitive)) book = BookType::CET6;
    else if (bookId.contains(QStringLiteral("CET4"), Qt::CaseInsensitive)) book = BookType::CET4;

    w.book = book;
    w.spelling = head.trimmed();
    w.wordRank = root.value(QStringLiteral("wordRank")).toInt();
    w.source = bookId;

    const QJsonObject c = wordObj.value(QStringLiteral("content")).toObject();

    w.phoneticUk = jstr(c, "ukphone");
    w.phoneticUs = jstr(c, "usphone");
    w.audioUk = jstr(c, "ukspeech");
    w.audioUs = jstr(c, "usspeech");
    w.star = c.value(QStringLiteral("star")).toInt();

    // 记忆方法
    const QJsonObject rem = c.value(QStringLiteral("remMethod")).toObject();
    if (!rem.isEmpty()) w.memMethod = rem.value(QStringLiteral("val")).toString();

    // 释义 trans[]
    for (const auto &tv : c.value(QStringLiteral("trans")).toArray()) {
        const auto to = tv.toObject();
        Translation t;
        t.pos = to.value(QStringLiteral("pos")).toString();
        t.tranCn = to.value(QStringLiteral("tranCn")).toString();
        t.tranEn = to.value(QStringLiteral("tranOther")).toString();
        if (!t.tranCn.isEmpty() || !t.tranEn.isEmpty()) w.translations << t;
    }

    // 例句
    const QJsonObject sentObj = c.value(QStringLiteral("sentence")).toObject();
    for (const auto &sv : sentObj.value(QStringLiteral("sentences")).toArray()) {
        const auto so = sv.toObject();
        Sentence s;
        s.en = difficultyLess(so.value(QStringLiteral("sContent")).toString());
        s.cn = so.value(QStringLiteral("sCn")).toString();
        s.audio = so.value(QStringLiteral("sSpeech")).toString();
        s.tag = QStringLiteral("例句");
        if (!s.en.isEmpty()) w.sentences << s;
    }
    // 真题例句
    const QJsonObject realObj = c.value(QStringLiteral("realExamSentence")).toObject();
    for (const auto &sv : realObj.value(QStringLiteral("sentences")).toArray()) {
        const auto so = sv.toObject();
        Sentence s;
        s.en = difficultyLess(so.value(QStringLiteral("sContent")).toString());
        const auto info = so.value(QStringLiteral("sourceInfo")).toObject();
        s.cn = QStringLiteral("%1 %2 %3").arg(jstr(info, "level"), jstr(info, "year"), jstr(info, "type")).trimmed();
        s.tag = QStringLiteral("真题例句");
        if (!s.en.isEmpty()) w.sentences << s;
    }

    // 短语
    const QJsonObject phraseObj = c.value(QStringLiteral("phrase")).toObject();
    for (const auto &pv : phraseObj.value(QStringLiteral("phrases")).toArray()) {
        const auto po = pv.toObject();
        Phrase p;
        p.content = po.value(QStringLiteral("pContent")).toString();
        p.meaning = po.value(QStringLiteral("pCn")).toString();
        if (!p.content.isEmpty()) w.phrases << p;
    }

    // 同近义词
    const QJsonObject synoObj = c.value(QStringLiteral("syno")).toObject();
    for (const auto &sv : synoObj.value(QStringLiteral("synos")).toArray()) {
        const auto so = sv.toObject();
        Synonym syn;
        syn.pos = so.value(QStringLiteral("pos")).toString();
        syn.tran = so.value(QStringLiteral("tran")).toString();
        for (const auto &hv : so.value(QStringLiteral("hwds")).toArray())
            syn.words << hv.toObject().value(QStringLiteral("w")).toString();
        if (!syn.words.isEmpty() || !syn.tran.isEmpty()) w.synonyms << syn;
    }

    // 同根词
    const QJsonObject relObj = c.value(QStringLiteral("relWord")).toObject();
    for (const auto &rv : relObj.value(QStringLiteral("rels")).toArray()) {
        const auto ro = rv.toObject();
        RelWord r;
        r.pos = ro.value(QStringLiteral("pos")).toString();
        for (const auto &wv : ro.value(QStringLiteral("words")).toArray()) {
            const auto wo = wv.toObject();
            r.words << qMakePair(wo.value(QStringLiteral("hwd")).toString(),
                                 wo.value(QStringLiteral("tran")).toString());
        }
        if (!r.words.isEmpty()) w.relWords << r;
    }

    // 真题考题 exam[]
    for (const auto &ev : c.value(QStringLiteral("exam")).toArray()) {
        const auto eo = ev.toObject();
        ExamItem e;
        e.question = eo.value(QStringLiteral("question")).toString();
        for (const auto &cv : eo.value(QStringLiteral("choices")).toArray())
            e.choices << cv.toObject().value(QStringLiteral("choice")).toString();
        const auto ans = eo.value(QStringLiteral("answer")).toObject();
        e.rightIndex = ans.value(QStringLiteral("rightIndex")).toInt();
        e.explain = ans.value(QStringLiteral("explain")).toString();
        if (!e.question.isEmpty()) w.exams << e;
    }

    if (ok) *ok = true;
    return w;
}

ImportStats JsonlImporter::importFile(const QString &path, bool replaceExisting,
                                      const std::function<void(int, int)> &progress) {
    ImportStats stats;
    QElapsedTimer timer;
    timer.start();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        Logger::error(QStringLiteral("Import"), QStringLiteral("无法打开文件: %1").arg(path));
        stats.failed = -1;
        return stats;
    }

    const BookType hint = inferBook(QFileInfo(path).fileName());

    // 统计总行数用于进度
    int totalLines = 0;
    {
        while (!f.atEnd()) {
            f.readLine();
            ++totalLines;
        }
        f.seek(0);
    }
    if (totalLines > 0 && progress) progress(0, totalLines);

    SqliteDb &db = m_repo.database().db();
    if (replaceExisting) m_repo.removeByBook(toKey(hint));

    // 同一拼写可能有多条记录，先在内存中合并，再统一写库
    QHash<QString, int> indexByKey;
    QList<Word> merged;
    merged.reserve(totalLines);

    int processed = 0;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine();
        if (line.trimmed().isEmpty()) continue;
        bool ok = false;
        Word w = parseLine(line, hint, &ok);
        ++processed;
        if (!ok) {
            ++stats.failed;
            continue;
        }
        const QString key = toKey(w.book) + QLatin1Char('|') + w.spelling.toLower();
        const auto it = indexByKey.constFind(key);
        if (it != indexByKey.constEnd()) {
            mergeWord(merged[*it], w);
        } else {
            indexByKey.insert(key, merged.size());
            merged << w;
        }
        if (progress && (processed % 500 == 0 || processed == totalLines))
            progress(processed, totalLines);
    }
    f.close();

    // 单事务写入，显著提升导入速度
    // 预先载入已有拼写，避免逐词查询整行数据
    QSet<QString> existingSpellings;
    {
        Stmt st(db, QStringLiteral("SELECT spelling FROM words WHERE book=?"));
        st.bind(1, toKey(hint));
        if (st.isValid())
            while (st.step()) existingSpellings.insert(st.colText(0).toLower());
    }

    const bool inTx = db.begin();
    for (const Word &w : merged) {
        const bool existed = existingSpellings.contains(w.spelling.toLower());
        const qint64 id = m_repo.upsert(w);
        if (id < 0) ++stats.failed;
        else if (existed) ++stats.updated;
        else ++stats.inserted;
        existingSpellings.insert(w.spelling.toLower());
    }
    if (inTx) db.commit();

    stats.elapsedMs = timer.elapsed();
    Logger::info(QStringLiteral("Import"),
                 QStringLiteral("导入完成 %1: 词条 %2, 新增 %3, 更新 %4, 失败 %5, 用时 %6ms")
                     .arg(QFileInfo(path).fileName())
                     .arg(merged.size())
                     .arg(stats.inserted).arg(stats.updated).arg(stats.failed).arg(stats.elapsedMs));
    return stats;
}

ImportStats JsonlImporter::importDirectory(const QString &dir, bool replaceExisting) {
    ImportStats total;
    QDir d(dir);
    const auto files = d.entryInfoList({QStringLiteral("*.jsonl")}, QDir::Files, QDir::Name);
    for (const auto &fi : files) {
        ImportStats s = importFile(fi.absoluteFilePath(), replaceExisting);
        total.inserted += s.inserted;
        total.updated += s.updated;
        total.skipped += s.skipped;
        total.failed += s.failed;
        total.elapsedMs += s.elapsedMs;
    }
    return total;
}

} // namespace vt
