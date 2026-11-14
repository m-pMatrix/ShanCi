#pragma once
//
// Theme.h - 界面主题与样式（表示层）
//
#include <QString>

QT_BEGIN_NAMESPACE
class QApplication;
QT_END_NAMESPACE

namespace vt {

namespace theme {

// 应用主题（light / dark）
void apply(QApplication &app, const QString &name);

// 主样式表
QString styleSheet(const QString &name);

// 颜色
QString accent();
QString accentHover();
QString dangerColor();
QString successColor();
QString warningColor();

} // namespace theme
} // namespace vt
