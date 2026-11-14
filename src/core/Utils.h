#pragma once
//
// Utils.h - 通用工具（字符串、时间、哈希、相似度、随机）
//
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace vt {
namespace util {

// 时间
QString nowIso();
QString toIso(const QDateTime &dt);
QDateTime fromIso(const QString &s);
QString todayDate(); // yyyy-MM-dd

// 字符串
QString trimAll(const QString &s);
QStringList splitCsv(const QString &s);
QString joinCsv(const QStringList &list);
QString normalizeSpelling(const QString &s); // 去空格、转小写
bool containsIgnoreCase(const QString &haystack, const QString &needle);

// 哈希（离线账户口令）
QString generateSalt(int bytes = 16);
QString sha256Hex(const QString &text);

// 相似度（用于形近词/拼写校验）
int levenshtein(const QString &a, const QString &b);
double similarity(const QString &a, const QString &b);
// 前缀相似（形近词：共同前缀越长越像）
double prefixSimilarity(const QString &a, const QString &b);

// 随机
void seedRandom(unsigned int seed);
int randomInt(int low, int high); // 闭区间
template <typename T>
void shuffle(QList<T> &list) {
    for (int i = list.size() - 1; i > 0; --i) {
        int j = randomInt(0, i);
        std::swap(list[i], list[j]);
    }
}
template <typename T>
QList<T> sample(const QList<T> &list, int n) {
    if (n >= list.size()) return list;
    QList<T> copy = list;
    shuffle(copy);
    return copy.mid(0, n);
}

// JSON 辅助
QJsonArray toJsonArray(const QStringList &list);
QStringList fromJsonArray(const QJsonArray &arr);
QString compactJson(const QJsonArray &arr);

// 显示辅助
QString humanBytes(qint64 bytes);
QString stripHtml(const QString &html);
QString firstNonEmpty(const QStringList &candidates);

} // namespace util
} // namespace vt
