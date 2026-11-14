#include "core/Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace vt {

Config &Config::instance() {
    static Config cfg;
    return cfg;
}

void Config::load(const QString &path) {
    m_path = path;
    QFile f(path);
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    const QByteArray data = f.readAll();
    f.close();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject())
        m_data = doc.object();
}

void Config::save() const {
    if (m_path.isEmpty()) return;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;
    f.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
    f.close();
}

QString Config::getString(const QString &key, const QString &def) const {
    const auto v = m_data.value(key);
    return v.isString() ? v.toString() : def;
}

int Config::getInt(const QString &key, int def) const {
    const auto v = m_data.value(key);
    return v.isDouble() ? v.toInt() : def;
}

bool Config::getBool(const QString &key, bool def) const {
    const auto v = m_data.value(key);
    return v.isBool() ? v.toBool() : def;
}

void Config::set(const QString &key, const QJsonValue &value) { m_data.insert(key, value); }

} // namespace vt
