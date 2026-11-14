#include "ui/pages/SyncPage.h"

#include "core/AppContext.h"
#include "core/Paths.h"
#include "core/Utils.h"
#include "ui/Navigation.h"
#include "ui/UiHelpers.h"

#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace vt {

SyncPage::SyncPage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("手动同步"), this);
    title->setObjectName(QStringLiteral("Header"));
    root->addWidget(title);
    auto *sub = new QLabel(QStringLiteral("同步内容：错题本 + 个人设置（仅通过本地文件，不涉及云端）"), this);
    sub->setObjectName(QStringLiteral("SubHeader"));
    root->addWidget(sub);

    // 导出
    auto *exportBox = new QGroupBox(QStringLiteral("导出"), this);
    auto *exportLay = new QHBoxLayout(exportBox);
    m_exportPath = new QLineEdit(exportBox);
    m_exportPath->setPlaceholderText(QStringLiteral("导出文件路径（.json）"));
    auto *browseExport = new QPushButton(QStringLiteral("浏览…"), exportBox);
    browseExport->setProperty("secondary", true);
    auto *exportBtn = new QPushButton(QStringLiteral("导出"), exportBox);
    exportLay->addWidget(m_exportPath, 1);
    exportLay->addWidget(browseExport);
    exportLay->addWidget(exportBtn);
    root->addWidget(exportBox);

    // 导入
    auto *importBox = new QGroupBox(QStringLiteral("导入"), this);
    auto *importLay = new QHBoxLayout(importBox);
    m_importPath = new QLineEdit(importBox);
    m_importPath->setPlaceholderText(QStringLiteral("导入文件路径（.json）"));
    auto *browseImport = new QPushButton(QStringLiteral("浏览…"), importBox);
    browseImport->setProperty("secondary", true);
    m_policy = new QComboBox(importBox);
    m_policy->addItem(QStringLiteral("较新者优先"), QStringLiteral("newer"));
    m_policy->addItem(QStringLiteral("以本地为准"), QStringLiteral("local"));
    m_policy->addItem(QStringLiteral("以导入为准"), QStringLiteral("remote"));
    m_policy->addItem(QStringLiteral("合并"), QStringLiteral("merge"));
    auto *importBtn = new QPushButton(QStringLiteral("导入"), importBox);
    importLay->addWidget(m_importPath, 1);
    importLay->addWidget(browseImport);
    importLay->addWidget(new QLabel(QStringLiteral("冲突策略"), importBox));
    importLay->addWidget(m_policy);
    importLay->addWidget(importBtn);
    root->addWidget(importBox);

    auto *logHeader = new QHBoxLayout;
    auto *logTitle = new QLabel(QStringLiteral("同步日志"), this);
    logTitle->setObjectName(QStringLiteral("Header"));
    auto *clearBtn = new QPushButton(QStringLiteral("清空日志"), this);
    clearBtn->setProperty("secondary", true);
    logHeader->addWidget(logTitle);
    logHeader->addStretch();
    logHeader->addWidget(clearBtn);
    root->addLayout(logHeader);

    m_logs = ui::makeTable({QStringLiteral("时间"), QStringLiteral("方向"), QStringLiteral("文件"),
                            QStringLiteral("条目"), QStringLiteral("冲突"), QStringLiteral("状态"),
                            QStringLiteral("说明")},
                           this);
    root->addWidget(m_logs, 1);

    connect(browseExport, &QPushButton::clicked, this, &SyncPage::onBrowseExport);
    connect(browseImport, &QPushButton::clicked, this, &SyncPage::onBrowseImport);
    connect(exportBtn, &QPushButton::clicked, this, &SyncPage::onExport);
    connect(importBtn, &QPushButton::clicked, this, &SyncPage::onImport);
    connect(clearBtn, &QPushButton::clicked, this, &SyncPage::onClearLogs);
    connect(&ctx, &AppContext::dataChanged, this, &SyncPage::refresh);

    const Setting s = ctx.currentSetting();
    const QString defDir = s.syncPath.isEmpty() ? Paths::syncDir() : s.syncPath;
    m_exportPath->setText(defDir + QStringLiteral("/vocab-sync-") +
                          QDate::currentDate().toString(QStringLiteral("yyyyMMdd")) + QStringLiteral(".json"));
}

void SyncPage::refresh() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    const auto logs = ctx().syncService().logs(uid, 200);
    ui::clearTable(m_logs);
    m_logs->setRowCount(logs.size());
    for (int i = 0; i < logs.size(); ++i) {
        const auto &l = logs[i];
        ui::setRow(m_logs, i,
                   {ui::fmtDate(l.createdAt), displayName(l.direction), l.filePath,
                    QString::number(l.itemCount), QString::number(l.conflictCount),
                    l.status == SyncStatus::Success ? QStringLiteral("成功")
                        : (l.status == SyncStatus::Partial ? QStringLiteral("部分成功") : QStringLiteral("失败")),
                    l.message});
    }
    m_logs->resizeColumnsToContents();
    if (m_logs->columnCount() > 2) m_logs->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
}

void SyncPage::onBrowseExport() {
    const QString f = QFileDialog::getSaveFileName(this, QStringLiteral("导出同步文件"),
                                                   m_exportPath->text(), QStringLiteral("JSON (*.json)"));
    if (!f.isEmpty()) m_exportPath->setText(f);
}

void SyncPage::onBrowseImport() {
    const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择同步文件"),
                                                   m_importPath->text(), QStringLiteral("JSON (*.json)"));
    if (!f.isEmpty()) m_importPath->setText(f);
}

void SyncPage::onExport() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    const QString path = m_exportPath->text().trimmed();
    if (path.isEmpty()) { ui::warn(this, QStringLiteral("请先选择导出路径")); return; }
    auto res = ctx().syncService().exportTo(path, uid);
    if (!res) { ui::error(this, res.error()); return; }
    ui::info(this, QStringLiteral("导出成功：\n%1").arg(res.value()));
    refresh();
}

void SyncPage::onImport() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    const QString path = m_importPath->text().trimmed();
    if (path.isEmpty()) { ui::warn(this, QStringLiteral("请先选择导入文件")); return; }

    SyncService::ConflictPolicy policy = SyncService::ConflictPolicy::PreferNewer;
    const QString p = m_policy->currentData().toString();
    if (p == QLatin1String("local")) policy = SyncService::ConflictPolicy::PreferLocal;
    else if (p == QLatin1String("remote")) policy = SyncService::ConflictPolicy::PreferRemote;
    else if (p == QLatin1String("merge")) policy = SyncService::ConflictPolicy::Merge;

    auto res = ctx().syncService().importFrom(path, uid, policy);
    if (!res) { ui::error(this, res.error()); return; }
    const auto &r = res.value();
    QString msg = QStringLiteral("导入完成\n新增：%1\n更新：%2\n冲突：%3")
                      .arg(r.added).arg(r.updated).arg(r.conflicts);
    if (!r.details.isEmpty()) {
        msg += QStringLiteral("\n\n冲突明细（最多显示 10 条）：");
        for (int i = 0; i < r.details.size() && i < 10; ++i) msg += QStringLiteral("\n· ") + r.details[i];
    }
    ui::info(this, msg);
    refresh();
    emit ctx().dataChanged();
}

void SyncPage::onClearLogs() {
    const qint64 uid = ctx().currentUserId();
    if (!ui::confirm(this, QStringLiteral("确定清空同步日志？"))) return;
    ctx().syncService().clearLogs(uid);
    refresh();
}

} // namespace vt
