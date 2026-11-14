#include "ui/pages/DictionaryPage.h"

#include "core/AppContext.h"
#include "core/Paths.h"
#include "core/Utils.h"
#include "importer/JsonlImporter.h"
#include "ui/Navigation.h"
#include "ui/UiHelpers.h"
#include "ui/dialogs/WordBookPicker.h"
#include "ui/dialogs/WordDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QProgressDialog>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace vt {

DictionaryPage::DictionaryPage(AppContext &ctx, Navigator *nav, QWidget *parent) : Page(ctx, nav, parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("查询字典"), this);
    title->setObjectName(QStringLiteral("Header"));
    m_info = new QLabel(this);
    m_info->setObjectName(QStringLiteral("SubHeader"));
    header->addWidget(title);
    header->addSpacing(12);
    header->addWidget(m_info);
    header->addStretch();
    auto *importBtn = new QPushButton(QStringLiteral("导入词表"), this);
    importBtn->setProperty("flat", true);
    header->addWidget(importBtn);
    root->addLayout(header);

    // ---- 搜索栏 ----
    auto *bar = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("输入英文单词或中文释义，实时联想…"));
    m_search->setMinimumWidth(280);
    m_bookFilter = new QComboBox(this);
    m_bookFilter->addItem(QStringLiteral("全部词书"), QString());
    m_bookFilter->addItem(QStringLiteral("四级"), QStringLiteral("CET4"));
    m_bookFilter->addItem(QStringLiteral("六级"), QStringLiteral("CET6"));
    auto *searchBtn = new QPushButton(QStringLiteral("搜索"), this);
    bar->addWidget(m_search, 1);
    bar->addWidget(m_bookFilter);
    bar->addWidget(searchBtn);
    root->addLayout(bar);

    // ---- 实时联想列表（最多 5 条，位于搜索框正下方）----
    m_suggestions = new QListWidget(this);
    m_suggestions->setMaximumHeight(150);
    m_suggestions->setVisible(false);
    m_suggestions->setFocusPolicy(Qt::NoFocus);
    root->addWidget(m_suggestions);

    m_table = ui::makeTable({QStringLiteral("单词"), QStringLiteral("音标"), QStringLiteral("释义"),
                             QStringLiteral("词书")},
                            this);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    root->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *detailBtn = new QPushButton(QStringLiteral("查看详情"), this);
    detailBtn->setProperty("secondary", true);
    auto *addWordBtn = new QPushButton(QStringLiteral("加入单词本"), this);
    actions->addWidget(detailBtn);
    actions->addStretch();
    actions->addWidget(addWordBtn);
    root->addLayout(actions);

    // 联想防抖
    m_suggestTimer = new QTimer(this);
    m_suggestTimer->setSingleShot(true);
    m_suggestTimer->setInterval(120);
    connect(m_suggestTimer, &QTimer::timeout, this, &DictionaryPage::updateSuggestions);
    connect(m_search, &QLineEdit::textEdited, this, &DictionaryPage::onTextEdited);
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        m_suggestions->setVisible(false);
        onSearch();
    });

    connect(searchBtn, &QPushButton::clicked, this, [this] {
        m_suggestions->setVisible(false);
        onSearch();
    });
    connect(m_bookFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { onSearch(); });
    connect(importBtn, &QPushButton::clicked, this, &DictionaryPage::onImportDict);
    connect(m_table, &QTableWidget::doubleClicked, this, &DictionaryPage::onDetail);
    connect(m_table, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (selectedRow() < 0) return;
        QMenu menu(this);
        menu.addAction(QStringLiteral("查看详情"), this, &DictionaryPage::onDetail);
        menu.addAction(QStringLiteral("加入单词本"), this, &DictionaryPage::onAddWord);
        menu.exec(m_table->viewport()->mapToGlobal(pos));
    });
    connect(detailBtn, &QPushButton::clicked, this, &DictionaryPage::onDetail);
    connect(addWordBtn, &QPushButton::clicked, this, &DictionaryPage::onAddWord);
    connect(m_suggestions, &QListWidget::itemClicked, this, &DictionaryPage::onSuggestionClicked);

    m_search->setFocus();
    onSearch();
}

