#pragma once
//
// HomePage.h - 主界面概览与功能导航
//
#include "ui/Page.h"

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace vt {

class HomePage : public Page {
    Q_OBJECT
public:
    HomePage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("home"); }
    void refresh() override;

private:
    QLabel *m_wordsLabel = nullptr;
    QLabel *m_bookLabel = nullptr;
    QLabel *m_wrongLabel = nullptr;
    QLabel *m_dailyLabel = nullptr;
};

} // namespace vt
