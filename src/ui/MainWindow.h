#pragma once
//
// MainWindow.h - 主界面（导航 + 页面容器），实现 Navigator
//
#include "ui/Navigation.h"

#include <QMainWindow>

#include <QHash>

QT_BEGIN_NAMESPACE
class QStackedWidget;
class QButtonGroup;
class QLabel;
class QPushButton;
class QHBoxLayout;
QT_END_NAMESPACE

namespace vt {

class AppContext;
class Page;
class QuizPage;
class ResultPage;

class MainWindow : public QMainWindow, public Navigator {
    Q_OBJECT
public:
    explicit MainWindow(AppContext &ctx, QWidget *parent = nullptr);
    ~MainWindow() override;

    // Navigator
    void navigateTo(const QString &pageId) override;
    void startQuiz(qint64 paperId) override;
    void showResult(qint64 paperId) override;

    // 打开时定位到指定页
    void showPage(const QString &pageId) { navigateTo(pageId); }

signals:
    void loggedOut();

private slots:
    void onLogout();

private:
    void buildSidebar(QHBoxLayout *rootLay);
    Page *page(const QString &id) const;

    AppContext &m_ctx;
    QStackedWidget *m_stack = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QHash<QString, Page *> m_pages;
    QHash<QString, QPushButton *> m_navButtons;
    QuizPage *m_quiz = nullptr;
    ResultPage *m_result = nullptr;
    QLabel *m_userLabel = nullptr;
};

} // namespace vt
