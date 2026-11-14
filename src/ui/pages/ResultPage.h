#pragma once
//
// ResultPage.h - 结果页
//
#include "ui/Page.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QTableWidget;
QT_END_NAMESPACE

namespace vt {

class ResultPage : public Page {
    Q_OBJECT
public:
    ResultPage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("result"); }
    void refresh() override;

    void showResult(qint64 paperId);

private slots:
    void onAddWrong();
    void onRetake();
    void onHome();

private:
    qint64 m_paperId = 0;
    QLabel *m_score = nullptr;
    QLabel *m_summary = nullptr;
    QTableWidget *m_table = nullptr;
};

} // namespace vt
