#pragma once
//
// SyncPage.h - 同步页（手动导出/导入 + 冲突处理 + 日志）
//
#include "ui/Page.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QComboBox;
class QTableWidget;
QT_END_NAMESPACE

namespace vt {

class SyncPage : public Page {
    Q_OBJECT
public:
    SyncPage(AppContext &ctx, Navigator *nav, QWidget *parent = nullptr);
    QString pageId() const override { return QStringLiteral("sync"); }
    void refresh() override;

private slots:
    void onExport();
    void onImport();
    void onClearLogs();
    void onBrowseExport();
    void onBrowseImport();

private:
    QLineEdit *m_exportPath = nullptr;
    QLineEdit *m_importPath = nullptr;
    QComboBox *m_policy = nullptr;
    QTableWidget *m_logs = nullptr;
};

} // namespace vt
