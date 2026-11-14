#include "ui/pages/WrongBookPage.h"

#include "core/AppContext.h"
#include "core/Utils.h"
#include "ui/Navigation.h"
#include "ui/UiHelpers.h"
#include "ui/dialogs/WordDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace vt {

namespace {
// 编辑错题条目（备注 / 我的答案 / 优先级 / 状态）
bool editWrongEntry(AppContext &ctx, WrongEntry entry, QWidget *parent) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("编辑错题"));
    auto *root = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout;

    auto *spelling = new QLineEdit(entry.word.spelling, &dlg);
    spelling->setReadOnly(true);
    auto *myAnswer = new QLineEdit(entry.myAnswer, &dlg);
    auto *priority = new QComboBox(&dlg);
    priority->addItem(QStringLiteral("低"), QStringLiteral("LOW"));
    priority->addItem(QStringLiteral("中"), QStringLiteral("NORMAL"));
    priority->addItem(QStringLiteral("高"), QStringLiteral("HIGH"));
    priority->setCurrentIndex(priority->findData(toKey(entry.priority)));
    auto *status = new QComboBox(&dlg);
    status->addItem(QStringLiteral("新错题"), QStringLiteral("NEW"));
    status->addItem(QStringLiteral("复习中"), QStringLiteral("REVIEWING"));
    status->addItem(QStringLiteral("已掌握"), QStringLiteral("MASTERED"));
    status->setCurrentIndex(status->findData(toKey(entry.status)));
    auto *remark = new QPlainTextEdit(entry.remark, &dlg);
    remark->setMaximumHeight(90);

    form->addRow(QStringLiteral("单词"), spelling);
    form->addRow(QStringLiteral("我的答案"), myAnswer);
    form->addRow(QStringLiteral("优先级"), priority);
    form->addRow(QStringLiteral("状态"), status);
    form->addRow(QStringLiteral("备注"), remark);
    root->addLayout(form);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    root->addWidget(box);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return false;
    entry.myAnswer = myAnswer->text();
    entry.priority = wrongPriorityFromKey(priority->currentData().toString());
    entry.status = wrongStatusFromKey(status->currentData().toString());
    entry.remark = remark->toPlainText();
    const auto st = ctx.wrongBookService().update(entry);
    if (!st.isOk()) { ui::error(parent, st.error()); return false; }
    return true;
}
} // namespace

WrongBookPage::WrongBookPage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("错题本"), this);
    title->setObjectName(QStringLiteral("Header"));
    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("SubHeader"));
    header->addWidget(title);
    header->addSpacing(12);
    header->addWidget(m_countLabel);
    header->addStretch();
    root->addLayout(header);

    auto *bar = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索错题…"));
    m_search->setMinimumWidth(200);
    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItem(QStringLiteral("全部状态"), QString());
    m_statusFilter->addItem(QStringLiteral("新错题"), QStringLiteral("NEW"));
    m_statusFilter->addItem(QStringLiteral("复习中"), QStringLiteral("REVIEWING"));
    m_statusFilter->addItem(QStringLiteral("已掌握"), QStringLiteral("MASTERED"));
    m_bookFilter = new QComboBox(this);
    m_bookFilter->addItem(QStringLiteral("全部词书"), QString());
    m_bookFilter->addItem(QStringLiteral("四级"), QStringLiteral("CET4"));
    m_bookFilter->addItem(QStringLiteral("六级"), QStringLiteral("CET6"));
    auto *searchBtn = new QPushButton(QStringLiteral("查询"), this);
    bar->addWidget(m_search);
    bar->addWidget(m_statusFilter);
    bar->addWidget(m_bookFilter);
    bar->addWidget(searchBtn);
    bar->addStretch();
    root->addLayout(bar);

    m_table = ui::makeTable({QStringLiteral("编号"), QStringLiteral("错题关联单词"),
                             QStringLiteral("释义"), QStringLiteral("错误次数"),
                             QStringLiteral("答对次数"), QStringLiteral("优先级"), QStringLiteral("状态"),
                             QStringLiteral("最近错误时间")},
                            this);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    root->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *detailBtn = new QPushButton(QStringLiteral("查看详情"), this);
    detailBtn->setProperty("secondary", true);
    auto *editBtn = new QPushButton(QStringLiteral("编辑"), this);
    editBtn->setProperty("secondary", true);
    auto *masterBtn = new QPushButton(QStringLiteral("标记已掌握"), this);
    masterBtn->setProperty("secondary", true);
    auto *removeBtn = new QPushButton(QStringLiteral("移除错题"), this);
    removeBtn->setProperty("danger", true);
    actions->addWidget(detailBtn);
    actions->addWidget(editBtn);
    actions->addWidget(masterBtn);
    actions->addStretch();
    actions->addWidget(removeBtn);
    root->addLayout(actions);

    connect(searchBtn, &QPushButton::clicked, this, &WrongBookPage::onSearch);
    connect(m_search, &QLineEdit::returnPressed, this, &WrongBookPage::onSearch);
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { refresh(); });
    connect(m_bookFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { refresh(); });
    connect(m_table, &QTableWidget::doubleClicked, this, &WrongBookPage::onEdit);
    connect(m_table, &QWidget::customContextMenuRequested, this, &WrongBookPage::onContextMenu);
    connect(detailBtn, &QPushButton::clicked, this, &WrongBookPage::onDetail);
    connect(editBtn, &QPushButton::clicked, this, &WrongBookPage::onEdit);
    connect(masterBtn, &QPushButton::clicked, this, &WrongBookPage::onMaster);
    connect(removeBtn, &QPushButton::clicked, this, &WrongBookPage::onRemove);

    connect(&ctx, &AppContext::dataChanged, this, &WrongBookPage::refresh);
}

