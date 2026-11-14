#include "core/Utils.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <algorithm>

namespace vt {
namespace util {

namespace {
// 本模块专用随机数发生器。
// 注意：Qt 6 中 QRandomGenerator::global()->seed() 会触发 qFatal，
// 因此这里使用独立的生成器实例（默认以系统随机数作为种子）。
QRandomGenerator &localRandom() {
    static QRandomGenerator gen(QRandomGenerator::system()->generate());
    return gen;
}
} // namespace

QString nowIso() { return toIso(QDateTime::currentDateTime()); }

QString toIso(const QDateTime &dt) {
    if (!dt.isValid()) return QString();
    return dt.toString(Qt::ISODate);
}

QDateTime fromIso(const QString &s) {
    if (s.trimmed().isEmpty()) return QDateTime();
    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return dt;
}

QString todayDate() {
    return QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
}

QString trimAll(const QString &s) { return s.trimmed(); }

QStringList splitCsv(const QString &s) {
    QStringList out;
    const auto parts = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const auto &p : parts) {
        const QString t = p.trimmed();
        if (!t.isEmpty()) out << t;
    }
    return out;
}

QString joinCsv(const QStringList &list) { return list.join(QLatin1Char(',')); }

QString normalizeSpelling(const QString &s) {
    QString t = s.trimmed().toLower();
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    t.replace(ws, QStringLiteral(" "));
    return t;
}

bool containsIgnoreCase(const QString &haystack, const QString &needle) {
    return haystack.contains(needle, Qt::CaseInsensitive);
}

QString generateSalt(int bytes) {
    QByteArray buf;
    buf.resize(bytes);
    for (int i = 0; i < bytes; ++i)
        buf[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    return QString::fromLatin1(buf.toHex());
}

QString sha256Hex(const QString &text) {
    const QByteArray hash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

int levenshtein(const QString &a, const QString &b) {
    const int n = a.size();
    const int m = b.size();
    if (n == 0) return m;
    if (m == 0) return n;
    QList<int> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        prev = cur;
    }
    return prev[m];
}

double similarity(const QString &a, const QString &b) {
    if (a.isEmpty() && b.isEmpty()) return 1.0;
    const int d = levenshtein(a, b);
    const int maxLen = std::max(a.size(), b.size());
    if (maxLen == 0) return 1.0;
    return 1.0 - static_cast<double>(d) / maxLen;
}

double prefixSimilarity(const QString &a, const QString &b) {
    const int n = std::min(a.size(), b.size());
    int common = 0;
    for (int i = 0; i < n; ++i) {
        if (a[i].toLower() == b[i].toLower()) ++common;
        else break;
    }
    const int maxLen = std::max(a.size(), b.size());
    if (maxLen == 0) return 0.0;
    return static_cast<double>(common) / maxLen;
}

void seedRandom(unsigned int seed) { localRandom().seed(seed); }

int randomInt(int low, int high) {
    if (high <= low) return low;
    return localRandom().bounded(low, high + 1);
}

QJsonArray toJsonArray(const QStringList &list) {
    QJsonArray arr;
    for (const auto &s : list) arr.append(s);
    return arr;
}

QStringList fromJsonArray(const QJsonArray &arr) {
    QStringList out;
    for (const auto &v : arr) out << v.toString();
    return out;
}

QString compactJson(const QJsonArray &arr) {
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QString humanBytes(qint64 bytes) {
    static const char *units[] = {"B", "KB", "MB", "GB"};
    double v = static_cast<double>(bytes);
    int i = 0;
    while (v >= 1024.0 && i < 3) { v /= 1024.0; ++i; }
    return QStringLiteral("%1 %2").arg(v, 0, 'f', i == 0 ? 0 : 1).arg(units[i]);
}

QString stripHtml(const QString &html) {
    QString s = html;
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    s.remove(tag);
    s.replace(QLatin1String("&nbsp;"), QStringLiteral(" "));
    s.replace(QLatin1String("&amp;"), QStringLiteral("&"));
    s.replace(QLatin1String("&lt;"), QStringLiteral("<"));
    s.replace(QLatin1String("&gt;"), QStringLiteral(">"));
    return s.simplified();
}

QString firstNonEmpty(const QStringList &candidates) {
    for (const auto &c : candidates)
        if (!c.trimmed().isEmpty()) return c.trimmed();
    return QString();
}

} // namespace util
} // namespace vt
