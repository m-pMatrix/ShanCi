#pragma once
//
// Navigation.h - 页面导航接口（表示层）
//
#include <QString>

namespace vt {

// 由 MainWindow 实现，供各页面请求跳转
class Navigator {
public:
    virtual ~Navigator() = default;
    virtual void navigateTo(const QString &pageId) = 0;
    virtual void startQuiz(qint64 paperId) = 0;
    virtual void showResult(qint64 paperId) = 0;
};

} // namespace vt
