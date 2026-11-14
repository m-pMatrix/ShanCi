#pragma once
//
// DailyListPage.h - 每日词表（背诵计划）页
//   支持多个自定义背诵计划：新建 / 选择 / 保存 / 删除，并用所选计划生成今日词表
//
#include "ui/Page.h"

QT_BEGIN_NAMESPACE
class QComboBox;
class QSpinBox;
class QCheckBox;
class QTableWidget;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace vt {

class DailyListPage : public Page {
    Q_OBJECT
public:
    DailyListPage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("dailylist"); }
    void refresh() override;

private slots:
    void onPlanChanged();
    void onNewPlan();
    void onSavePlan();
    void onDeletePlan();
    void onGenerateToday();
    void onToggleCompleted();
    void onHistoryChanged();

private:
    void reloadPlans();
    void loadPlanWidgets();
    void loadList(qint64 listId);
    void updateProgressLabel();

    QComboBox *m_planCombo = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QComboBox *m_bookCombo = nullptr;
    QSpinBox *m_daysSpin = nullptr;
    QSpinBox *m_newSpin = nullptr;
    QSpinBox *m_reviewSpin = nullptr;
    QCheckBox *m_includeWrong = nullptr;

    QComboBox *m_historyCombo = nullptr;
    QLabel *m_progressLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_infoLabel = nullptr;
    qint64 m_currentPlanId = 0;
    qint64 m_currentListId = 0;
};

} // namespace vt
