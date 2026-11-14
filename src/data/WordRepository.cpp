#include "core/Logger.h"
#include "core/Utils.h"
#include "data/Repositories.h"
#include "data/WordMapper.h"

#include <QSet>

#include <algorithm>

namespace vt {

namespace {
// 计算形近候选的排序分值
struct SimilarCandidate {
    qint64 id;
    double score;
};
} // namespace

qint64 WordRepository::insert(const Word &w) {
    Stmt st(m_db.db(),
            QStringLiteral("INSERT INTO words (book, word_rank, spelling, phonetic_uk, phonetic_us, "
                           "audio_uk, audio_us, star, pos, translation_cn, translation_en, mem_method, "
                           "source, phrases_json, sentences_json, synos_json, relwords_json, exam_json, created_at) "
                           "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    if (!st.isValid()) return -1;
    const QString now = util::nowIso();
    const QString pos = w.translations.isEmpty() ? QString() : w.translations.first().pos;
    const QString cn = w.translations.isEmpty() ? QString() : w.translations.first().tranCn;
    const QString en = w.translations.isEmpty() ? QString() : w.translations.first().tranEn;
    st.bind(1, toKey(w.book)).bind(2, w.wordRank).bind(3, w.spelling)
        .bind(4, w.phoneticUk).bind(5, w.phoneticUs).bind(6, w.audioUk).bind(7, w.audioUs)
        .bind(8, w.star).bind(9, pos).bind(10, cn).bind(11, en).bind(12, w.memMethod)
        .bind(13, w.source)
        .bind(14, mapper::phrasesToJson(w.phrases))
        .bind(15, mapper::sentencesToJson(w.sentences))
        .bind(16, mapper::synonymsToJson(w.synonyms))
        .bind(17, mapper::relWordsToJson(w.relWords))
        .bind(18, mapper::examsToJson(w.exams))
        .bind(19, now);
    if (!st.run()) return -1;
    return m_db.db().lastInsertId();
}

bool WordRepository::update(const Word &w) {
    Stmt st(m_db.db(),
            QStringLiteral("UPDATE words SET book=?, word_rank=?, spelling=?, phonetic_uk=?, phonetic_us=?, "
                           "audio_uk=?, audio_us=?, star=?, pos=?, translation_cn=?, translation_en=?, "
                           "mem_method=?, source=?, phrases_json=?, sentences_json=?, synos_json=?, "
                           "relwords_json=?, exam_json=? WHERE id=?"));
    if (!st.isValid()) return false;
    const QString pos = w.translations.isEmpty() ? QString() : w.translations.first().pos;
    const QString cn = w.translations.isEmpty() ? QString() : w.translations.first().tranCn;
    const QString en = w.translations.isEmpty() ? QString() : w.translations.first().tranEn;
    st.bind(1, toKey(w.book)).bind(2, w.wordRank).bind(3, w.spelling)
        .bind(4, w.phoneticUk).bind(5, w.phoneticUs).bind(6, w.audioUk).bind(7, w.audioUs)
        .bind(8, w.star).bind(9, pos).bind(10, cn).bind(11, en).bind(12, w.memMethod)
        .bind(13, w.source)
        .bind(14, mapper::phrasesToJson(w.phrases))
        .bind(15, mapper::sentencesToJson(w.sentences))
        .bind(16, mapper::synonymsToJson(w.synonyms))
        .bind(17, mapper::relWordsToJson(w.relWords))
        .bind(18, mapper::examsToJson(w.exams))
        .bind(19, w.id);
    return st.run();
}

qint64 WordRepository::upsert(const Word &w) {
    Stmt st(m_db.db(), QStringLiteral("SELECT id FROM words WHERE book=? AND spelling=?"));
    st.bind(1, toKey(w.book)).bind(2, w.spelling);
    if (st.isValid() && st.step()) {
        const qint64 id = st.colInt64(0);
        Word copy = w;
        copy.id = id;
        update(copy);
        return id;
    }
    return insert(w);
}

std::optional<Word> WordRepository::findById(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("SELECT %1 FROM words w WHERE w.id=?").arg(mapper::wordColumns()));
    st.bind(1, id);
    if (st.isValid() && st.step()) return mapper::wordFromStmt(st, 0);
    return std::nullopt;
}

std::optional<Word> WordRepository::findBySpelling(const QString &spelling, const QString &book) {
    QString sql = QStringLiteral("SELECT %1 FROM words w WHERE w.spelling=? COLLATE NOCASE").arg(mapper::wordColumns());
    if (!book.isEmpty()) sql += QStringLiteral(" AND w.book=?");
    sql += QStringLiteral(" LIMIT 1");
    Stmt st(m_db.db(), sql);
    st.bind(1, spelling.trimmed());
    if (!book.isEmpty()) st.bind(2, book);
    if (st.isValid() && st.step()) return mapper::wordFromStmt(st, 0);
    return std::nullopt;
}

QList<Word> WordRepository::search(const WordQuery &q, int *totalOut) {
    QString where = QStringLiteral(" WHERE 1=1");
    QList<QString> binds;
    if (!q.keyword.trimmed().isEmpty()) {
        where += QStringLiteral(" AND (w.spelling LIKE ? ESCAPE '\\' OR w.translation_cn LIKE ? ESCAPE '\\')");
        const QString kw = QStringLiteral("%") + q.keyword.trimmed() + QStringLiteral("%");
        binds << kw << kw;
    }
    if (!q.book.isEmpty()) {
        where += QStringLiteral(" AND w.book=?");
        binds << q.book;
    }
    if (q.minStar >= 0) {
        where += QStringLiteral(" AND w.star >= ?");
        binds << QString::number(q.minStar);
    }

    if (totalOut) {
        Stmt cnt(m_db.db(), QStringLiteral("SELECT COUNT(*) FROM words w") + where);
        int idx = 1;
        for (const auto &b : binds) cnt.bind(idx++, b);
        *totalOut = (cnt.isValid() && cnt.step()) ? cnt.colInt(0) : 0;
    }

    QString order = QStringLiteral("w.word_rank");
    if (q.orderBy == QLatin1String("spelling")) order = QStringLiteral("w.spelling COLLATE NOCASE");
    else if (q.orderBy == QLatin1String("rank_desc")) order = QStringLiteral("w.word_rank DESC");

    QString sql = QStringLiteral("SELECT %1 FROM words w").arg(mapper::wordColumns()) + where +
                  QStringLiteral(" ORDER BY ") + order +
                  QStringLiteral(" LIMIT ? OFFSET ?");
    Stmt st(m_db.db(), sql);
    int idx = 1;
    for (const auto &b : binds) st.bind(idx++, b);
    st.bind(idx++, q.limit).bind(idx++, q.offset);

    QList<Word> out;
    if (st.isValid()) {
        while (st.step()) out << mapper::wordFromStmt(st, 0);
    }
    return out;
}

QList<Word> WordRepository::suggest(const QString &keyword, const QString &book, int limit) {
    QList<Word> out;
    const QString kw = keyword.trimmed();
    if (kw.isEmpty() || limit <= 0) return out;

    auto runQuery = [&](const QString &sql, const QString &pattern, const QString &bookArg,
                        QList<Word> &target, int max) {
        Stmt st(m_db.db(), sql);
        if (!st.isValid()) return;
        int idx = 1;
        st.bind(idx++, pattern);
        if (!bookArg.isEmpty()) st.bind(idx++, bookArg);
        st.bind(idx++, max);
        while (st.step()) target << mapper::wordFromStmt(st, 0);
    };

    const QString cols = mapper::wordColumns();
    QList<Word> candidates;

    // 1) 前缀匹配（最可能）
    QString sqlPrefix = QStringLiteral("SELECT %1 FROM words w WHERE w.spelling LIKE ? ESCAPE '\\'")
                            .arg(cols);
    if (!book.isEmpty()) sqlPrefix += QStringLiteral(" AND w.book=?");
    sqlPrefix += QStringLiteral(" ORDER BY LENGTH(w.spelling) ASC, w.word_rank ASC LIMIT ?");
    QString escapedKw = kw;
    escapedKw.replace(QLatin1Char('%'), QStringLiteral("\\%")).replace(QLatin1Char('_'), QStringLiteral("\\_"));
    runQuery(sqlPrefix, escapedKw + QStringLiteral("%"), book, candidates, 20);

    // 2) 包含匹配
    if (candidates.size() < limit) {
        QString sqlContains = QStringLiteral("SELECT %1 FROM words w WHERE w.spelling LIKE ? ESCAPE '\\'")
                                  .arg(cols);
        if (!book.isEmpty()) sqlContains += QStringLiteral(" AND w.book=?");
        sqlContains += QStringLiteral(" ORDER BY LENGTH(w.spelling) ASC, w.word_rank ASC LIMIT ?");
        runQuery(sqlContains, QStringLiteral("%") + escapedKw + QStringLiteral("%"), book, candidates, 20);
    }

    // 3) 中文释义匹配
    if (candidates.size() < limit) {
        QString sqlCn = QStringLiteral("SELECT %1 FROM words w WHERE w.translation_cn LIKE ? ESCAPE '\\'")
                            .arg(cols);
        if (!book.isEmpty()) sqlCn += QStringLiteral(" AND w.book=?");
        sqlCn += QStringLiteral(" ORDER BY w.word_rank ASC LIMIT ?");
        runQuery(sqlCn, QStringLiteral("%") + escapedKw + QStringLiteral("%"), book, candidates, 20);
    }

    // 去重
    QList<Word> unique;
    QSet<qint64> seen;
    for (const auto &w : candidates) {
        if (seen.contains(w.id)) continue;
        seen.insert(w.id);
        unique << w;
    }

    // 打分排序：完全相等 > 前缀 > 包含 > 拼写相近 > 中文
    const QString lowKw = kw.toLower();
    struct Scored { double score; Word word; };
    QList<Scored> scored;
    for (const auto &w : unique) {
        const QString sp = w.spelling.toLower();
        double s = 0.0;
        if (sp == lowKw) s = 1000.0;
        else if (sp.startsWith(lowKw)) s = 900.0 - sp.size();
        else if (sp.contains(lowKw)) s = 700.0 - sp.size();
        else {
            const double sim = util::similarity(sp, lowKw);
            const bool cnHit = w.allChinese().contains(kw, Qt::CaseInsensitive);
            if (sim >= 0.6) s = 500.0 + sim * 100.0;
            else if (cnHit) s = 400.0;
            else if (sp.startsWith(lowKw.left(1))) s = 300.0;
            else s = 100.0;
        }
        if (w.wordRank > 0) s += 10.0 / (1.0 + w.wordRank / 1000.0);
        scored.append({s, w});
    }
    std::sort(scored.begin(), scored.end(),
              [](const Scored &a, const Scored &b) { return a.score > b.score; });
    for (int i = 0; i < scored.size() && out.size() < limit; ++i) out << scored[i].word;
    return out;
}

QList<Word> WordRepository::findByIds(const QList<qint64> &ids) {    QList<Word> out;
    if (ids.isEmpty()) return out;
    QStringList placeholders;
    for (int i = 0; i < ids.size(); ++i) placeholders << QStringLiteral("?");
    const QString sql = QStringLiteral("SELECT %1 FROM words w WHERE w.id IN (%2)")
                            .arg(mapper::wordColumns(), placeholders.join(QStringLiteral(",")));
    Stmt st(m_db.db(), sql);
    if (!st.isValid()) return out;
    for (int i = 0; i < ids.size(); ++i) st.bind(i + 1, ids[i]);
    while (st.step()) out << mapper::wordFromStmt(st, 0);
    return out;
}

QList<Word> WordRepository::randomByBook(const QString &book, int n, const QList<qint64> &excludeIds) {
    QList<Word> out;
    if (n <= 0) return out;
    QString sql = QStringLiteral("SELECT %1 FROM words w").arg(mapper::wordColumns());
    QStringList conds;
    if (!book.isEmpty()) conds << QStringLiteral("w.book=?");
    if (!excludeIds.isEmpty()) {
        QStringList ph;
        for (int i = 0; i < excludeIds.size(); ++i) ph << QStringLiteral("?");
        conds << QStringLiteral("w.id NOT IN (%1)").arg(ph.join(QStringLiteral(",")));
    }
    if (!conds.isEmpty()) sql += QStringLiteral(" WHERE ") + conds.join(QStringLiteral(" AND "));
    sql += QStringLiteral(" ORDER BY RANDOM() LIMIT ?");
    Stmt st(m_db.db(), sql);
    if (!st.isValid()) return out;
    int idx = 1;
    if (!book.isEmpty()) st.bind(idx++, book);
    for (auto id : excludeIds) st.bind(idx++, id);
    st.bind(idx++, n);
    while (st.step()) out << mapper::wordFromStmt(st, 0);
    return out;
}

QList<Word> WordRepository::findSimilar(const QString &spelling, const QString &book, int n) {
    QList<Word> out;
    if (spelling.trimmed().isEmpty()) return out;
    // 用前缀 + 长度相近做粗筛，再在内存中按相似度排序
    const QString s = util::normalizeSpelling(spelling);
    const QString head = s.left(std::min<int>(3, s.size()));
    QString sql = QStringLiteral("SELECT %1 FROM words w WHERE w.spelling LIKE ? AND w.spelling <> ?")
                      .arg(mapper::wordColumns());
    if (!book.isEmpty()) sql += QStringLiteral(" AND w.book=?");
    sql += QStringLiteral(" LIMIT 400");
    Stmt st(m_db.db(), sql);
    if (!st.isValid()) return out;
    st.bind(1, head + QStringLiteral("%")).bind(2, spelling.trimmed());
    if (!book.isEmpty()) st.bind(3, book);

    QList<SimilarCandidate> cands;
    QList<Word> words;
    while (st.step()) {
        Word w = mapper::wordFromStmt(st, 0);
        const double sim = util::similarity(s, util::normalizeSpelling(w.spelling));
        cands.append({w.id, sim});
        words.append(w);
    }
    if (words.isEmpty()) {
        // 前缀无匹配时，随机取若干同长度词作为干扰
        return randomByBook(book, n);
    }
    // 按相似度降序
    QList<int> order;
    for (int i = 0; i < cands.size(); ++i) order << i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return cands[a].score > cands[b].score; });
    for (int i = 0; i < order.size() && out.size() < n; ++i) out << words[order[i]];
    return out;
}

QList<Word> WordRepository::findFormSimilar(const QString &spelling, int n) {
    return findSimilar(spelling, QString(), n);
}

qint64 WordRepository::countAll(const QString &book) {
    if (book.isEmpty()) return m_db.db().queryInt64(QStringLiteral("SELECT COUNT(*) FROM words"));
    Stmt st(m_db.db(), QStringLiteral("SELECT COUNT(*) FROM words WHERE book=?"));
    st.bind(1, book);
    return (st.isValid() && st.step()) ? st.colInt64(0) : 0;
}

bool WordRepository::remove(qint64 id) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM words WHERE id=?"));
    st.bind(1, id);
    return st.run();
}

bool WordRepository::removeByBook(const QString &book) {
    Stmt st(m_db.db(), QStringLiteral("DELETE FROM words WHERE book=?"));
    st.bind(1, book);
    return st.run();
}

} // namespace vt
