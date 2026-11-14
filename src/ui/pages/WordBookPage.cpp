#include "ui/pages/WordBookPage.h"

#include "core/AppContext.h"
#include "core/Utils.h"
#include "ui/Navigation.h"
#include "ui/UiHelpers.h"
#include "ui/dialogs/WordDialog.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace vt {

WordBookPage::WordBookPage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("单词本"), this);
    title->setObjectName(QStringLiteral("Header"));
    m_hintLabel = new QLabel(
        QStringLiteral("单词本用于收藏你在「查询字典」中查到的生词，可创建多个单词本分类收藏"),
        this);
    m_hintLabel->setObjectName(QStringLiteral("SubHeader"));
    header->addWidget(title);
    header->addSpacing(12);
    header->addWidget(m_hintLabel);
    header->addStretch();
    root->addLayout(header);

    // ---- 单词本（收藏夹）管理 ----
    auto *bookBar = new QGridLayout;
    bookBar->setSpacing(8);
    bookBar->addWidget(new QLabel(QStringLiteral("当前单词本"), this), 0, 0);
    m_bookCombo = new QComboBox(this);
    m_bookCombo->setMinimumWidth(140);
    m_bookCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *newBtn = new QPushButton(QStringLiteral("新建"), this);
    auto *renameBtn = new QPushButton(QStringLiteral("重命名"), this);
    renameBtn->setProperty("secondary", true);
    auto *delBtn = new QPushButton(QStringLiteral("删除"), this);
    delBtn->setProperty("danger", true);
    auto *batchBtn = new QPushButton(QStringLiteral("批量添加词书…"), this);
    batchBtn->setProperty("flat", true);
    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("SubHeader"));

    bookBar->addWidget(m_bookCombo, 0, 1);
    bookBar->addWidget(newBtn, 0, 2);
    bookBar->addWidget(renameBtn, 0, 3);
    bookBar->addWidget(delBtn, 0, 4);
    bookBar->setColumnStretch(5, 1);
    bookBar->addWidget(batchBtn, 1, 0);
    bookBar->addWidget(m_countLabel, 1, 1, 1, 3);
    root->addLayout(bookBar);

    // ---- 过滤 ----
    auto *bar = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("在本单词本中搜索…"));
    m_search->setMinimumWidth(200);
    m_bookFilter = new QComboBox(this);
    m_bookFilter->addItem(QStringLiteral("全部词书"), QString());
    m_bookFilter->addItem(QStringLiteral("四级"), QStringLiteral("CET4"));
    m_bookFilter->addItem(QStringLiteral("六级"), QStringLiteral("CET6"));
    m_famFilter = new QComboBox(this);
    m_famFilter->addItem(QStringLiteral("全部熟悉度"), -1);
    for (int i = 0; i <= 5; ++i)
        m_famFilter->addItem(QStringLiteral("熟悉度 ≥ %1").arg(i), i);
    auto *searchBtn = new QPushButton(QStringLiteral("查询"), this);
    bar->addWidget(m_search);
    bar->addWidget(m_bookFilter);
    bar->addWidget(m_famFilter);
    bar->addWidget(searchBtn);
    bar->addStretch();
    root->addLayout(bar);

    m_table = ui::makeTable({QStringLiteral("单词"), QStringLiteral("音标"), QStringLiteral("释义"),
                             QStringLiteral("词书"), QStringLiteral("熟悉度"), QStringLiteral("复习次数"),
                             QStringLiteral("下次复习")},
                            this);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    root->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *detailBtn = new QPushButton(QStringLiteral("查看详情"), this);
    detailBtn->setProperty("secondary", true);
    auto *editBtn = new QPushButton(QStringLiteral("编辑"), this);
    editBtn->setProperty("secondary", true);
    auto *removeBtn = new QPushButton(QStringLiteral("移出本单词本"), this);
    removeBtn->setProperty("danger", true);
    actions->addWidget(detailBtn);
    actions->addWidget(editBtn);
    actions->addStretch();
    actions->addWidget(removeBtn);
    root->addLayout(actions);

    connect(m_bookCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &WordBookPage::onBookChanged);
    connect(newBtn, &QPushButton::clicked, this, &WordBookPage::onNewBook);
    connect(renameBtn, &QPushButton::clicked, this, &WordBookPage::onRenameBook);
    connect(delBtn, &QPushButton::clicked, this, &WordBookPage::onDeleteBook);
    connect(batchBtn, &QPushButton::clicked, this, &WordBookPage::onBatchAddBookWords);
    connect(searchBtn, &QPushButton::clicked, this, &WordBookPage::onSearch);
    connect(m_search, &QLineEdit::returnPressed, this, &WordBookPage::onSearch);
    connect(m_bookFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { refresh(); });
    connect(m_famFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { refresh(); });
    connect(m_table, &QTableWidget::doubleClicked, this, &WordBookPage::onDetail);
    connect(m_table, &QWidget::customContextMenuRequested, this, &WordBookPage::onContextMenu);
    connect(detailBtn, &QPushButton::clicked, this, &WordBookPage::onDetail);
    connect(editBtn, &QPushButton::clicked, this, &WordBookPage::onEdit);
    connect(removeBtn, &QPushButton::clicked, this, &WordBookPage::onRemove);

    connect(&ctx, &AppContext::dataChanged, this, &WordBookPage::refresh);
}

