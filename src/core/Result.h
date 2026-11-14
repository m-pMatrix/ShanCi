#pragma once
//
// Result.h - 服务层统一返回类型，避免异常穿透
//
#include <QString>
#include <utility>

namespace vt {

template <typename T>
class Result {
public:
    static Result ok(T value) { return Result(true, QString(), std::move(value)); }
    static Result err(const QString &message) { return Result(false, message, T{}); }

    bool isOk() const { return m_ok; }
    bool isErr() const { return !m_ok; }
    const QString &error() const { return m_error; }
    const T &value() const { return m_value; }
    T &value() { return m_value; }
    T take() { return std::move(m_value); }

    explicit operator bool() const { return m_ok; }

private:
    Result(bool ok, QString err, T value)
        : m_ok(ok), m_error(std::move(err)), m_value(std::move(value)) {}

    bool m_ok;
    QString m_error;
    T m_value;
};

// 无返回值场景
class Status {
public:
    static Status ok() { return Status(true, QString()); }
    static Status err(const QString &message) { return Status(false, message); }
    bool isOk() const { return m_ok; }
    bool isErr() const { return !m_ok; }
    const QString &error() const { return m_error; }
    explicit operator bool() const { return m_ok; }

private:
    Status(bool ok, QString err) : m_ok(ok), m_error(std::move(err)) {}
    bool m_ok;
    QString m_error;
};

} // namespace vt
