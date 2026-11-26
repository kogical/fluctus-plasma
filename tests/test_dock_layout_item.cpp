#include "quick/BlurRegionController.hpp"
#include "quick/DockLayoutItem.hpp"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QStandardItemModel>
#include <QtTest/QtTest>

using namespace fluctus::quick;

class DockLayoutItemTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testBlurRoundedRegionInsetAndEmptyGuard() {
        // Too small rect produces empty region.
        QVERIFY(BlurRegionController::buildRoundedRegion(QRectF(0, 0, 2, 2), 8.0).isEmpty());

        // Normal pill rect produces non-empty region strictly inset by >= 1 px.
        const QRectF pill(10.0, 20.0, 300.0, 60.0);
        const QRegion region = BlurRegionController::buildRoundedRegion(pill, 16.0);
        QVERIFY(!region.isEmpty());
        const QRect bounds = region.boundingRect();
        QVERIFY(bounds.left() >= 11);
        QVERIFY(bounds.top() >= 21);
        QVERIFY(bounds.right() <= 309);
        QVERIFY(bounds.bottom() <= 79);
    }

    void testDelegateLifecycleAndScaleOnlyUpdates() {
        QQmlEngine engine;
        QQmlComponent delegateComp(&engine);
        delegateComp.setData(
            R"(
            import QtQuick
            Item {
                property int taskIndex: -1
                property var modelIndex
                property string displayTitle: ""
                property var decorationIcon
                property bool isActive: false
                property bool isLauncher: false
                property bool isWindow: false
                property bool isStartup: false
                property bool isDemandingAttention: false
                property bool isMinimized: false
                property bool isGroupParent: false
                property int childCount: 0
                property string appId: ""
                property url launcherUrl
                property int dockEdge: 0
                property real delegateLogicalSize: 87
                property real slotScale: 1.0
                property real bounceOffset: 0.0
            }
            )",
            QUrl()
        );
        QVERIFY2(delegateComp.isReady(), qPrintable(delegateComp.errorString()));

        QStandardItemModel model;
        for (int i = 0; i < 4; ++i) {
            auto *row = new QStandardItem(QStringLiteral("App %1").arg(i));
            model.appendRow(row);
        }

        DockLayoutItem dock;
        dock.setBaseIconSize(48.0);
        dock.setMaxScale(1.8);
        dock.setSize(QSizeF(800.0, 160.0));
        dock.setModel(&model);
        dock.setDelegate(&delegateComp);

        QCOMPARE(dock.count(), 4);
        QCOMPARE(dock.recommendedPanelThickness(), 147);

        QQuickItem *first = dock.itemAtIndex(0);
        QVERIFY(first != nullptr);
        QCOMPARE(first->property("displayTitle").toString(), QStringLiteral("App 0"));
        QCOMPARE(first->width(), dock.delegateLogicalSize());
        QCOMPARE(first->height(), dock.delegateLogicalSize());

        // Insert a row dynamically -> count increases and delegate is created.
        model.insertRow(2, new QStandardItem(QStringLiteral("Inserted")));
        QCOMPARE(dock.count(), 5);
        QQuickItem *inserted = dock.itemAtIndex(2);
        QVERIFY(inserted != nullptr);
        QCOMPARE(inserted->property("displayTitle").toString(), QStringLiteral("Inserted"));

        // Remove a row -> active count drops immediately (ghost slot animates out).
        model.removeRow(1);
        QCOMPARE(dock.count(), 4);
    }
};

QTEST_MAIN(DockLayoutItemTest)
#include "test_dock_layout_item.moc"
