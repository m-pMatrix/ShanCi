#pragma once
//
// Config.h - 应用级配置管理（第七层：配置管理）
// 与用户个人设置（存数据库）区分：这里保存与账户无关的全局配置。
//
#include <QJsonObject>
#include <QString>

namespace vt {

class Config {
public:
    static Config &instance();

    void load(const QString &path);
    void save() const;

    QString getString(const QString &key, const QString &def = QString()) const;
    int getInt(const QString &key, int def = 0) const;
    bool getBool(const QString &key, bool def = false) const;
    void set(const QString &key, const QJsonValue &value);

    QString filePath() const { return m_path; }

private:
    Config() = default;
    QString m_path;
    QJsonObject m_data;
};

} // namespace vt
