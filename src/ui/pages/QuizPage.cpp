#include "ui/pages/QuizPage.h"

#include "core/AppContext.h"
#include "ui/Navigation.h"
#include "ui/UiHelpers.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace vt {

QuizPage::QuizPage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 20, 28, 20);
    root->setSpacing(12);

    auto *top = new QHBoxLayout;
    m_progress = new QLabel(QStringLiteral("做题"), this);
    m_progress->setObjectName(QStringLiteral("Header"));
    m_typeLabel = new QLabel(this);
    m_typeLabel->setObjectName(QStringLiteral("SubHeader"));
    top->addWidget(m_progress);
    top->addSpacing(12);
    top->addWidget(m_typeLabel);
    top->addStretch();
    m_finishBtn = new QPushButton(QStringLiteral("交卷"), this);
    top->addWidget(m_finishBtn);
    root->addLayout(top);

    m_bar = new QProgressBar(this);
    m_bar->setTextVisible(true);
    root->addWidget(m_bar);

    auto *card = new QFrame(this);
    card->setStyleSheet(QStringLiteral("QFrame{background:palette(base);border:1px solid #E2E6EF;border-radius:12px;}"));
    auto *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(20, 18, 20, 18);
    m_stem = new QLabel(card);
    m_stem->setWordWrap(true);
    m_stem->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_stem->setStyleSheet(QStringLiteral("font-size:17px;font-weight:600;"));
    cardLay->addWidget(m_stem);

    m_optionHost = new QWidget(card);
    m_optionLayout = new QVBoxLayout(m_optionHost);
    m_optionLayout->setContentsMargins(0, 8, 0, 0);
    m_optionLayout->setSpacing(6);
    cardLay->addWidget(m_optionHost);

    m_input = new QLineEdit(card);
    m_input->setPlaceholderText(QStringLiteral("请输入英文单词…"));
    m_input->setVisible(false);
    cardLay->addWidget(m_input);

    m_feedback = new QLabel(card);
    m_feedback->setWordWrap(true);
    m_feedback->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_feedback->setVisible(false);
    cardLay->addWidget(m_feedback);

    root->addWidget(card, 1);

    auto *actions = new QHBoxLayout;
    m_submitBtn = new QPushButton(QStringLiteral("提交答案"), this);
    m_nextBtn = new QPushButton(QStringLiteral("下一题"), this);
    m_nextBtn->setEnabled(false);
    actions->addStretch();
    actions->addWidget(m_submitBtn);
    actions->addWidget(m_nextBtn);
    root->addLayout(actions);

    connect(m_submitBtn, &QPushButton::clicked, this, &QuizPage::onSubmit);
    connect(m_nextBtn, &QPushButton::clicked, this, &QuizPage::onNext);
    connect(m_finishBtn, &QPushButton::clicked, this, &QuizPage::onFinish);
    connect(m_input, &QLineEdit::returnPressed, this, &QuizPage::onSubmit);
}

void QuizPage::beginQuiz(qint64 paperId) {
    m_paperId = paperId;
    m_entries = ctx().questionService().entries(paperId);
    m_index = 0;
    m_answered = false;
    if (!m_entries.isEmpty()) showQuestion(0);
}

void QuizPage::refresh() {
    if (m_paperId > 0 && m_entries.isEmpty()) beginQuiz(m_paperId);
}

void QuizPage::clearOptions() {
    for (auto *r : m_radios) r->deleteLater();
    m_radios.clear();
}

