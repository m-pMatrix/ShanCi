#pragma once
//
// QuizPage.h - 做题页
//
#include "domain/Entities.h"
#include "ui/Page.h"

#include <QElapsedTimer>

QT_BEGIN_NAMESPACE
class QLabel;
class QVBoxLayout;
class QRadioButton;
class QLineEdit;
class QPushButton;
class QProgressBar;
QT_END_NAMESPACE

namespace vt {

class QuizPage : public Page {
    Q_OBJECT
public:
    QuizPage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("quiz"); }
    void refresh() override;

    void beginQuiz(qint64 paperId);

private slots:
    void onSubmit();
    void onNext();
    void onFinish();

private:
    void showQuestion(int index);
    void clearOptions();
    QString currentAnswer() const;
    void setFeedback(bool correct, const QString &correctAnswer, const QString &explanation);

    qint64 m_paperId = 0;
    QList<PaperEntry> m_entries;
    int m_index = 0;
    bool m_answered = false;
    QElapsedTimer m_timer;

    QLabel *m_progress = nullptr;
    QProgressBar *m_bar = nullptr;
    QLabel *m_typeLabel = nullptr;
    QLabel *m_stem = nullptr;
    QVBoxLayout *m_optionLayout = nullptr;
    QWidget *m_optionHost = nullptr;
    QLineEdit *m_input = nullptr;
    QLabel *m_feedback = nullptr;
    QPushButton *m_submitBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_finishBtn = nullptr;
    QList<QRadioButton *> m_radios;
};

} // namespace vt
