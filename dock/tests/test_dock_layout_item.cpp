#include "quick/BlurRegionController.hpp"
#include "quick/DockIconItem.hpp"
#include "quick/DockLayoutItem.hpp"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QSortFilterProxyModel>
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

    void testBlurCustomRegionOverridesRoundedPill() {
        BlurRegionController controller;
        controller.setWindowPillRect(QRectF(0.0, 0.0, 200.0, 50.0));
        controller.setCornerRadius(12.0);
        const QRegion rounded = controller.currentRegion();
        QVERIFY(!rounded.isEmpty());
        QCOMPARE(rounded, BlurRegionController::buildRoundedRegion(QRectF(0.0, 0.0, 200.0, 50.0), 12.0));

        // A FrameSvg-style mask replaces the rounded rect verbatim...
        const QRegion mask(QRect(5, 5, 40, 20));
        controller.setCustomRegion(mask);
        QCOMPARE(controller.currentRegion(), mask);

        // ...and clearing it falls back to the rounded pill.
        controller.setCustomRegion({});
        QCOMPARE(controller.currentRegion(), rounded);

        // Empty pill without a mask yields no blur region.
        controller.setWindowPillRect({});
        QVERIFY(controller.currentRegion().isEmpty());
    }

    void testBlurReapplyIsCoalescedAndSafeWithoutWindow() {
        BlurRegionController controller;
        controller.setWindowPillRect(QRectF(0.0, 0.0, 200.0, 50.0));
        // Many updates in one pass must not crash or apply synchronously without a window.
        for (int i = 0; i < 50; ++i) {
            controller.setWindowPillRect(QRectF(0.0, 0.0, 200.0 + i, 50.0));
            controller.setContrast(BlurContrast{true, 1.0 + i * 0.01, 1.0, 1.5});
        }
        controller.applyNow();
        QCoreApplication::processEvents();
        QVERIFY(!controller.currentRegion().isEmpty());
    }

    void testDockLayoutBlurMaskAcceptsRegionAndClearsOnUndefined() {
        DockLayoutItem item;
        QVERIFY(item.blurMask().value<QRegion>().isEmpty());
        item.setBlurMask(QVariant::fromValue(QRegion(QRect(0, 0, 10, 10))));
        QCOMPARE(item.blurMask().value<QRegion>(), QRegion(QRect(0, 0, 10, 10)));
        item.setBlurMask(QVariant());
        QVERIFY(item.blurMask().value<QRegion>().isEmpty());

        // Contrast setters round-trip.
        item.setBlurSaturation(1.8);
        QCOMPARE(item.blurSaturation(), 1.8);
        item.setBlurContrast(1.1);
        QCOMPARE(item.blurContrast(), 1.1);
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

    void testProxyModelLayoutChangedPreservesDelegatesAndSyncsIndices() {
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

        QStandardItemModel source;
        for (const QString &name : {QStringLiteral("Alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma")}) {
            source.appendRow(new QStandardItem(name));
        }

        QSortFilterProxyModel proxy;
        proxy.setSourceModel(&source);
        proxy.sort(0, Qt::AscendingOrder);

        DockLayoutItem dock;
        dock.setBaseIconSize(48.0);
        dock.setMaxScale(1.8);
        dock.setSize(QSizeF(800.0, 160.0));
        dock.setModel(&proxy);
        dock.setDelegate(&delegateComp);

        QCOMPARE(dock.count(), 3);
        QQuickItem *alphaItem = dock.itemAtIndex(0);
        QQuickItem *betaItem = dock.itemAtIndex(1);
        QQuickItem *gammaItem = dock.itemAtIndex(2);
        QVERIFY(alphaItem && betaItem && gammaItem);
        QCOMPARE(alphaItem->property("taskIndex").toInt(), 0);
        QCOMPARE(gammaItem->property("taskIndex").toInt(), 2);

        // Reverse sort order: QSortFilterProxyModel emits layoutChanged (not rowsMoved),
        // matching TaskManager::TasksModel::move().
        proxy.sort(0, Qt::DescendingOrder);

        QCOMPARE(dock.count(), 3);
        // Exact same delegate instances must be preserved in the new order.
        QCOMPARE(dock.itemAtIndex(0), gammaItem);
        QCOMPARE(dock.itemAtIndex(1), betaItem);
        QCOMPARE(dock.itemAtIndex(2), alphaItem);

        // taskIndex and modelIndex must be updated to match the proxy's new row indices.
        QCOMPARE(gammaItem->property("taskIndex").toInt(), 0);
        QCOMPARE(betaItem->property("taskIndex").toInt(), 1);
        QCOMPARE(alphaItem->property("taskIndex").toInt(), 2);

        const QModelIndex gammaIdx = gammaItem->property("modelIndex").toModelIndex();
        const QModelIndex alphaIdx = alphaItem->property("modelIndex").toModelIndex();
        QVERIFY(gammaIdx.isValid());
        QVERIFY(alphaIdx.isValid());
        QCOMPARE(gammaIdx.row(), 0);
        QCOMPARE(alphaIdx.row(), 2);
        QCOMPARE(proxy.mapToSource(gammaIdx).data().toString(), QStringLiteral("Gamma"));
        QCOMPARE(proxy.mapToSource(alphaIdx).data().toString(), QStringLiteral("Alpha"));
    }

    void testImplicitWidthDoesNotCollapseInFitContentPanel() {
        QQmlEngine engine;
        QQmlComponent delegateComp(&engine);
        delegateComp.setData(QByteArrayLiteral("import QtQuick\nItem { property int taskIndex: -1; property var modelIndex }"), QUrl());
        QVERIFY2(delegateComp.isReady(), qPrintable(delegateComp.errorString()));

        QStandardItemModel model;
        for (int i = 0; i < 5; ++i) {
            model.appendRow(new QStandardItem(QStringLiteral("App %1").arg(i)));
        }

        DockLayoutItem dock;
        dock.setAutoScaleToPanel(true);
        dock.setPanelIconRatio(0.65);
        dock.setMaxZoomPanelRatio(1.0);
        dock.setIconSpacingRatio(0.15);
        dock.setClampToContainer(true);
        dock.setEdgePadding(6.0);
        dock.setTopMargin(6.0);
        // Simulate a Fit-Content horizontal Plasma panel starting at minimum width (64 px)
        // with real panel height (80 px).
        dock.setSize(QSizeF(64.0, 80.0));
        dock.setModel(&model);
        dock.setDelegate(&delegateComp);

        // Available cross = 80 - 12 = 68 px -> baseIconSize = 68 * 0.65 = 44.2 px.
        // 5 icons + 4 gaps must request > 240 px implicitWidth, NOT stay trapped at ~64 px.
        QVERIFY2(
            dock.implicitWidth() > 240.0,
            qPrintable(QStringLiteral("implicitWidth collapsed to %1").arg(dock.implicitWidth()))
        );

        // Once the panel expands to the requested implicitWidth, icons render at full proportional size.
        dock.setSize(QSizeF(dock.implicitWidth(), 80.0));
        QVERIFY(std::abs(dock.effectiveBaseIconSize() - 44.2) < 0.1);
    }

    void testRowsMovedFollowedByInvalidatePreservesDraggedDelegateIdentity() {
        // Mirrors TaskManager::TasksModel::move(), which calls beginMoveRows/endMoveRows
        // and immediately follows with QSortFilterProxyModel::invalidate() (layoutChanged).
        class ManualSortProxy : public QSortFilterProxyModel {
        public:
            QList<int> order{0, 1, 2, 3, 4};

            void moveRowManual(int from, int to) {
                const int dest = (to > from) ? (to + 1) : to;
                beginMoveRows(QModelIndex(), from, from, QModelIndex(), dest);
                order.move(from, to);
                endMoveRows();
                invalidate();
            }

        protected:
            bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
                return order.indexOf(left.row()) < order.indexOf(right.row());
            }
        };

        QQmlEngine engine;
        QQmlComponent delegateComp(&engine);
        delegateComp.setData(
            R"(
            import QtQuick
            Item {
                property int taskIndex: -1
                property var modelIndex
                property string displayTitle: ""
            }
            )",
            QUrl()
        );
        QVERIFY2(delegateComp.isReady(), qPrintable(delegateComp.errorString()));

        QStandardItemModel source;
        for (const QString &name : {QStringLiteral("VSCode"), QStringLiteral("Claude"),
                                    QStringLiteral("Dolphin"), QStringLiteral("Ghostty"),
                                    QStringLiteral("T3")}) {
            source.appendRow(new QStandardItem(name));
        }

        ManualSortProxy proxy;
        proxy.setSourceModel(&source);
        proxy.sort(0);

        DockLayoutItem dock;
        dock.setSize(QSizeF(800.0, 100.0));
        dock.setModel(&proxy);
        dock.setDelegate(&delegateComp);

        QQuickItem *t3Item = dock.itemAtIndex(4);
        QVERIFY(t3Item != nullptr);
        QCOMPARE(t3Item->property("displayTitle").toString(), QStringLiteral("T3"));

        // Drag T3 from 4 -> 3, then 3 -> 2 (exact sequence from user bug report).
        proxy.moveRowManual(4, 3);
        QCOMPARE(dock.itemAtIndex(3), t3Item);
        QCOMPARE(t3Item->property("taskIndex").toInt(), 3);
        QCOMPARE(t3Item->property("displayTitle").toString(), QStringLiteral("T3"));

        proxy.moveRowManual(3, 2);
        QCOMPARE(dock.itemAtIndex(2), t3Item);
        QCOMPARE(t3Item->property("taskIndex").toInt(), 2);
        QCOMPARE(t3Item->property("displayTitle").toString(), QStringLiteral("T3"));
        QCOMPARE(dock.itemAtIndex(3)->property("displayTitle").toString(), QStringLiteral("Dolphin"));
        QCOMPARE(dock.itemAtIndex(4)->property("displayTitle").toString(), QStringLiteral("Ghostty"));
    }

    void testPinnedAndUnpinnedOpenCloseLifecycleReconciliation() {
        // Simulates TaskManager::TasksModel's Launcher <-> Startup <-> Window transitions.
        class MockTasksModel : public QAbstractListModel {
        public:
            enum Roles {
                AppIdRole = Qt::UserRole + 1,
                AppNameRole,
                StorageIdRole,
                LauncherUrlRole,
                IsLauncherRole,
                IsStartupRole,
                IsWindowRole,
            };

            struct Row {
                QString display;
                QString appId;
                QString appName;
                QString storageId;
                QUrl launcherUrl;
                bool isLauncher{false};
                bool isStartup{false};
                bool isWindow{false};
            };

            QList<Row> rows;

            int rowCount(const QModelIndex &parent = QModelIndex()) const override {
                return parent.isValid() ? 0 : static_cast<int>(rows.size());
            }

            QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
                if (!index.isValid() || index.row() < 0 || index.row() >= rows.size()) {
                    return {};
                }
                const auto &r = rows[index.row()];
                switch (role) {
                case Qt::DisplayRole: return r.display;
                case AppIdRole: return r.appId;
                case AppNameRole: return r.appName;
                case StorageIdRole: return r.storageId;
                case LauncherUrlRole: return r.launcherUrl;
                case IsLauncherRole: return r.isLauncher;
                case IsStartupRole: return r.isStartup;
                case IsWindowRole: return r.isWindow;
                default: return {};
                }
            }

            QHash<int, QByteArray> roleNames() const override {
                return {
                    {Qt::DisplayRole, "display"},
                    {AppIdRole, "AppId"},
                    {AppNameRole, "AppName"},
                    {StorageIdRole, "StorageId"},
                    {LauncherUrlRole, "LauncherUrlWithoutIcon"},
                    {IsLauncherRole, "IsLauncher"},
                    {IsStartupRole, "IsStartup"},
                    {IsWindowRole, "IsWindow"},
                };
            }

            void insertTask(int pos, Row r) {
                beginInsertRows(QModelIndex(), pos, pos);
                rows.insert(pos, std::move(r));
                endInsertRows();
            }

            void removeTask(int pos) {
                beginRemoveRows(QModelIndex(), pos, pos);
                rows.removeAt(pos);
                endRemoveRows();
            }
        };

        QQmlEngine engine;
        QQmlComponent delegateComp(&engine);
        delegateComp.setData(
            R"(
            import QtQuick
            Item {
                property int taskIndex: -1
                property var modelIndex
                property string displayTitle: ""
                property bool isLauncher: false
                property bool isStartup: false
                property bool isWindow: false
                property string appId: ""
                property string appName: ""
                property url launcherUrl
            }
            )",
            QUrl()
        );
        QVERIFY2(delegateComp.isReady(), qPrintable(delegateComp.errorString()));

        MockTasksModel model;
        model.rows = {
            {QStringLiteral("Vesktop"), QStringLiteral("vesktop"), QStringLiteral("Vesktop"),
             QStringLiteral("vesktop.desktop"), QUrl(QStringLiteral("applications:vesktop.desktop")), true, false, false},
            {QStringLiteral("Dolphin"), QStringLiteral("org.kde.dolphin"), QStringLiteral("Dolphin"),
             QStringLiteral("org.kde.dolphin.desktop"), QUrl(QStringLiteral("preferred://filemanager")), true, false, false},
            {QStringLiteral("T3"), QStringLiteral("t3-code"), QStringLiteral("T3"),
             QStringLiteral("t3-code.desktop"), QUrl(QStringLiteral("applications:t3-code.desktop")), false, false, true},
        };

        DockLayoutItem dock;
        dock.setSize(QSizeF(800.0, 100.0));
        dock.setModel(&model);
        dock.setDelegate(&delegateComp);

        QCOMPARE(dock.count(), 3);
        QQuickItem *dolphinItem = dock.itemAtIndex(1);
        QVERIFY(dolphinItem != nullptr);

        // 1. Launching pinned Dolphin: duplicate Startup row appears at end alongside Launcher.
        //    Must be suppressed (count stays 3, exact same Dolphin delegate stays in place).
        model.insertTask(3, {
            QStringLiteral("Dolphin"), QStringLiteral("org.kde.dolphin"), QStringLiteral("Dolphin"),
            QStringLiteral("org.kde.dolphin.desktop"), QUrl(QStringLiteral("applications:org.kde.dolphin.desktop")),
            false, true, false
        });
        QCoreApplication::processEvents();
        QCOMPARE(dock.count(), 3);
        QCOMPARE(dock.itemAtIndex(1), dolphinItem);

        // 2. Startup finishes & real Dolphin Window replaces pinned Launcher in same tick.
        //    Exact same Dolphin QQuickItem must be reused with zero ghost duplication.
        model.removeTask(3); // remove Startup
        model.removeTask(1); // hide Launcher
        model.insertTask(1, {
            QStringLiteral("Desktop — Dolphin"), QStringLiteral("org.kde.dolphin"), QStringLiteral("Dolphin"),
            QStringLiteral("org.kde.dolphin.desktop"), QUrl(QStringLiteral("applications:org.kde.dolphin.desktop")),
            false, false, true
        });
        QCoreApplication::processEvents();
        QCOMPARE(dock.count(), 3);
        QCOMPARE(dock.itemAtIndex(1), dolphinItem);
        QCOMPARE(dolphinItem->property("isWindow").toBool(), true);
        QCOMPARE(dolphinItem->property("isLauncher").toBool(), false);

        // 3. Closing pinned Dolphin: Window removed & Launcher re-inserted in same tick.
        //    Exact same Dolphin QQuickItem must be reused (zero shrink/slide ghost).
        model.removeTask(1);
        model.insertTask(1, {
            QStringLiteral("Dolphin"), QStringLiteral("org.kde.dolphin"), QStringLiteral("Dolphin"),
            QStringLiteral("org.kde.dolphin.desktop"), QUrl(QStringLiteral("preferred://filemanager")),
            true, false, false
        });
        QCoreApplication::processEvents();
        QCOMPARE(dock.count(), 3);
        QCOMPARE(dock.itemAtIndex(1), dolphinItem);
        QCOMPARE(dolphinItem->property("isLauncher").toBool(), true);
        QCOMPARE(dolphinItem->property("isWindow").toBool(), false);

        // 4. Opening a genuinely new unpinned app (Startup -> Window -> Close).
        model.insertTask(3, {
            QStringLiteral("Ghostty"), QStringLiteral("com.mitchellh.ghostty"), QStringLiteral("Ghostty"),
            QStringLiteral("com.mitchellh.ghostty.desktop"), QUrl(QStringLiteral("applications:com.mitchellh.ghostty.desktop")),
            false, true, false
        });
        QCoreApplication::processEvents();
        QCOMPARE(dock.count(), 4);
        QQuickItem *ghosttyItem = dock.itemAtIndex(3);
        QVERIFY(ghosttyItem != nullptr);

        // Startup -> Window reuses the exact same delegate seamlessly.
        model.removeTask(3);
        model.insertTask(3, {
            QStringLiteral("Ghostty"), QStringLiteral("com.mitchellh.ghostty"), QStringLiteral("Ghostty"),
            QStringLiteral("com.mitchellh.ghostty.desktop"), QUrl(QStringLiteral("applications:com.mitchellh.ghostty.desktop")),
            false, false, true
        });
        QCoreApplication::processEvents();
        QCOMPARE(dock.count(), 4);
        QCOMPARE(dock.itemAtIndex(3), ghosttyItem);

        // Closing unpinned Ghostty drops active count to 3 (smooth ghost-out).
        model.removeTask(3);
        QCoreApplication::processEvents();
        QCOMPARE(dock.count(), 3);
    }

    void testModelChangesDoNotRewriteUnchangedDelegateIcons() {
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
                property real delegateLogicalSize: 87
                property int iconWrites: 0
                property int titleWrites: 0
                onDecorationIconChanged: iconWrites += 1
                onDisplayTitleChanged: titleWrites += 1
            }
            )",
            QUrl()
        );
        QVERIFY2(delegateComp.isReady(), qPrintable(delegateComp.errorString()));

        QPixmap pix(16, 16);
        pix.fill(Qt::red);
        const QIcon icon(pix);

        QStandardItemModel model;
        for (int i = 0; i < 3; ++i) {
            auto *row = new QStandardItem(QStringLiteral("App %1").arg(i));
            row->setIcon(icon);
            model.appendRow(row);
        }

        DockLayoutItem dock;
        dock.setSize(QSizeF(800.0, 160.0));
        dock.setModel(&model);
        dock.setDelegate(&delegateComp);
        QCOMPARE(dock.count(), 3);

        QQuickItem *first = dock.itemAtIndex(0);
        QVERIFY(first != nullptr);
        const int iconWritesBefore = first->property("iconWrites").toInt();
        const int titleWritesBefore = first->property("titleWrites").toInt();

        // An app opening inserts a row; untouched delegates must not see their icon re-assigned
        // (that made every Kirigami.Icon reload and the whole dock flicker).
        auto *opened = new QStandardItem(QStringLiteral("Opened"));
        opened->setIcon(icon);
        model.appendRow(opened);
        QCOMPARE(dock.count(), 4);
        model.item(1)->setText(QStringLiteral("Renamed"));

        QCOMPARE(first->property("iconWrites").toInt(), iconWritesBefore);
        QCOMPARE(first->property("titleWrites").toInt(), titleWritesBefore);
        QCOMPARE(dock.itemAtIndex(1)->property("displayTitle").toString(), QStringLiteral("Renamed"));

        // A genuinely new icon still propagates.
        QPixmap pix2(16, 16);
        pix2.fill(Qt::blue);
        model.item(0)->setIcon(QIcon(pix2));
        QCOMPARE(first->property("iconWrites").toInt(), iconWritesBefore + 1);
    }

    void testLanczos3IconResamplerPreservesSharpnessAndPremultipliedBounds() {
        // High-contrast 256x256 icon with transparent border and crisp inner square.
        QImage src(256, 256, QImage::Format_RGBA8888_Premultiplied);
        src.fill(Qt::transparent);
        for (int y = 32; y < 224; ++y) {
            std::uint8_t *line = src.scanLine(y);
            for (int x = 32; x < 224; ++x) {
                const bool stripe = ((x / 16) % 2) == 0;
                line[x * 4 + 0] = stripe ? 255 : 20;
                line[x * 4 + 1] = stripe ? 255 : 140;
                line[x * 4 + 2] = stripe ? 255 : 240;
                line[x * 4 + 3] = 255;
            }
        }

        const QImage out = DockIconItem::resampleLanczos3(src, 25, 25);
        QCOMPARE(out.size(), QSize(25, 25));
        QCOMPARE(out.format(), QImage::Format_RGBA8888_Premultiplied);

        // Verify strict premultiplied-alpha bounds (0 <= r,g,b <= a) on every pixel.
        for (int y = 0; y < out.height(); ++y) {
            const std::uint8_t *line = out.constScanLine(y);
            for (int x = 0; x < out.width(); ++x) {
                const std::uint8_t r = line[x * 4 + 0];
                const std::uint8_t g = line[x * 4 + 1];
                const std::uint8_t b = line[x * 4 + 2];
                const std::uint8_t a = line[x * 4 + 3];
                QVERIFY(r <= a && g <= a && b <= a);
            }
        }

        // Outer corner remains transparent; center remains opaque.
        QCOMPARE(out.constScanLine(0)[3], std::uint8_t{0});
        QCOMPARE(out.constScanLine(12)[12 * 4 + 3], std::uint8_t{255});

        // Non-square source preserves aspect ratio inside square target.
        QImage wide(128, 64, QImage::Format_RGBA8888_Premultiplied);
        wide.fill(Qt::red);
        const QImage wideOut = DockIconItem::resampleLanczos3(wide, 24, 24);
        QCOMPARE(wideOut.size(), QSize(24, 24));
        QCOMPARE(wideOut.constScanLine(0)[12 * 4 + 3], std::uint8_t{0});
        QCOMPARE(wideOut.constScanLine(12)[12 * 4 + 3], std::uint8_t{255});
    }
};

QTEST_MAIN(DockLayoutItemTest)
#include "test_dock_layout_item.moc"
