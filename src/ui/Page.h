#pragma once
//
// Page.h - 页面基类
//
#include <QString>
#include <QWidget>

namespace vt {

class AppContext;
class Navigator;

class Page : public QWidget {
    Q_OBJECT
public:
    Page(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr)
        : QWidget(parent), m_ctx(ctx), m_nav(nav) {}
    ~Page() override = default;

    virtual QString pageId() const = 0;
    virtual void refresh() {}

protected:
    AppContext &ctx() const { return m_ctx; }
    Navigator *nav() const { return m_nav; }

private:
    AppContext &m_ctx;
    Navigator *m_nav;
};

} // namespace vt
