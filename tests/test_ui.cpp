//
// test_ui.cpp - 界面样式测试（需要 QApplication）
//   重点验证：表格选中行必须显示为蓝色高亮
//
#include "ui/Theme.h"

#include <QApplication>
#include <QTableWidget>
#include <QtTest/QtTest>

using namespace vt;

namespace {
bool isAccentBlue(const QColor &c) {
    return c.blue() > 150 && c.blue() > c.red() + 60 && c.blue() > c.green() + 20;
}

// 统计（按设备像素比换算后的）矩形内蓝色像素占比
double accentRatio(const QImage &img, const QRect &logicalRect) {
    const qreal dpr = img.devicePixelRatio();
    const QRect dev = QRect(qRound(logicalRect.x() * dpr), qRound(logicalRect.y() * dpr),
                            qRound(logicalRect.width() * dpr), qRound(logicalRect.height() * dpr))
                          .intersected(img.rect());
    if (dev.isEmpty()) return 0.0;
    int total = 0, blue = 0;
    for (int y = dev.top(); y <= dev.bottom(); ++y) {
        for (int x = dev.left(); x <= dev.right(); ++x) {
            ++total;
            if (isAccentBlue(img.pixelColor(x, y))) ++blue;
        }
    }
    return total > 0 ? static_cast<double>(blue) / total : 0.0;
}
} // namespace

class TestUi : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        theme::apply(*qApp, QStringLiteral("light"));
        QVERIFY2(!qApp->styleSheet().isEmpty(), "样式表未生效");
    }

    void selectedRowIsHighlighted() {
        QTableWidget t(4, 3);
        t.setHorizontalHeaderLabels({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")});
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c)
                t.setItem(r, c, new QTableWidgetItem(QString())); // 空文本，便于取纯背景色
        t.resize(420, 240);
        t.show();
        QVERIFY(QTest::qWaitForWindowExposed(&t));
        QTest::qWait(120);

        t.selectRow(1);
        QTest::qWait(120);
        QVERIFY(t.selectionModel()->isSelected(t.model()->index(1, 1)));

        const QImage img = t.viewport()->grab().toImage();
        const double selectedRatio = accentRatio(img, t.visualItemRect(t.item(1, 1)));
        const double normalRatio = accentRatio(img, t.visualItemRect(t.item(3, 1)));

        qInfo("选中行蓝色占比=%.2f  未选中行蓝色占比=%.2f  DPR=%.2f", selectedRatio, normalRatio,
              img.devicePixelRatio());

        QVERIFY2(selectedRatio > 0.5,
                 qPrintable(QStringLiteral("选中行未显示蓝色高亮（占比 %1）").arg(selectedRatio)));
        QVERIFY2(normalRatio < 0.05,
                 qPrintable(QStringLiteral("未选中行不应为蓝色（占比 %1）").arg(normalRatio)));
    }

    void accentColorIsBlue() {
        const QColor accent(theme::accent());
        QVERIFY(accent.isValid());
        QVERIFY(accent.blue() > accent.red() + 60);
    }
};

QTEST_MAIN(TestUi)
#include "test_ui.moc"
