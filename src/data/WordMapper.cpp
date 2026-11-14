#include "data/WordMapper.h"

#include "core/Utils.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace vt {
namespace mapper {

QString wordColumns(const QString &alias) {
    const QString a = alias.isEmpty() ? QString() : alias + QLatin1Char('.');
    const QStringList cols = {
        QStringLiteral("id"), QStringLiteral("book"), QStringLiteral("word_rank"),
        QStringLiteral("spelling"), QStringLiteral("phonetic_uk"), QStringLiteral("phonetic_us"),
        QStringLiteral("audio_uk"), QStringLiteral("audio_us"), QStringLiteral("star"),
        QStringLiteral("pos"), QStringLiteral("translation_cn"), QStringLiteral("translation_en"),
        QStringLiteral("mem_method"), QStringLiteral("source"), QStringLiteral("phrases_json"),
        QStringLiteral("sentences_json"), QStringLiteral("synos_json"), QStringLiteral("relwords_json"),
        QStringLiteral("exam_json"), QStringLiteral("created_at"),
    };
    QStringList out;
    for (const auto &c : cols) out << a + c;
    return out.join(QStringLiteral(", "));
}

namespace {
QJsonArray parseArray(const QString &json) {
    if (json.trimmed().isEmpty()) return QJsonArray();
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    return doc.isArray() ? doc.array() : QJsonArray();
}
QString dumpArray(const QJsonArray &arr) {
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}
} // namespace

Word wordFromStmt(const Stmt &st, int base) {
    Word w;
    w.id = st.colInt64(base + 0);
    w.book = bookTypeFromKey(st.colText(base + 1));
    w.wordRank = st.colInt(base + 2);
    w.spelling = st.colText(base + 3);
    w.phoneticUk = st.colText(base + 4);
    w.phoneticUs = st.colText(base + 5);
    w.audioUk = st.colText(base + 6);
    w.audioUs = st.colText(base + 7);
    w.star = st.colInt(base + 8);
    const QString pos = st.colText(base + 9);
    const QString cn = st.colText(base + 10);
    const QString en = st.colText(base + 11);
    w.memMethod = st.colText(base + 12);
    w.source = st.colText(base + 13);
    w.phrases = phrasesFromJson(st.colText(base + 14));
    w.sentences = sentencesFromJson(st.colText(base + 15));
    w.synonyms = synonymsFromJson(st.colText(base + 16));
    w.relWords = relWordsFromJson(st.colText(base + 17));
    w.exams = examsFromJson(st.colText(base + 18));
    w.createdAt = util::fromIso(st.colText(base + 19));

    // 组装主释义（Trans/pos），完整释义由 JSON 提供（此处单列存储）
    Translation t;
    t.pos = pos;
    t.tranCn = cn;
    t.tranEn = en;
    if (!cn.isEmpty() || !en.isEmpty() || !pos.isEmpty()) w.translations << t;
    return w;
}

QString phrasesToJson(const QList<Phrase> &list) {
    QJsonArray arr;
    for (const auto &p : list) {
        QJsonObject o;
        o.insert(QStringLiteral("content"), p.content);
        o.insert(QStringLiteral("meaning"), p.meaning);
        arr.append(o);
    }
    return dumpArray(arr);
}

QList<Phrase> phrasesFromJson(const QString &json) {
    QList<Phrase> out;
    for (const auto &v : parseArray(json)) {
        const auto o = v.toObject();
        out << Phrase{o.value(QStringLiteral("content")).toString(),
                      o.value(QStringLiteral("meaning")).toString()};
    }
    return out;
}

QString sentencesToJson(const QList<Sentence> &list) {
    QJsonArray arr;
    for (const auto &s : list) {
        QJsonObject o;
        o.insert(QStringLiteral("en"), s.en);
        o.insert(QStringLiteral("cn"), s.cn);
        o.insert(QStringLiteral("audio"), s.audio);
        o.insert(QStringLiteral("tag"), s.tag);
        arr.append(o);
    }
    return dumpArray(arr);
}

QList<Sentence> sentencesFromJson(const QString &json) {
    QList<Sentence> out;
    for (const auto &v : parseArray(json)) {
        const auto o = v.toObject();
        out << Sentence{o.value(QStringLiteral("en")).toString(),
                        o.value(QStringLiteral("cn")).toString(),
                        o.value(QStringLiteral("audio")).toString(),
                        o.value(QStringLiteral("tag")).toString()};
    }
    return out;
}

QString synonymsToJson(const QList<Synonym> &list) {
    QJsonArray arr;
    for (const auto &s : list) {
        QJsonObject o;
        o.insert(QStringLiteral("pos"), s.pos);
        o.insert(QStringLiteral("tran"), s.tran);
        o.insert(QStringLiteral("words"), util::toJsonArray(s.words));
        arr.append(o);
    }
    return dumpArray(arr);
}

QList<Synonym> synonymsFromJson(const QString &json) {
    QList<Synonym> out;
    for (const auto &v : parseArray(json)) {
        const auto o = v.toObject();
        out << Synonym{o.value(QStringLiteral("pos")).toString(),
                       o.value(QStringLiteral("tran")).toString(),
                       util::fromJsonArray(o.value(QStringLiteral("words")).toArray())};
    }
    return out;
}

QString relWordsToJson(const QList<RelWord> &list) {
    QJsonArray arr;
    for (const auto &r : list) {
        QJsonObject o;
        o.insert(QStringLiteral("pos"), r.pos);
        QJsonArray words;
        for (const auto &p : r.words) {
            QJsonObject wo;
            wo.insert(QStringLiteral("hwd"), p.first);
            wo.insert(QStringLiteral("tran"), p.second);
            words.append(wo);
        }
        o.insert(QStringLiteral("words"), words);
        arr.append(o);
    }
    return dumpArray(arr);
}

QList<RelWord> relWordsFromJson(const QString &json) {
    QList<RelWord> out;
    for (const auto &v : parseArray(json)) {
        const auto o = v.toObject();
        RelWord r;
        r.pos = o.value(QStringLiteral("pos")).toString();
        for (const auto &wv : o.value(QStringLiteral("words")).toArray()) {
            const auto wo = wv.toObject();
            r.words << qMakePair(wo.value(QStringLiteral("hwd")).toString(),
                                 wo.value(QStringLiteral("tran")).toString());
        }
        out << r;
    }
    return out;
}

QString examsToJson(const QList<ExamItem> &list) {
    QJsonArray arr;
    for (const auto &e : list) {
        QJsonObject o;
        o.insert(QStringLiteral("question"), e.question);
        o.insert(QStringLiteral("choices"), util::toJsonArray(e.choices));
        o.insert(QStringLiteral("rightIndex"), e.rightIndex);
        o.insert(QStringLiteral("explain"), e.explain);
        arr.append(o);
    }
    return dumpArray(arr);
}

QList<ExamItem> examsFromJson(const QString &json) {
    QList<ExamItem> out;
    for (const auto &v : parseArray(json)) {
        const auto o = v.toObject();
        ExamItem e;
        e.question = o.value(QStringLiteral("question")).toString();
        e.choices = util::fromJsonArray(o.value(QStringLiteral("choices")).toArray());
        e.rightIndex = o.value(QStringLiteral("rightIndex")).toInt();
        e.explain = o.value(QStringLiteral("explain")).toString();
        out << e;
    }
    return out;
}

} // namespace mapper
} // namespace vt
