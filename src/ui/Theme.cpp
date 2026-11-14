#include "ui/Theme.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>

namespace vt {
namespace theme {

QString accent() { return QStringLiteral("#3B6DF6"); }
QString accentHover() { return QStringLiteral("#2F5BE0"); }
QString dangerColor() { return QStringLiteral("#E5484D"); }
QString successColor() { return QStringLiteral("#2FA36B"); }
QString warningColor() { return QStringLiteral("#D9822B"); }

QString styleSheet(const QString &name) {
    const bool dark = name.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0;

    const QString bg = dark ? QStringLiteral("#161920") : QStringLiteral("#F5F7FC");
    const QString panel = dark ? QStringLiteral("#1E222B") : QStringLiteral("#FFFFFF");
    const QString text = dark ? QStringLiteral("#E7EAF1") : QStringLiteral("#1E2430");
    const QString sub = dark ? QStringLiteral("#98A1B2") : QStringLiteral("#6C7484");
    const QString border = dark ? QStringLiteral("#2C323D") : QStringLiteral("#E4E8F2");
    const QString sidebar = dark ? QStringLiteral("#1B1F27") : QStringLiteral("#FFFFFF");
    const QString hover = dark ? QStringLiteral("#262C37") : QStringLiteral("#F0F3FA");
    const QString alt = dark ? QStringLiteral("#222833") : QStringLiteral("#FAFBFE");
    const QString tableHead = dark ? QStringLiteral("#222833") : QStringLiteral("#F7F9FD");
    const QString acc = dark ? QStringLiteral("#5B86FF") : QStringLiteral("#3B6DF6");
    const QString accHover = dark ? QStringLiteral("#6E96FF") : QStringLiteral("#2F5BE0");
    const QString accPressed = dark ? QStringLiteral("#4A73E6") : QStringLiteral("#274FCC");
    const QString accSoft = dark ? QStringLiteral("#223052") : QStringLiteral("#EDF2FF");
    const QString accSoftBorder = dark ? QStringLiteral("#33507F") : QStringLiteral("#C9D7FB");
    const QString danger = dark ? QStringLiteral("#F06A6F") : QStringLiteral("#E5484D");
    const QString disabled = dark ? QStringLiteral("#2A303B") : QStringLiteral("#E9ECF3");

    QString css = QStringLiteral(R"QSS(
* { font-family: "Microsoft YaHei UI", "Segoe UI", "PingFang SC", "Noto Sans CJK SC", sans-serif; font-size: 13px; }
QWidget { background: @BG@; color: @TEXT@; }
QMainWindow, QDialog { background: @BG@; }
QLabel { background: transparent; }
QToolTip { background: @PANEL@; color: @TEXT@; border: 1px solid @BORDER@; padding: 6px 8px; border-radius: 6px; }

/* ---------- 侧边栏 ---------- */
#Sidebar { background: @SIDEBAR@; border-right: 1px solid @BORDER@; }
#SidebarLogo { font-size: 19px; font-weight: 800; color: @ACCENT@; padding: 22px 18px 16px 18px; letter-spacing: 2px; }
#NavButton {
    text-align: left; padding: 11px 16px; margin: 2px 10px;
    border: none; border-radius: 10px; color: @TEXT@; background: transparent; font-size: 14px;
}
#NavButton:hover { background: @HOVER@; }
#NavButton:checked {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @ACCENT@, stop:1 @ACCENT_HOVER@);
    color: #FFFFFF; font-weight: 700;
}

/* ---------- 排版 ---------- */
#Header { font-size: 21px; font-weight: 800; padding: 2px 0 4px 0; letter-spacing: 0.5px; }
#SubHeader { color: @SUB@; font-size: 12px; }
#BrandTitle { font-size: 30px; font-weight: 800; color: @ACCENT@; letter-spacing: 6px; }
#StatCard { background: @PANEL@; border: 1px solid @BORDER@; border-radius: 14px; }

/* ---------- 按钮 ---------- */
QPushButton {
    background: @ACCENT@; color: #FFFFFF; border: none; border-radius: 8px;
    padding: 8px 16px; font-size: 13px; font-weight: 600;
}
QPushButton:hover { background: @ACCENT_HOVER@; }
QPushButton:pressed { background: @ACCENT_PRESSED@; }
QPushButton:disabled { background: @DISABLED@; color: @SUB@; }
QPushButton[flat="true"] { background: transparent; color: @ACCENT@; border: 1px solid @ACCENT@; }
QPushButton[flat="true"]:hover { background: @ACCENT_SOFT@; }
QPushButton[secondary="true"] { background: @HOVER@; color: @TEXT@; }
QPushButton[secondary="true"]:hover { background: @BORDER@; }
QPushButton[danger="true"] { background: @DANGER@; color: #FFFFFF; }
QPushButton[danger="true"]:hover { background: #C93C41; }

/* ---------- 输入控件 ---------- */
QLineEdit, QComboBox, QSpinBox, QPlainTextEdit, QTextEdit {
    background: @PANEL@; border: 1px solid @BORDER@; border-radius: 8px;
    padding: 7px 10px; color: @TEXT@;
    selection-background-color: @ACCENT@; selection-color: #FFFFFF;
}
QLineEdit:hover, QComboBox:hover, QSpinBox:hover { border-color: @ACCENT_SOFT_BORDER@; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border: 1px solid @ACCENT@;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background: @PANEL@; border: 1px solid @BORDER@; outline: none;
    selection-background-color: @ACCENT_SOFT@; selection-color: @ACCENT@;
}
QSpinBox::up-button, QSpinBox::down-button { width: 16px; border: none; background: transparent; }

/* ---------- 列表 ---------- */
QListWidget { background: @PANEL@; border: 1px solid @BORDER@; border-radius: 10px; outline: none; padding: 4px; }
QListWidget::item { padding: 8px 10px; border-radius: 8px; }
QListWidget::item:hover { background: @HOVER@; }
QListWidget::item:selected { background: @ACCENT_SOFT@; color: @ACCENT@; }

/* ---------- 表格 ---------- */
QTableWidget, QTableView {
    background: @PANEL@; border: 1px solid @BORDER@; border-radius: 10px;
    gridline-color: @BORDER@; alternate-background-color: @ALT@;
    selection-background-color: @ACCENT@; selection-color: #FFFFFF;
}
QHeaderView::section {
    background: @TABLE_HEAD@; color: @SUB@; padding: 9px 8px;
    border: none; border-bottom: 1px solid @BORDER@; font-weight: 700;
}
QTableCornerButton::section { background: @TABLE_HEAD@; border: none; }
/* 选中行高亮：必须显式指定 ::item:selected，否则自定义 ::item 会覆盖选中颜色 */
QTableWidget::item, QTableView::item { padding: 6px; border: none; }
QTableWidget::item:selected, QTableView::item:selected,
QTableWidget::item:selected:active, QTableView::item:selected:active,
QTableWidget::item:selected:!active, QTableView::item:selected:!active {
    background: @ACCENT@; color: #FFFFFF;
}

/* ---------- 分组框 ---------- */
QGroupBox {
    background: @PANEL@; border: 1px solid @BORDER@; border-radius: 12px;
    margin-top: 16px; padding: 14px; font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin; left: 14px; padding: 0 6px;
    color: @SUB@; background: @BG@;
}

/* ---------- 其它 ---------- */
QTabWidget::pane { border: 1px solid @BORDER@; border-radius: 10px; }
QTabBar::tab { padding: 9px 18px; border: none; background: transparent; color: @SUB@; }
QTabBar::tab:selected { color: @ACCENT@; border-bottom: 2px solid @ACCENT@; font-weight: 700; }

QCheckBox, QRadioButton { spacing: 8px; }
QCheckBox::indicator {
    width: 16px; height: 16px; border: 1px solid @BORDER@; border-radius: 4px; background: @PANEL@;
}
QCheckBox::indicator:hover { border-color: @ACCENT@; }
QCheckBox::indicator:checked { background: @ACCENT@; border-color: @ACCENT@; }
QRadioButton::indicator {
    width: 16px; height: 16px; border: 1px solid @BORDER@; border-radius: 8px; background: @PANEL@;
}
QRadioButton::indicator:hover { border-color: @ACCENT@; }
QRadioButton::indicator:checked { background: @ACCENT@; border-color: @ACCENT@; }

QProgressBar {
    border: 1px solid @BORDER@; border-radius: 8px; text-align: center;
    background: @PANEL@; height: 18px;
}
QProgressBar::chunk { background: @ACCENT@; border-radius: 7px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @BORDER@; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: @SUB@; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: @BORDER@; border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: @SUB@; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

QScrollArea { border: none; background: transparent; }
)QSS");

    css.replace(QStringLiteral("@BG@"), bg);
    css.replace(QStringLiteral("@PANEL@"), panel);
    css.replace(QStringLiteral("@TEXT@"), text);
    css.replace(QStringLiteral("@SUB@"), sub);
    css.replace(QStringLiteral("@BORDER@"), border);
    css.replace(QStringLiteral("@SIDEBAR@"), sidebar);
    css.replace(QStringLiteral("@HOVER@"), hover);
    css.replace(QStringLiteral("@ALT@"), alt);
    css.replace(QStringLiteral("@TABLE_HEAD@"), tableHead);
    css.replace(QStringLiteral("@ACCENT@"), acc);
    css.replace(QStringLiteral("@ACCENT_HOVER@"), accHover);
    css.replace(QStringLiteral("@ACCENT_PRESSED@"), accPressed);
    css.replace(QStringLiteral("@ACCENT_SOFT@"), accSoft);
    css.replace(QStringLiteral("@ACCENT_SOFT_BORDER@"), accSoftBorder);
    css.replace(QStringLiteral("@DANGER@"), danger);
    css.replace(QStringLiteral("@DISABLED@"), disabled);
    return css;
}

void apply(QApplication &app, const QString &name) {
    const bool dark = name.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0;
    QPalette pal = app.palette();
    if (dark) {
        pal.setColor(QPalette::Window, QColor(QStringLiteral("#161920")));
        pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#E7EAF1")));
        pal.setColor(QPalette::Base, QColor(QStringLiteral("#1E222B")));
        pal.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#222833")));
        pal.setColor(QPalette::Text, QColor(QStringLiteral("#E7EAF1")));
        pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#E7EAF1")));
        pal.setColor(QPalette::Highlight, QColor(QStringLiteral("#5B86FF")));
        pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
    } else {
        pal.setColor(QPalette::Window, QColor(QStringLiteral("#F5F7FC")));
        pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#1E2430")));
        pal.setColor(QPalette::Base, QColor(QStringLiteral("#FFFFFF")));
        pal.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#FAFBFE")));
        pal.setColor(QPalette::Text, QColor(QStringLiteral("#1E2430")));
        pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#1E2430")));
        pal.setColor(QPalette::Highlight, QColor(QStringLiteral("#3B6DF6")));
        pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
    }
    app.setPalette(pal);
    app.setStyleSheet(styleSheet(name));
}

} // namespace theme
} // namespace vt