int WordBookPage::selectedRow() const {
    const auto rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

qint64 WordBookPage::selectedWordId() const {
    const int row = selectedRow();
    if (row < 0) return -1;
    return m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
}

qint64 WordBookPage::currentBookId() const { return m_bookCombo->currentData().toLongLong(); }

void WordBookPage::reloadBooks(qint64 keepId) {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    if (keepId < 0) keepId = currentBookId();

    m_bookCombo->blockSignals(true);
    m_bookCombo->clear();
    const auto books = ctx().wordBookService().listBooks(uid);
    for (const auto &b : books)
        m_bookCombo->addItem(QStringLiteral("%1（%2）").arg(b.name).arg(b.entryCount), b.id);
    m_bookCombo->blockSignals(false);

    if (!books.isEmpty()) {
        int idx = m_bookCombo->findData(keepId);
        if (idx < 0) idx = 0;
        m_bookCombo->setCurrentIndex(idx);
    }
}

void WordBookPage::onBookChanged() { refresh(); }

void WordBookPage::refresh() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;

    // 单词本列表可能变化（新建/删除/条目数），保持选择
    const qint64 keep = currentBookId();
    reloadBooks(keep);

    const qint64 bookId = currentBookId();
    if (bookId <= 0) {
        ui::clearTable(m_table);
        m_countLabel->setText(QStringLiteral("请先新建一个单词本"));
        return;
    }

    WordQuery q;
    q.keyword = m_search->text();
    q.book = m_bookFilter->currentData().toString();
    q.familiarityMin = m_famFilter->currentData().toInt();
    q.limit = 500;
    q.orderBy = QStringLiteral("word_rank");

    const auto entries = ctx().wordBookService().listEntries(bookId, q, &m_total);
    ui::clearTable(m_table);
    m_table->setRowCount(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        QString ph = e.word.phoneticUk.isEmpty() ? e.word.phoneticUs : e.word.phoneticUk;
        if (!ph.isEmpty()) ph = QStringLiteral("/%1/").arg(ph);
        ui::setRow(m_table, i,
                   {e.word.spelling, ph, ui::shortMeaning(e.word, 36), displayName(e.word.book),
                    QString::number(e.familiarity), QString::number(e.reviewCount),
                    ui::fmtDate(e.nextReviewAt, QStringLiteral("未安排"))});
        m_table->item(i, 0)->setData(Qt::UserRole, e.wordId);
        m_table->item(i, 0)->setData(Qt::UserRole + 1, e.id);
    }
    m_table->resizeColumnsToContents();
    if (m_table->columnCount() > 2)
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_countLabel->setText(QStringLiteral("共 %1 词（显示 %2）").arg(m_total).arg(entries.size()));
}

void WordBookPage::onSearch() { refresh(); }