int DictionaryPage::selectedRow() const {
    const auto rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

qint64 DictionaryPage::selectedWordId() const {
    const int row = selectedRow();
    if (row < 0) return -1;
    return m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
}

void DictionaryPage::refresh() { onSearch(); }

void DictionaryPage::onTextEdited() { m_suggestTimer->start(); }

// ---- 实时联想：显示最可能查询的 5 个单词 ----
void DictionaryPage::updateSuggestions() {
    const QString kw = m_search->text().trimmed();
    if (kw.isEmpty()) {
        m_suggestions->setVisible(false);
        m_suggestions->clear();
        return;
    }
    const QString book = m_bookFilter->currentData().toString();
    const auto words = ctx().dictionaryService().suggest(kw, book, 5);
    m_suggestions->clear();
    for (const auto &w : words) {
        QString ph = w.phoneticUk.isEmpty() ? w.phoneticUs : w.phoneticUk;
        if (!ph.isEmpty()) ph = QStringLiteral("  /%1/").arg(ph);
        const QString text = QStringLiteral("%1%2    %3")
                                 .arg(w.spelling, ph, ui::shortMeaning(w, 24));
        auto *item = new QListWidgetItem(text, m_suggestions);
        item->setData(Qt::UserRole, w.id);
        item->setData(Qt::UserRole + 1, w.spelling);
        item->setToolTip(w.spelling);
    }
    m_suggestions->setVisible(!words.isEmpty());
}

void DictionaryPage::onSuggestionClicked() {
    auto *item = m_suggestions->currentItem();
    if (!item) return;
    const qint64 wordId = item->data(Qt::UserRole).toLongLong();
    const QString spelling = item->data(Qt::UserRole + 1).toString();
    jumpToWord(wordId, spelling);
}

// 跳转到指定单词：填入搜索框 → 刷新结果 → 选中该行 → 打开详情
void DictionaryPage::jumpToWord(qint64 wordId, const QString &spelling) {
    m_suggestions->setVisible(false);
    m_search->setText(spelling);
    onSearch();

    for (int i = 0; i < m_table->rowCount(); ++i) {
        if (m_table->item(i, 0)->data(Qt::UserRole).toLongLong() == wordId) {
            m_table->selectRow(i);
            m_table->scrollToItem(m_table->item(i, 0), QAbstractItemView::PositionAtCenter);
            break;
        }
    }

    WordDialog dlg(ctx(), wordId, this);
    dlg.exec();
    emit ctx().dataChanged();
    refresh();
}

void DictionaryPage::onSearch() {
    const QString kw = m_search->text().trimmed();
    if (kw.isEmpty()) {
        ui::clearTable(m_table);
        m_info->setText(QStringLiteral("词库共 %1 词　·　输入关键词开始查询")
                            .arg(ctx().wordService().count()));
        return;
    }
    const QString book = m_bookFilter->currentData().toString();
    const auto words = ctx().dictionaryService().search(kw, book, 200);
    ui::clearTable(m_table);
    m_table->setRowCount(words.size());
    for (int i = 0; i < words.size(); ++i) {
        const auto &w = words[i];
        QString ph = w.phoneticUk.isEmpty() ? w.phoneticUs : w.phoneticUk;
        if (!ph.isEmpty()) ph = QStringLiteral("/%1/").arg(ph);
        ui::setRow(m_table, i, {w.spelling, ph, ui::shortMeaning(w, 44), displayName(w.book)});
        m_table->item(i, 0)->setData(Qt::UserRole, w.id);
    }
    m_table->resizeColumnsToContents();
    if (m_table->columnCount() > 2)
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_info->setText(QStringLiteral("词库共 %1 词　·　找到 %2 个结果")
                        .arg(ctx().wordService().count())
                        .arg(words.size()));
}

void DictionaryPage::onImportDict() {
    if (m_importing) return;
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("导入词表到词库（JSONL）"),
                                                      Paths::dictDir(),
                                                      QStringLiteral("JSONL 词表 (*.jsonl);;所有文件 (*)"));
    if (file.isEmpty()) return;

    m_importing = true;
    QProgressDialog progress(QStringLiteral("正在导入词库…"), QString(), 0, 100, this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    QApplication::processEvents();

    const auto stats = ctx().wordService().importJsonl(file, false, [&](int done, int total) {
        if (total > 0) progress.setValue(int(100.0 * done / total));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    });
    progress.setValue(100);
    m_importing = false;

    if (stats.failed < 0) {
        ui::error(this, QStringLiteral("无法打开词表文件：\n%1").arg(file));
        return;
    }
    ui::info(this, QStringLiteral("导入完成\n\n词库：新增 %1 条，更新 %2 条\n用时：%3 ms\n\n词库总数：%4")
                      .arg(stats.inserted).arg(stats.updated).arg(stats.elapsedMs)
                      .arg(ctx().wordService().count()));
    onSearch();
    emit ctx().dataChanged();
}

void DictionaryPage::onDetail() {
    const qint64 id = selectedWordId();
    if (id < 0) { ui::warn(this, QStringLiteral("请先选择一个单词")); return; }
    WordDialog dlg(ctx(), id, this);
    dlg.exec();
    emit ctx().dataChanged();
    onSearch();
}

void DictionaryPage::onAddWord() {
    const qint64 id = selectedWordId();
    const qint64 uid = ctx().currentUserId();
    if (id < 0) { ui::warn(this, QStringLiteral("请先选择一个单词")); return; }
    if (uid <= 0) return;
    const QString book = ui::addWordToChosenBook(ctx(), uid, id, this);
    if (!book.isEmpty()) {
        ui::info(this, QStringLiteral("已加入单词本「%1」").arg(book));
        emit ctx().dataChanged();
    }
}

} // namespace vt