void QuizPage::showQuestion(int index) {
    if (index < 0 || index >= m_entries.size()) return;
    m_index = index;
    m_answered = false;
    const PaperEntry &e = m_entries[index];
    const Question &q = e.question;

    m_progress->setText(QStringLiteral("第 %1 / %2 题").arg(index + 1).arg(m_entries.size()));
    m_typeLabel->setText(QStringLiteral("[%1]").arg(displayName(q.type)));
    m_bar->setRange(0, m_entries.size());
    m_bar->setValue(index + 1);

    m_stem->setText(q.stem);
    m_feedback->setVisible(false);
    m_nextBtn->setEnabled(false);
    m_submitBtn->setEnabled(!e.answered);
    clearOptions();

    const bool isChoice = !q.options.isEmpty();
    m_optionHost->setVisible(isChoice);
    m_input->setVisible(!isChoice);
    m_input->clear();
    m_input->setEnabled(!e.answered);

    if (isChoice) {
        for (int i = 0; i < q.options.size(); ++i) {
            auto *r = new QRadioButton(QStringLiteral("%1. %2").arg(QChar('A' + i)).arg(q.options[i]),
                                       m_optionHost);
            r->setEnabled(!e.answered);
            if (e.answered && e.userAnswer == q.options[i]) r->setChecked(true);
            m_optionLayout->addWidget(r);
            m_radios << r;
        }
    } else if (e.answered) {
        m_input->setText(e.userAnswer);
        m_input->setEnabled(false);
    }

    // 如果已作答，显示反馈
    if (e.answered) {
        setFeedback(e.isCorrect, q.answer, q.explanation);
        m_nextBtn->setEnabled(index < m_entries.size() - 1);
        if (index == m_entries.size() - 1) m_nextBtn->setEnabled(false);
    }

    m_timer.restart();
}

QString QuizPage::currentAnswer() const {
    if (!m_radios.isEmpty()) {
        for (auto *r : m_radios)
            if (r->isChecked()) {
                const int idx = m_radios.indexOf(r);
                return m_entries[m_index].question.options.value(idx);
            }
        return QString();
    }
    return m_input->text().trimmed();
}

void QuizPage::setFeedback(bool correct, const QString &correctAnswer, const QString &explanation) {
    m_feedback->setVisible(true);
    const QString color = correct ? QStringLiteral("#2E9E5B") : QStringLiteral("#D64545");
    const QString head = correct ? QStringLiteral("✔ 回答正确") : QStringLiteral("✘ 回答错误");
    QString text = QStringLiteral("<span style='color:%1;font-weight:600;'>%2</span>").arg(color, head);
    if (!correct)
        text += QStringLiteral("<br>正确答案：<b>%1</b>").arg(correctAnswer.toHtmlEscaped());
    if (!explanation.isEmpty())
        text += QStringLiteral("<br><span style='color:#6B7280'>%1</span>")
                    .arg(explanation.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
    m_feedback->setText(text);
}

void QuizPage::onSubmit() {
    if (m_answered || m_index >= m_entries.size()) return;
    const QString answer = currentAnswer();
    if (answer.isEmpty()) { ui::warn(this, QStringLiteral("请先作答")); return; }

    const PaperEntry &e = m_entries[m_index];
    const qint64 ms = m_timer.isValid() ? m_timer.elapsed() : 0;
    auto res = ctx().questionService().submitAnswer(e.id, answer, ms);
    if (!res) { ui::error(this, res.error()); return; }

    m_entries[m_index].answered = true;
    m_entries[m_index].userAnswer = answer;
    m_entries[m_index].isCorrect = res.value().correct;
    m_answered = true;
    m_submitBtn->setEnabled(false);
    if (!m_radios.isEmpty()) for (auto *r : m_radios) r->setEnabled(false);
    m_input->setEnabled(false);
    setFeedback(res.value().correct, res.value().correctAnswer, res.value().explanation);
    m_nextBtn->setEnabled(m_index < m_entries.size() - 1);
    emit ctx().dataChanged();
}

void QuizPage::onNext() {
    if (m_index < m_entries.size() - 1) showQuestion(m_index + 1);
}

void QuizPage::onFinish() {
    if (m_paperId <= 0) { nav()->navigateTo(QStringLiteral("home")); return; }
    const auto st = ctx().questionService().finishPaper(m_paperId);
    if (!st.isOk()) ui::warn(this, st.error());
    emit ctx().dataChanged();
    nav()->showResult(m_paperId);
    m_paperId = 0;
    m_entries.clear();
}

} // namespace vt