void WordBookPage::onNewBook() {
    const qint64 uid = ctx().currentUserId();
    if (uid <= 0) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建单词本"),
                                               QStringLiteral("单词本名称："), QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    auto created = ctx().wordBookService().createBook(uid, name.trimmed());
    if (!created) { ui::warn(this, created.error()); return; }
    reloadBooks(created.value().id);
    refresh();
    emit ctx().dataChanged();
}

void WordBookPage::onRenameBook() {
    const qint64 bookId = currentBookId();
    if (bookId <= 0) return;
    const QString old = m_bookCombo->currentText().section(QStringLiteral("（"), 0, 0);
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("重命名单词本"),
                                               QStringLiteral("新的名称："), QLineEdit::Normal, old, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    const auto st = ctx().wordBookService().renameBook(bookId, name.trimmed());
    if (!st.isOk()) { ui::warn(this, st.error()); return; }
    reloadBooks(bookId);
    refresh();
    emit ctx().dataChanged();
}

void WordBookPage::onDeleteBook() {
    const qint64 bookId = currentBookId();
    if (bookId <= 0) return;
    const QString name = m_bookCombo->currentText().section(QStringLiteral("（"), 0, 0);
    if (!ui::confirm(this, QStringLiteral("确定删除单词本「%1」？其中的收藏记录也会一并删除。").arg(name)))
        return;
    const auto st = ctx().wordBookService().removeBook(bookId);
    if (!st.isOk()) { ui::warn(this, st.error()); return; }
    reloadBooks(0);
    refresh();
    emit ctx().dataChanged();
}

void WordBookPage::onBatchAddBookWords() {
    const qint64 uid = ctx().currentUserId();
    const qint64 bookId = currentBookId();
    if (uid <= 0 || bookId <= 0) return;
    const QStringList options{QStringLiteral("全部词书"), QStringLiteral("四级"), QStringLiteral("六级")};
    bool ok = false;
    const QString which = QInputDialog::getItem(this, QStringLiteral("批量添加词书单词"),
                                                QStringLiteral("把哪个词书的全部单词加入当前单词本？"),
                                                options, 0, false, &ok);
    if (!ok) return;
    QString bookFilter;
    if (which == options[1]) bookFilter = QStringLiteral("CET4");
    else if (which == options[2]) bookFilter = QStringLiteral("CET6");

    const int added = ctx().wordBookService().addAllDictionaryWords(uid, bookFilter);
    ui::info(this, QStringLiteral("已加入 %1 个单词").arg(added));
    refresh();
    emit ctx().dataChanged();
}

void WordBookPage::onDetail() {
    const qint64 id = selectedWordId();
    if (id < 0) { ui::warn(this, QStringLiteral("请先选择一个单词")); return; }
    WordDialog dlg(ctx(), id, this);
    connect(&dlg, &WordDialog::wordChanged, this, &WordBookPage::refresh);
    dlg.exec();
    refresh();
}

void WordBookPage::onEdit() {
    const qint64 id = selectedWordId();
    if (id < 0) { ui::warn(this, QStringLiteral("请先选择一个单词")); return; }
    auto w = ctx().wordService().detail(id);
    if (!w) return;
    WordEditDialog dlg(ctx(), w.value(), false, this);
    if (dlg.exec() == QDialog::Accepted && dlg.saved()) {
        refresh();
        emit ctx().dataChanged();
    }
}

void WordBookPage::onRemove() {
    const qint64 bookId = currentBookId();
    const int row = selectedRow();
    if (bookId <= 0 || row < 0) return;
    const qint64 wordId = m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
    if (!ui::confirm(this, QStringLiteral("确定把该单词移出当前单词本？"))) return;
    const auto st = ctx().wordBookService().removeWord(bookId, wordId);
    if (!st.isOk()) ui::warn(this, st.error());
    refresh();
    emit ctx().dataChanged();
}

void WordBookPage::onContextMenu(const QPoint &pos) {
    if (selectedRow() < 0) return;
    QMenu menu(this);
    menu.addAction(QStringLiteral("查看详情"), this, &WordBookPage::onDetail);
    menu.addAction(QStringLiteral("编辑"), this, &WordBookPage::onEdit);
    menu.addSeparator();
    menu.addAction(QStringLiteral("移出本单词本"), this, &WordBookPage::onRemove);
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

} // namespace vt