int WrongBookPage::selectedRow() const {
    const auto rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

qint64 WrongBookPage::selectedEntryId() const {
    const int row = selectedRow();
    if (row < 0) return -1;
    return m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void WrongBookPage::onSearch() { refresh(); }

void WrongBookPage::refresh() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;

    WordQuery q;
    q.keyword = m_search->text();
    q.wrongStatus = m_statusFilter->currentData().toString();
    q.book = m_bookFilter->currentData().toString();
    q.limit = 500;

    const auto entries = ctx().wrongBookService().list(uid, q, &m_total);
    ui::clearTable(m_table);
    m_table->setRowCount(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        ui::setRow(m_table, i,
                   {QString::number(i + 1), e.word.spelling, ui::shortMeaning(e.word, 34),
                    QString::number(e.wrongCount), QString::number(e.correctCount),
                    ui::priorityBadge(e.priority), ui::statusBadge(e.status),
                    ui::fmtDate(e.lastWrongAt)});
        m_table->item(i, 0)->setData(Qt::UserRole, e.id);
        m_table->item(i, 0)->setData(Qt::UserRole + 1, e.wordId);
    }
    m_table->resizeColumnsToContents();
    if (m_table->columnCount() > 2)
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_countLabel->setText(QStringLiteral("共 %1 道错题（显示 %2）").arg(m_total).arg(entries.size()));
}

void WrongBookPage::onDetail() {
    const qint64 uid = ctx().currentUserId();
    const int row = selectedRow();
    if (row < 0) { ui::warn(this, QStringLiteral("请先选择一道错题")); return; }
    const qint64 wordId = m_table->item(row, 0)->data(Qt::UserRole + 1).toLongLong();
    if (wordId <= 0) return;
    WordDialog dlg(ctx(), wordId, this);
    connect(&dlg, &WordDialog::wordChanged, this, &WrongBookPage::refresh);
    dlg.exec();
    refresh();
    Q_UNUSED(uid);
}

void WrongBookPage::onEdit() {
    const qint64 uid = ctx().currentUserId();
    const int row = selectedRow();
    if (row < 0) return;
    const qint64 entryId = m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
    WordQuery q;
    q.limit = 100000;
    const auto all = ctx().wrongBookService().list(uid, q);
    for (const auto &e : all) {
        if (e.id == entryId) {
            if (editWrongEntry(ctx(), e, this)) { refresh(); emit ctx().dataChanged(); }
            return;
        }
    }
}

void WrongBookPage::onRemove() {
    const qint64 entryId = selectedEntryId();
    if (entryId < 0) return;
    if (!ui::confirm(this, QStringLiteral("确定移除该错题？"))) return;
    const auto st = ctx().wrongBookService().remove(entryId);
    if (!st.isOk()) ui::warn(this, st.error());
    refresh();
    emit ctx().dataChanged();
}

void WrongBookPage::onMaster() {
    const qint64 uid = ctx().currentUserId();
    const int row = selectedRow();
    if (row < 0) return;
    const qint64 wordId = m_table->item(row, 0)->data(Qt::UserRole + 1).toLongLong();
    if (wordId <= 0) return;
    const auto st = ctx().wrongBookService().markMastered(uid, wordId);
    if (!st.isOk()) ui::warn(this, st.error());
    refresh();
    emit ctx().dataChanged();
}

void WrongBookPage::onContextMenu(const QPoint &pos) {
    if (selectedRow() < 0) return;
    QMenu menu(this);
    menu.addAction(QStringLiteral("查看详情"), this, &WrongBookPage::onDetail);
    menu.addAction(QStringLiteral("编辑"), this, &WrongBookPage::onEdit);
    menu.addAction(QStringLiteral("标记已掌握"), this, &WrongBookPage::onMaster);
    menu.addSeparator();
    menu.addAction(QStringLiteral("移除"), this, &WrongBookPage::onRemove);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

} // namespace vt
