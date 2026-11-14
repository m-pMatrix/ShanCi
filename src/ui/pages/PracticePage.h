#pragma once
//
// PracticePage.h - 练习页：统一的做题入口
//   可从「每日词表 / 单词本 / 错题本 / 词库随机」选择题目来源并开始做题
//
#include "ui/Page.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace vt {

class PracticePage : public Page {
    Q_OBJECT
public:
    PracticePage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("practice"); }
    void refresh() override;

private slots:
    void onSourceChanged();
    void onStart();

private:
    QString currentSource() const;

    QComboBox *m_sourceCombo = nullptr;
    QComboBox *m_bookCombo = nullptr;
    QLabel *m_bookLabel = nullptr;
    QSpinBox *m_countSpin = nullptr;
    QLabel *m_infoLabel = nullptr;
    QPushButton *m_startBtn = nullptr;
};

} // namespace vt
