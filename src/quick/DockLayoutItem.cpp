#include "quick/DockLayoutItem.hpp"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace fluctus::quick {

namespace {

[[nodiscard]] QRectF toQRectF(const core::Rect &r) noexcept {
    return QRectF{r.x, r.y, r.width, r.height};
}

} // namespace

DockLayoutItem::DockLayoutItem(QQuickItem *parent)
    : QQuickItem(parent)
    , m_driver(new DockAnimationDriver(this))
    , m_blurController(new BlurRegionController(this)) {
    setClip(false);
    m_engine.setConfig(m_config);

    m_reconcileTimer.setSingleShot(true);
    m_reconcileTimer.setInterval(0);
    connect(&m_reconcileTimer, &QTimer::timeout, this, &DockLayoutItem::reconcileModel);

    m_driver->setTickCallback([this](double dtSeconds) {
        const bool active = m_engine.step(dtSeconds, width(), height());
        const core::DockFrame frame = m_engine.computeFrame(width(), height());
        applyCurrentFrame(frame);

        // Clean up delegates whose ghost slots have finished animating out.
        const std::size_t prunedGhosts = std::erase_if(m_delegates, [&frame](const ManagedDelegate &entry) {
            if (!entry.ghost) {
                return false;
            }
            const bool stillInEngine = std::any_of(
                frame.items.begin(),
                frame.items.end(),
                [id = entry.slotId](const core::DockSlotLayout &s) { return s.id == id; }
            );
            if (!stillInEngine) {
                if (entry.item) {
                    entry.item->setVisible(false);
                    entry.item->deleteLater();
                }
                return true;
            }
            return false;
        });

        if (prunedGhosts > 0) {
            updateStructuralImplicitSize();
        }

        const bool settled = m_engine.isSettled();
        if (settled) {
            publishDelegateGeometries();
        }
        return active && !settled;
    });
}

DockLayoutItem::~DockLayoutItem() {
    m_reconcileTimer.stop();
    m_driver->stopImmediately();
    clearAllDelegates();
}

void DockLayoutItem::setModel(QAbstractItemModel *model) {
    if (m_model == model) {
        return;
    }

    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }

    m_model = model;

    if (m_model) {
        connect(m_model, &QAbstractItemModel::modelReset, this, &DockLayoutItem::onModelReset);
        connect(m_model, &QAbstractItemModel::rowsInserted, this, &DockLayoutItem::onRowsInserted);
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, &DockLayoutItem::onRowsRemoved);
        connect(m_model, &QAbstractItemModel::rowsMoved, this, &DockLayoutItem::onRowsMoved);
        connect(m_model, &QAbstractItemModel::layoutChanged, this, &DockLayoutItem::onLayoutChanged);
        connect(m_model, &QAbstractItemModel::dataChanged, this, &DockLayoutItem::onDataChanged);
    }

    Q_EMIT modelChanged();
    if (isComponentComplete()) {
        rebuildAllDelegates();
    }
}

void DockLayoutItem::setDelegate(QQmlComponent *delegate) {
    if (m_delegate == delegate) {
        return;
    }
    m_delegate = delegate;
    Q_EMIT delegateChanged();
    if (isComponentComplete()) {
        rebuildAllDelegates();
    }
}

void DockLayoutItem::setEdge(int edge) {
    const int clamped = std::clamp(edge, 0, 3);
    const auto newEdge = static_cast<core::DockEdge>(clamped);
    if (m_config.axis.edge == newEdge) {
        return;
    }
    m_config.axis.edge = newEdge;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setReversePrimary(bool reverse) {
    if (m_config.axis.reversePrimary == reverse) {
        return;
    }
    m_config.axis.reversePrimary = reverse;
    m_engine.setConfig(m_config);
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setBaseIconSize(qreal size) {
    const double clamped = std::clamp<double>(size, 16.0, 128.0);
    if (qFuzzyCompare(m_config.baseIconSize, clamped)) {
        return;
    }
    m_config.baseIconSize = clamped;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setIconSpacing(qreal spacing) {
    const double clamped = std::clamp<double>(spacing, 0.0, 48.0);
    if (qFuzzyCompare(m_config.iconSpacing, clamped)) {
        return;
    }
    m_config.iconSpacing = clamped;
    m_engine.setConfig(m_config);
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setMaxScale(qreal scale) {
    const double clamped = std::clamp<double>(scale, 1.0, 2.5);
    if (qFuzzyCompare(m_config.maxScale, clamped)) {
        return;
    }
    m_config.maxScale = clamped;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setInfluenceRadiusIcons(qreal radiusIcons) {
    const double clamped = std::clamp<double>(radiusIcons, 1.0, 4.0);
    if (qFuzzyCompare(m_config.influenceRadiusIcons, clamped)) {
        return;
    }
    m_config.influenceRadiusIcons = clamped;
    m_engine.setConfig(m_config);
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setMagnificationCurve(int curve) {
    const int clamped = std::clamp(curve, 0, 2);
    const auto newCurve = static_cast<core::MagnificationCurve>(clamped);
    if (m_config.curve == newCurve) {
        return;
    }
    m_config.curve = newCurve;
    m_engine.setConfig(m_config);
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setBounceEnabled(bool enabled) {
    if (m_config.bounceEnabled == enabled) {
        return;
    }
    m_config.bounceEnabled = enabled;
    m_engine.setConfig(m_config);
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setBounceRatio(qreal ratio) {
    const double clamped = std::clamp<double>(ratio, 0.0, 1.0);
    if (qFuzzyCompare(m_config.bounceRatio, clamped)) {
        return;
    }
    m_config.bounceRatio = clamped;
    m_engine.setConfig(m_config);
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setMagnificationEnabled(bool enabled) {
    if (m_config.magnificationEnabled == enabled) {
        return;
    }
    m_config.magnificationEnabled = enabled;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    m_driver->ensureRunning();
    Q_EMIT configChanged();
}

void DockLayoutItem::setEditModeFrozen(bool frozen) {
    if (m_config.editModeFrozen == frozen) {
        return;
    }
    m_config.editModeFrozen = frozen;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    m_driver->ensureRunning();
    Q_EMIT configChanged();
}

void DockLayoutItem::setAutoScaleToPanel(bool enabled) {
    if (m_config.autoScaleToPanel == enabled) {
        return;
    }
    m_config.autoScaleToPanel = enabled;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setPanelIconRatio(qreal ratio) {
    const double clamped = std::clamp<double>(ratio, 0.25, 1.0);
    if (qFuzzyCompare(m_config.panelIconRatio, clamped)) {
        return;
    }
    m_config.panelIconRatio = clamped;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setMaxZoomPanelRatio(qreal ratio) {
    const double clamped = std::clamp<double>(ratio, 0.25, 1.0);
    if (qFuzzyCompare(m_config.maxZoomPanelRatio, clamped)) {
        return;
    }
    m_config.maxZoomPanelRatio = clamped;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setIconSpacingRatio(qreal ratio) {
    const double clamped = std::clamp<double>(ratio, 0.0, 0.75);
    if (qFuzzyCompare(m_config.iconSpacingRatio, clamped)) {
        return;
    }
    m_config.iconSpacingRatio = clamped;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setClampToContainer(bool enabled) {
    if (m_config.clampToContainer == enabled) {
        return;
    }
    m_config.clampToContainer = enabled;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setBlurEnabled(bool enabled) {
    if (m_blurEnabled == enabled) {
        return;
    }
    m_blurEnabled = enabled;
    m_blurController->setEnabled(enabled);
    Q_EMIT blurConfigChanged();
}

void DockLayoutItem::setBlurStyle(int style) {
    const int clamped = std::clamp(style, 0, 4);
    if (m_blurStyle == clamped) {
        return;
    }
    m_blurStyle = clamped;
    m_blurController->setBlurStyle(clamped);
    Q_EMIT blurConfigChanged();
}

void DockLayoutItem::setPillCornerRadius(qreal radius) {
    const qreal clamped = std::max<qreal>(0.0, radius);
    if (qFuzzyCompare(m_pillCornerRadius, clamped)) {
        return;
    }
    m_pillCornerRadius = clamped;
    m_blurController->setCornerRadius(clamped);
    Q_EMIT blurConfigChanged();
}

void DockLayoutItem::setEdgePadding(qreal padding) {
    const double clamped = std::max<double>(0.0, padding);
    if (qFuzzyCompare(m_config.edgePadding, clamped)) {
        return;
    }
    m_config.edgePadding = clamped;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setPillPaddingPrimary(qreal padding) {
    const double clamped = std::max<double>(0.0, padding);
    if (qFuzzyCompare(m_config.pillPaddingPrimary, clamped)) {
        return;
    }
    m_config.pillPaddingPrimary = clamped;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setPillPaddingCross(qreal padding) {
    const double clamped = std::max<double>(0.0, padding);
    if (qFuzzyCompare(m_config.pillPaddingCross, clamped)) {
        return;
    }
    m_config.pillPaddingCross = clamped;
    m_engine.setConfig(m_config);
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::setTopMargin(qreal margin) {
    const double clamped = std::max<double>(0.0, margin);
    if (qFuzzyCompare(m_config.topMargin, clamped)) {
        return;
    }
    m_config.topMargin = clamped;
    m_engine.setConfig(m_config);
    syncAllDelegateSizesAndOrigins();
    updateStructuralImplicitSize();
    polish();
    Q_EMIT configChanged();
}

void DockLayoutItem::updatePointer(qreal x, qreal y) {
    m_engine.updatePointer(core::Vec2{x, y}, width(), height());
    m_driver->ensureRunning();
}

void DockLayoutItem::clearPointer() {
    m_engine.clearPointer();
    m_driver->ensureRunning();
}

int DockLayoutItem::restingInsertionIndex(qreal x, qreal y) const {
    return static_cast<int>(m_engine.restingInsertionIndex(core::Vec2{x, y}, width(), height()));
}

int DockLayoutItem::restingDragTargetIndex(qreal x, qreal y, int currentIndex) const {
    return static_cast<int>(
        m_engine.restingDragTargetIndex(core::Vec2{x, y}, width(), height(), currentIndex)
    );
}

QRectF DockLayoutItem::insertionIndicatorRect(int insertionIndex) const {
    const auto idx = static_cast<std::size_t>(std::max(0, insertionIndex));
    return toQRectF(m_engine.insertionIndicatorRect(idx, width(), height()));
}

QQuickItem *DockLayoutItem::itemAtIndex(int activeIndex) const {
    if (activeIndex < 0) {
        return nullptr;
    }
    int seen = 0;
    for (const auto &entry : m_delegates) {
        if (!entry.ghost) {
            if (seen == activeIndex) {
                return entry.item;
            }
            ++seen;
        }
    }
    return nullptr;
}

void DockLayoutItem::reapplyBlur() {
    updateBlurRegion();
    m_blurController->reapply();
}

void DockLayoutItem::publishDelegateGeometries() {
    if (!m_model || !window()) {
        return;
    }

    const core::DockFrame frame = m_engine.computeFrame(width(), height());
    for (const auto &entry : m_delegates) {
        if (entry.ghost || !entry.modelIndex.isValid() || !entry.item) {
            continue;
        }
        const auto it = std::find_if(
            frame.items.begin(),
            frame.items.end(),
            [id = entry.slotId](const core::DockSlotLayout &s) { return s.id == id; }
        );
        if (it == frame.items.end()) {
            continue;
        }
        const QPointF topLeftGlobal = mapToGlobal(QPointF(it->restingRect.x, it->restingRect.y));
        const QRect globalRect(
            static_cast<int>(std::round(topLeftGlobal.x())),
            static_cast<int>(std::round(topLeftGlobal.y())),
            static_cast<int>(std::round(it->restingRect.width)),
            static_cast<int>(std::round(it->restingRect.height))
        );

        QMetaObject::invokeMethod(
            m_model,
            "requestPublishDelegateGeometry",
            Qt::DirectConnection,
            Q_ARG(QModelIndex, QModelIndex(entry.modelIndex)),
            Q_ARG(QRect, globalRect),
            Q_ARG(QObject *, entry.item.data())
        );
    }
}

void DockLayoutItem::componentComplete() {
    QQuickItem::componentComplete();
    refreshDevicePixelRatio();
    rebuildAllDelegates();
}

void DockLayoutItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        syncAllDelegateSizesAndOrigins();
        updateStructuralImplicitSize();
        polish();
        Q_EMIT configChanged();
    }
}

void DockLayoutItem::itemChange(ItemChange change, const ItemChangeData &value) {
    QQuickItem::itemChange(change, value);
    if (change == ItemSceneChange) {
        m_blurController->setWindow(value.window);
        refreshDevicePixelRatio();
        polish();
    } else if (change == ItemDevicePixelRatioHasChanged) {
        refreshDevicePixelRatio();
        polish();
    }
}

void DockLayoutItem::updatePolish() {
    const double expectedDelSize = m_engine.effectiveDelegateLogicalSize(width(), height());
    if (std::abs(expectedDelSize - m_lastDelegateLogicalSize) > 0.25) {
        syncAllDelegateSizesAndOrigins();
    }
    const core::DockFrame frame = m_engine.computeFrame(width(), height());
    applyCurrentFrame(frame);
    if (m_engine.isSettled()) {
        publishDelegateGeometries();
    }
}

void DockLayoutItem::onModelReset() {
    m_reconcileTimer.stop();
    m_suppressNextLayoutReorder = false;
    rebuildAllDelegates();
}

void DockLayoutItem::scheduleReconcile() {
    if (!m_reconcileTimer.isActive()) {
        m_reconcileTimer.start();
    }
}

bool DockLayoutItem::hasTaskModelRoles() const {
    return roleIdByName(QByteArrayLiteral("IsLauncher")) >= 0
        || roleIdByName(QByteArrayLiteral("AppId")) >= 0
        || roleIdByName(QByteArrayLiteral("StorageId")) >= 0;
}

DockLayoutItem::TaskIdentity DockLayoutItem::readTaskIdentity(int row) const {
    TaskIdentity id{};
    if (!m_model || row < 0 || row >= m_model->rowCount()) {
        return id;
    }
    id.hasTaskRoles = hasTaskModelRoles();
    if (!id.hasTaskRoles) {
        return id;
    }

    const QModelIndex idx = m_model->index(row, 0);
    auto normalizeAppKey = [](QString s) -> QString {
        s = s.trimmed().toLower();
        if (s.endsWith(QStringLiteral(".desktop"))) {
            s.chop(8);
        }
        return s;
    };

    if (const int storageRole = roleIdByName(QByteArrayLiteral("StorageId")); storageRole >= 0) {
        id.storageOrAppId = normalizeAppKey(m_model->data(idx, storageRole).toString());
    }
    if (id.storageOrAppId.isEmpty()) {
        if (const int appIdRole = roleIdByName(QByteArrayLiteral("AppId")); appIdRole >= 0) {
            id.storageOrAppId = normalizeAppKey(m_model->data(idx, appIdRole).toString());
        }
    }
    if (const int urlRole = roleIdByName(QByteArrayLiteral("LauncherUrlWithoutIcon")); urlRole >= 0) {
        id.launcherUrl = m_model->data(idx, urlRole).toUrl().toString().trimmed().toLower();
    }
    if (const int appNameRole = roleIdByName(QByteArrayLiteral("AppName")); appNameRole >= 0) {
        id.appName = m_model->data(idx, appNameRole).toString().trimmed().toLower();
    }
    if (id.appName.isEmpty()) {
        id.appName = m_model->data(idx, Qt::DisplayRole).toString().trimmed().toLower();
    }
    return id;
}

bool DockLayoutItem::matchesTaskIdentity(const TaskIdentity &a, const TaskIdentity &b) noexcept {
    if (!a.hasTaskRoles || !b.hasTaskRoles) {
        return false;
    }
    if (!a.storageOrAppId.isEmpty() && !b.storageOrAppId.isEmpty()) {
        return a.storageOrAppId == b.storageOrAppId;
    }
    if (!a.launcherUrl.isEmpty() && !b.launcherUrl.isEmpty() && a.launcherUrl == b.launcherUrl) {
        return true;
    }
    if (!a.appName.isEmpty() && !b.appName.isEmpty() && a.appName == b.appName) {
        return true;
    }
    return false;
}

void DockLayoutItem::reconcileModel() {
    m_reconcileTimer.stop();
    if (!m_model || !m_delegate) {
        return;
    }

    struct DesiredRow {
        int modelRow{0};
        QPersistentModelIndex persistentIndex;
        TaskIdentity identity{};
        bool isStartup{false};
        bool isLauncher{false};
        bool forceBounce{false};
    };

    const int rowCount = m_model->rowCount();
    const int startupRole = roleIdByName(QByteArrayLiteral("IsStartup"));
    const int launcherRole = roleIdByName(QByteArrayLiteral("IsLauncher"));

    std::vector<DesiredRow> rawRows;
    rawRows.reserve(static_cast<std::size_t>(std::max(0, rowCount)));
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex idx = m_model->index(row, 0);
        DesiredRow desired{};
        desired.modelRow = row;
        desired.persistentIndex = QPersistentModelIndex(idx);
        desired.identity = readTaskIdentity(row);
        desired.isStartup = (startupRole >= 0) && m_model->data(idx, startupRole).toBool();
        desired.isLauncher = (launcherRole >= 0) && m_model->data(idx, launcherRole).toBool();
        rawRows.push_back(std::move(desired));
    }

    // Suppress duplicate IsStartup rows that mirror an already-present Launcher/Window row
    // (e.g., preferred://filemanager alongside a Dolphin startup notification) and route
    // the bounce directly onto the existing pinned/running icon instead.
    std::vector<DesiredRow> desiredRows;
    desiredRows.reserve(rawRows.size());
    for (const auto &r : rawRows) {
        if (!r.isStartup) {
            desiredRows.push_back(r);
        }
    }
    for (const auto &r : rawRows) {
        if (!r.isStartup) {
            continue;
        }
        bool mergedIntoExisting = false;
        for (auto &existing : desiredRows) {
            if (matchesTaskIdentity(r.identity, existing.identity)) {
                existing.forceBounce = true;
                mergedIntoExisting = true;
                break;
            }
        }
        if (!mergedIntoExisting) {
            desiredRows.push_back(r);
        }
    }
    std::sort(
        desiredRows.begin(),
        desiredRows.end(),
        [](const DesiredRow &a, const DesiredRow &b) { return a.modelRow < b.modelRow; }
    );

    // Three-pass identity matching against existing delegates:
    //   1. Exact QPersistentModelIndex match (active delegates)
    //   2. TaskIdentity match against remaining active delegates (same-tick Launcher<->Startup<->Window swap)
    //   3. TaskIdentity match against animating ghost delegates (cross-tick revival without duplication)
    std::vector<bool> oldUsed(m_delegates.size(), false);
    std::vector<int> matchedOldIndex(desiredRows.size(), -1);

    // Pass 1: exact persistent model index match among active delegates.
    for (std::size_t i = 0; i < desiredRows.size(); ++i) {
        for (std::size_t j = 0; j < m_delegates.size(); ++j) {
            if (oldUsed[j] || m_delegates[j].ghost) {
                continue;
            }
            if (m_delegates[j].modelIndex.isValid()
                && m_delegates[j].modelIndex.model() == m_model
                && m_delegates[j].modelIndex.row() == desiredRows[i].modelRow) {
                oldUsed[j] = true;
                matchedOldIndex[i] = static_cast<int>(j);
                break;
            }
        }
    }

    // Pass 2: TaskIdentity match among remaining active delegates.
    for (std::size_t i = 0; i < desiredRows.size(); ++i) {
        if (matchedOldIndex[i] >= 0 || !desiredRows[i].identity.hasTaskRoles) {
            continue;
        }
        for (std::size_t j = 0; j < m_delegates.size(); ++j) {
            if (oldUsed[j] || m_delegates[j].ghost) {
                continue;
            }
            if (matchesTaskIdentity(m_delegates[j].identity, desiredRows[i].identity)) {
                oldUsed[j] = true;
                matchedOldIndex[i] = static_cast<int>(j);
                break;
            }
        }
    }

    // Pass 3: TaskIdentity match among animating ghost delegates (revive in-place!).
    for (std::size_t i = 0; i < desiredRows.size(); ++i) {
        if (matchedOldIndex[i] >= 0 || !desiredRows[i].identity.hasTaskRoles) {
            continue;
        }
        for (std::size_t j = 0; j < m_delegates.size(); ++j) {
            if (oldUsed[j] || !m_delegates[j].ghost) {
                continue;
            }
            if (matchesTaskIdentity(m_delegates[j].identity, desiredRows[i].identity)) {
                oldUsed[j] = true;
                matchedOldIndex[i] = static_cast<int>(j);
                break;
            }
        }
    }

    // Any unused old active delegates are genuinely closed/unpinned -> animate out as ghosts.
    std::vector<ManagedDelegate> survivingGhosts;
    for (std::size_t j = 0; j < m_delegates.size(); ++j) {
        if (oldUsed[j]) {
            continue;
        }
        ManagedDelegate entry = m_delegates[j];
        if (!entry.ghost) {
            entry.ghost = true;
            m_engine.removeSlotById(entry.slotId, true);
        }
        survivingGhosts.push_back(std::move(entry));
    }

    // Build updated active delegates (reusing matched ones, reviving ghosts, creating new ones).
    std::vector<ManagedDelegate> nextActive;
    nextActive.reserve(desiredRows.size());
    std::vector<std::uint64_t> orderedActiveIds;
    orderedActiveIds.reserve(desiredRows.size());

    for (std::size_t i = 0; i < desiredRows.size(); ++i) {
        const auto &desired = desiredRows[i];
        if (matchedOldIndex[i] >= 0) {
            ManagedDelegate entry = m_delegates[static_cast<std::size_t>(matchedOldIndex[i])];
            if (entry.ghost) {
                entry.ghost = false;
                m_engine.reviveSlotById(entry.slotId, false);
            }
            entry.modelIndex = desired.persistentIndex;
            entry.isStartup = desired.isStartup;
            entry.isLauncher = desired.isLauncher;
            entry.identity = desired.identity;
            orderedActiveIds.push_back(entry.slotId);
            nextActive.push_back(std::move(entry));
        } else {
            const std::uint64_t slotId = m_engine.insertSlot(i, true);
            QQuickItem *item = createDelegateForRow(desired.modelRow, slotId, true);
            ManagedDelegate entry{};
            entry.slotId = slotId;
            entry.modelIndex = desired.persistentIndex;
            entry.item = item;
            entry.ghost = false;
            entry.isStartup = desired.isStartup;
            entry.isLauncher = desired.isLauncher;
            entry.identity = desired.identity;
            orderedActiveIds.push_back(slotId);
            nextActive.push_back(std::move(entry));
        }
    }

    m_engine.reorderActiveSlots(orderedActiveIds, width(), height());

    for (std::size_t i = 0; i < nextActive.size(); ++i) {
        syncDelegateProperties(nextActive[i], desiredRows[i].modelRow);
        const bool bounce = desiredRows[i].forceBounce || isRowBouncing(desiredRows[i].modelRow);
        m_engine.setSlotBouncingById(nextActive[i].slotId, bounce);
    }

    m_delegates.clear();
    m_delegates.reserve(nextActive.size() + survivingGhosts.size());
    for (auto &entry : nextActive) {
        m_delegates.push_back(std::move(entry));
    }
    for (auto &ghost : survivingGhosts) {
        m_delegates.push_back(std::move(ghost));
    }

    updateStructuralImplicitSize();
    updateAttentionState();
    applyCurrentFrame(m_engine.computeFrame(width(), height()));
    Q_EMIT countChanged();
    if (!m_engine.isSettled()) {
        m_driver->ensureRunning();
    }
}

void DockLayoutItem::onRowsInserted(const QModelIndex &parent, int first, int last) {
    Q_UNUSED(first);
    Q_UNUSED(last);
    if (parent.isValid() || !m_model || !m_delegate) {
        return;
    }
    m_suppressNextLayoutReorder = false;
    if (hasTaskModelRoles()) {
        scheduleReconcile();
    } else {
        reconcileModel();
    }
}

void DockLayoutItem::onRowsRemoved(const QModelIndex &parent, int first, int last) {
    Q_UNUSED(first);
    Q_UNUSED(last);
    if (parent.isValid() || !m_model || !m_delegate) {
        return;
    }
    m_suppressNextLayoutReorder = false;
    if (hasTaskModelRoles()) {
        scheduleReconcile();
    } else {
        reconcileModel();
    }
}

void DockLayoutItem::onRowsMoved(
    const QModelIndex &parent,
    int start,
    int end,
    const QModelIndex &destination,
    int row
) {
    if (parent.isValid() || destination.isValid()) {
        return;
    }

    if (m_reconcileTimer.isActive()
        || !m_model
        || static_cast<int>(m_engine.activeSlotCount()) != m_model->rowCount()) {
        if (hasTaskModelRoles()) {
            scheduleReconcile();
        } else {
            reconcileModel();
        }
        return;
    }

    const int moveCount = end - start + 1;
    for (int i = 0; i < moveCount; ++i) {
        const int fromIdx = (row > start) ? start : (start + i);
        const int toIdx = (row > start) ? (row - 1) : (row + i);
        m_engine.moveSlot(static_cast<std::size_t>(fromIdx), static_cast<std::size_t>(toIdx), width(), height());

        // Mirror move in m_delegates among active entries.
        int activeSeen = 0;
        std::size_t fromPos = m_delegates.size();
        for (std::size_t k = 0; k < m_delegates.size(); ++k) {
            if (!m_delegates[k].ghost) {
                if (activeSeen == fromIdx) {
                    fromPos = k;
                    break;
                }
                ++activeSeen;
            }
        }
        if (fromPos < m_delegates.size()) {
            ManagedDelegate moved = m_delegates[fromPos];
            m_delegates.erase(m_delegates.begin() + static_cast<std::ptrdiff_t>(fromPos));
            activeSeen = 0;
            std::size_t toPos = m_delegates.size();
            for (std::size_t k = 0; k < m_delegates.size(); ++k) {
                if (!m_delegates[k].ghost) {
                    if (activeSeen == toIdx) {
                        toPos = k;
                        break;
                    }
                    ++activeSeen;
                }
            }
            m_delegates.insert(m_delegates.begin() + static_cast<std::ptrdiff_t>(toPos), moved);
        }
    }

    // TaskManager::TasksModel::move() calls beginMoveRows/endMoveRows and immediately
    // follows with forceResort() (QSortFilterProxyModel::invalidate -> layoutChanged).
    // That synchronous invalidate() remaps QPersistentModelIndex through stale proxy maps,
    // which would otherwise swap the moved delegates back. Suppress the reorder pass on that
    // immediate follow-up layoutChanged and re-anchor persistent indexes cleanly instead.
    m_suppressNextLayoutReorder = true;
    QMetaObject::invokeMethod(
        this,
        [this]() { m_suppressNextLayoutReorder = false; },
        Qt::QueuedConnection
    );

    int activeIdx = 0;
    for (auto &entry : m_delegates) {
        if (!entry.ghost) {
            if (m_model) {
                entry.modelIndex = QPersistentModelIndex(m_model->index(activeIdx, 0));
            }
            syncDelegateProperties(entry, activeIdx++);
        }
    }

    polish();
    if (!m_engine.isSettled()) {
        m_driver->ensureRunning();
    }
}

void DockLayoutItem::onLayoutChanged(
    const QList<QPersistentModelIndex> &parents,
    QAbstractItemModel::LayoutChangeHint hint
) {
    Q_UNUSED(hint);
    if (!m_model || !m_delegate) {
        return;
    }

    // If specific parents were provided and none is the root index, only a child subtree changed.
    if (!parents.isEmpty()) {
        bool affectsRoot = false;
        for (const QPersistentModelIndex &p : parents) {
            if (!p.isValid()) {
                affectsRoot = true;
                break;
            }
        }
        if (!affectsRoot) {
            return;
        }
    }

    if (m_reconcileTimer.isActive()) {
        return;
    }

    const int rowCount = m_model->rowCount();
    int activeCount = 0;
    for (const auto &entry : m_delegates) {
        if (!entry.ghost) {
            ++activeCount;
        }
    }

    if (activeCount != rowCount) {
        m_suppressNextLayoutReorder = false;
        reconcileModel();
        return;
    }

    if (m_suppressNextLayoutReorder) {
        m_suppressNextLayoutReorder = false;
        int activeIdx = 0;
        for (auto &entry : m_delegates) {
            if (!entry.ghost) {
                entry.modelIndex = QPersistentModelIndex(m_model->index(activeIdx, 0));
                syncDelegateProperties(entry, activeIdx);
                m_engine.setSlotBouncing(static_cast<std::size_t>(activeIdx), isRowBouncing(activeIdx));
                ++activeIdx;
            }
        }
        updateAttentionState();
        polish();
        if (!m_engine.isSettled()) {
            m_driver->ensureRunning();
        }
        return;
    }

    // Verify that existing active delegates' QPersistentModelIndex rows form a valid permutation of 0..rowCount-1.
    std::vector<ManagedDelegate> byNewRow(static_cast<std::size_t>(std::max(0, rowCount)));
    std::vector<bool> seen(static_cast<std::size_t>(std::max(0, rowCount)), false);
    bool validPermutation = true;
    bool orderChanged = false;
    int currentActiveIdx = 0;

    for (const auto &entry : m_delegates) {
        if (entry.ghost) {
            continue;
        }
        if (!entry.modelIndex.isValid() || entry.modelIndex.model() != m_model) {
            validPermutation = false;
            break;
        }
        const int newRow = entry.modelIndex.row();
        if (newRow < 0 || newRow >= rowCount) {
            validPermutation = false;
            break;
        }
        const auto uRow = static_cast<std::size_t>(newRow);
        if (seen[uRow]) {
            validPermutation = false;
            break;
        }
        seen[uRow] = true;
        byNewRow[uRow] = entry;
        if (newRow != currentActiveIdx) {
            orderChanged = true;
        }
        ++currentActiveIdx;
    }

    if (!validPermutation) {
        reconcileModel();
        return;
    }

    if (orderChanged) {
        std::vector<std::uint64_t> orderedActiveIds;
        orderedActiveIds.reserve(byNewRow.size());
        for (const auto &entry : byNewRow) {
            orderedActiveIds.push_back(entry.slotId);
        }

        std::size_t nextActive = 0;
        for (auto &entry : m_delegates) {
            if (!entry.ghost) {
                entry = byNewRow[nextActive++];
            }
        }
        m_engine.reorderActiveSlots(orderedActiveIds, width(), height());
    }

    // Always re-sync delegate properties (including fresh QModelIndex and taskIndex) after layoutChanged.
    int activeIdx = 0;
    for (auto &entry : m_delegates) {
        if (!entry.ghost) {
            syncDelegateProperties(entry, activeIdx);
            m_engine.setSlotBouncing(static_cast<std::size_t>(activeIdx), isRowBouncing(activeIdx));
            ++activeIdx;
        }
    }

    updateAttentionState();
    polish();
    if (!m_engine.isSettled()) {
        m_driver->ensureRunning();
    }
}

void DockLayoutItem::onDataChanged(
    const QModelIndex &topLeft,
    const QModelIndex &bottomRight,
    const QList<int> &roles
) {
    Q_UNUSED(roles);
    if (!m_model || topLeft.parent().isValid() || m_reconcileTimer.isActive()) {
        return;
    }

    const int first = topLeft.row();
    const int last = bottomRight.row();
    for (auto &entry : m_delegates) {
        if (!entry.ghost && entry.modelIndex.isValid()) {
            const int modelRow = entry.modelIndex.row();
            if (modelRow >= first && modelRow <= last) {
                syncDelegateProperties(entry, modelRow);
                m_engine.setSlotBouncingById(entry.slotId, isRowBouncing(modelRow));
            }
        }
    }

    updateAttentionState();
    if (!m_engine.isSettled()) {
        m_driver->ensureRunning();
    }
}

void DockLayoutItem::rebuildAllDelegates() {
    m_reconcileTimer.stop();
    clearAllDelegates();
    m_engine.clearSlots();

    if (!m_model || !m_delegate) {
        updateStructuralImplicitSize();
        updateAttentionState();
        Q_EMIT countChanged();
        polish();
        return;
    }

    const int rows = m_model->rowCount();
    m_delegates.reserve(static_cast<std::size_t>(std::max(0, rows)));
    for (int row = 0; row < rows; ++row) {
        const std::uint64_t slotId = m_engine.insertSlot(static_cast<std::size_t>(row), false);
        QQuickItem *item = createDelegateForRow(row, slotId);

        ManagedDelegate entry{};
        entry.slotId = slotId;
        entry.modelIndex = QPersistentModelIndex(m_model->index(row, 0));
        entry.item = item;
        entry.ghost = false;
        entry.identity = readTaskIdentity(row);
        m_delegates.push_back(entry);

        m_engine.setSlotBouncingById(slotId, isRowBouncing(row));
    }

    updateStructuralImplicitSize();
    updateAttentionState();
    Q_EMIT countChanged();
    polish();
    if (!m_engine.isSettled()) {
        m_driver->ensureRunning();
    }
}

void DockLayoutItem::clearAllDelegates() {
    for (auto &entry : m_delegates) {
        if (entry.item) {
            entry.item->setVisible(false);
            entry.item->deleteLater();
        }
    }
    m_delegates.clear();
}

QQuickItem *DockLayoutItem::createDelegateForRow(int row, std::uint64_t slotId, bool startHidden) {
    Q_UNUSED(slotId);
    if (!m_delegate || !m_model) {
        return nullptr;
    }

    QQmlContext *ctx = qmlContext(this);
    if (!ctx) {
        ctx = m_delegate->creationContext();
    }
    if (!ctx && m_delegate->engine()) {
        ctx = m_delegate->engine()->rootContext();
    }
    if (!ctx) {
        return nullptr;
    }

    QObject *rawObj = m_delegate->beginCreate(ctx);
    auto *item = qobject_cast<QQuickItem *>(rawObj);
    if (!item) {
        delete rawObj;
        return nullptr;
    }

    item->setParentItem(this);
    const double delSize = m_engine.effectiveDelegateLogicalSize(width(), height());
    item->setSize(QSizeF(delSize, delSize));
    item->setTransformOrigin(transformOriginForEdge());
    if (startHidden) {
        item->setScale(0.0);
        item->setOpacity(0.0);
    }

    ManagedDelegate temp{};
    temp.modelIndex = QPersistentModelIndex(m_model->index(row, 0));
    temp.item = item;
    syncDelegateProperties(temp, row);

    m_delegate->completeCreate();
    return item;
}

void DockLayoutItem::syncDelegateProperties(ManagedDelegate &entry, int activeIndex) {
    if (!entry.item || !m_model || !entry.modelIndex.isValid()) {
        return;
    }

    const QModelIndex idx = entry.modelIndex;
    auto readRole = [&](const QByteArray &name, const QVariant &fallback = {}) -> QVariant {
        const int role = roleIdByName(name);
        if (role < 0) {
            return fallback;
        }
        const QVariant v = m_model->data(idx, role);
        return v.isValid() ? v : fallback;
    };

    entry.item->setProperty("taskIndex", activeIndex);
    entry.item->setProperty("modelIndex", QVariant::fromValue(idx));
    entry.item->setProperty("displayTitle", m_model->data(idx, Qt::DisplayRole).toString());
    entry.item->setProperty("decorationIcon", m_model->data(idx, Qt::DecorationRole));
    entry.item->setProperty("isActive", readRole(QByteArrayLiteral("IsActive"), false).toBool());
    const bool launcher = readRole(QByteArrayLiteral("IsLauncher"), false).toBool();
    entry.isLauncher = launcher;
    entry.item->setProperty("isLauncher", launcher);
    entry.item->setProperty("isWindow", readRole(QByteArrayLiteral("IsWindow"), false).toBool());
    const bool startup = readRole(QByteArrayLiteral("IsStartup"), false).toBool();
    entry.isStartup = startup;
    entry.item->setProperty("isStartup", startup);
    entry.item->setProperty("isDemandingAttention", readRole(QByteArrayLiteral("IsDemandingAttention"), false).toBool());
    entry.item->setProperty("isMinimized", readRole(QByteArrayLiteral("IsMinimized"), false).toBool());
    entry.item->setProperty("isMaximized", readRole(QByteArrayLiteral("IsMaximized"), false).toBool());
    entry.item->setProperty("isFullScreen", readRole(QByteArrayLiteral("IsFullScreen"), false).toBool());
    entry.item->setProperty("isKeepAbove", readRole(QByteArrayLiteral("IsKeepAbove"), false).toBool());
    entry.item->setProperty("isKeepBelow", readRole(QByteArrayLiteral("IsKeepBelow"), false).toBool());
    entry.item->setProperty("isClosable", readRole(QByteArrayLiteral("IsClosable"), true).toBool());
    entry.item->setProperty("isMinimizable", readRole(QByteArrayLiteral("IsMinimizable"), true).toBool());
    entry.item->setProperty("isMaximizable", readRole(QByteArrayLiteral("IsMaximizable"), true).toBool());
    entry.item->setProperty("isMovable", readRole(QByteArrayLiteral("IsMovable"), true).toBool());
    entry.item->setProperty("isResizable", readRole(QByteArrayLiteral("IsResizable"), true).toBool());
    entry.item->setProperty("isFullScreenable", readRole(QByteArrayLiteral("IsFullScreenable"), true).toBool());
    entry.item->setProperty("isOnAllVirtualDesktops", readRole(QByteArrayLiteral("IsOnAllVirtualDesktops"), false).toBool());
    entry.item->setProperty("virtualDesktops", readRole(QByteArrayLiteral("VirtualDesktops"), QVariantList()));
    entry.item->setProperty("winIdList", readRole(QByteArrayLiteral("WinIdList"), QVariantList()));
    entry.item->setProperty("isGroupParent", readRole(QByteArrayLiteral("IsGroupParent"), false).toBool());
    entry.item->setProperty("childCount", readRole(QByteArrayLiteral("ChildCount"), 0).toInt());
    const QString appId = readRole(QByteArrayLiteral("AppId"), QString()).toString();
    entry.item->setProperty("appId", appId);
    entry.item->setProperty("appName", readRole(QByteArrayLiteral("AppName"), QString()).toString());
    entry.item->setProperty("appPid", readRole(QByteArrayLiteral("AppPid"), 0).toInt());
    const QUrl launcherUrl = readRole(QByteArrayLiteral("LauncherUrlWithoutIcon"), QUrl()).toUrl();
    entry.item->setProperty("launcherUrl", launcherUrl);
    entry.item->setProperty("launcherUrlFull", readRole(QByteArrayLiteral("LauncherUrl"), QUrl()));
    entry.item->setProperty("dockEdge", static_cast<int>(m_config.axis.edge));
    entry.item->setProperty("delegateLogicalSize", m_engine.effectiveDelegateLogicalSize(width(), height()));
    entry.identity = readTaskIdentity(idx.row());
}

void DockLayoutItem::syncAllDelegateSizesAndOrigins() {
    const double delSize = m_engine.effectiveDelegateLogicalSize(width(), height());
    m_lastDelegateLogicalSize = delSize;
    const QQuickItem::TransformOrigin origin = transformOriginForEdge();
    for (auto &entry : m_delegates) {
        if (entry.item) {
            entry.item->setSize(QSizeF(delSize, delSize));
            entry.item->setTransformOrigin(origin);
            entry.item->setProperty("dockEdge", static_cast<int>(m_config.axis.edge));
            entry.item->setProperty("delegateLogicalSize", delSize);
        }
    }
}

void DockLayoutItem::updateStructuralImplicitSize() {
    // Include animating ghost slots so the Fit-Content panel never shrinks prematurely
    // while a closing icon is still scaling out (shrinking happens once the ghost is pruned).
    const std::size_t n = m_engine.totalSlotCount();
    // Pass 0 for the primary axis so implicit primary size reflects the desired
    // cross-proportional icon size rather than feeding back into primary clamping
    // (which would trap Fit-Content Plasma panels at minimum width on startup).
    const double queryW = m_config.axis.isHorizontal() ? 0.0 : width();
    const double queryH = m_config.axis.isHorizontal() ? height() : 0.0;
    const double effBase = m_engine.effectiveBaseIconSize(queryW, queryH);
    const double effSpacing = m_engine.effectiveIconSpacing(queryW, queryH);
    const double effMax = m_engine.effectiveMaxScaleForContainer(queryW, queryH);
    const double effRadius = std::max(1.0, m_config.influenceRadiusIcons * (effBase + effSpacing));
    const double effSideSlack = core::maxSideSlack(m_config.curve, std::max(1.0, effMax), effRadius);
    const double restingLength = (n > 0)
        ? (static_cast<double>(n) * effBase
           + static_cast<double>(n - 1) * effSpacing)
        : 0.0;
    const double primary = restingLength
        + 2.0 * (m_config.pillPaddingPrimary + effSideSlack);
    const double cross = static_cast<double>(m_config.recommendedPanelThickness());

    if (m_config.axis.isHorizontal()) {
        setImplicitSize(primary, cross);
    } else {
        setImplicitSize(cross, primary);
    }
}

void DockLayoutItem::applyCurrentFrame(const core::DockFrame &frame) {
    for (const auto &slot : frame.items) {
        const auto it = std::find_if(
            m_delegates.begin(),
            m_delegates.end(),
            [id = slot.id](const ManagedDelegate &entry) { return entry.slotId == id; }
        );
        if (it == m_delegates.end() || !it->item) {
            continue;
        }
        QQuickItem *item = it->item;
        // Scale-only + position-only per frame (zero per-frame setSize!).
        item->setPosition(QPointF(slot.delegateTopLeft.x, slot.delegateTopLeft.y));
        item->setScale(slot.delegateScale);
        const double visualOpacity = slot.ghost
            ? std::clamp(slot.weight * slot.weight, 0.0, 1.0)
            : std::clamp(slot.weight * 1.35, 0.0, 1.0);
        item->setOpacity(visualOpacity);
        item->setProperty("slotScale", slot.scale);
        item->setProperty("bounceOffset", slot.bounceOffset);
    }

    const QRectF nextPill = toQRectF(frame.pillRect);
    if (nextPill != m_pillRect) {
        m_pillRect = nextPill;
        Q_EMIT pillRectChanged();
        updateBlurRegion();
    }

    if (!qFuzzyCompare(m_hoverAlpha + 1.0, frame.hoverAlpha + 1.0)) {
        m_hoverAlpha = frame.hoverAlpha;
        Q_EMIT hoverAlphaChanged();
    }
}

void DockLayoutItem::updateAttentionState() {
    bool anyAttention = false;
    if (m_model) {
        const int rows = m_model->rowCount();
        for (int row = 0; row < rows; ++row) {
            if (isRowDemandingAttention(row)) {
                anyAttention = true;
                break;
            }
        }
    }
    if (m_hasAttentionTask != anyAttention) {
        m_hasAttentionTask = anyAttention;
        Q_EMIT hasAttentionTaskChanged();
    }
}

void DockLayoutItem::refreshDevicePixelRatio() {
    const double dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    if (!qFuzzyCompare(m_config.devicePixelRatio, dpr)) {
        m_config.devicePixelRatio = dpr;
        m_engine.setConfig(m_config);
    }
}

void DockLayoutItem::updateBlurRegion() {
    if (!window() || m_pillRect.isEmpty()) {
        m_blurController->setWindowPillRect({});
        return;
    }
    const QPointF topLeftInWindow = mapToScene(m_pillRect.topLeft());
    const QRectF rectInWindow(topLeftInWindow, m_pillRect.size());
    m_blurController->setWindowPillRect(rectInWindow);
}

QQuickItem::TransformOrigin DockLayoutItem::transformOriginForEdge() const noexcept {
    switch (m_config.axis.edge) {
    case core::DockEdge::Bottom:
        return QQuickItem::Bottom;
    case core::DockEdge::Top:
        return QQuickItem::Top;
    case core::DockEdge::Left:
        return QQuickItem::Left;
    case core::DockEdge::Right:
        return QQuickItem::Right;
    }
    return QQuickItem::Bottom;
}

int DockLayoutItem::roleIdByName(const QByteArray &name) const {
    if (!m_model) {
        return -1;
    }
    const auto roles = m_model->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        if (it.value() == name) {
            return it.key();
        }
    }
    return -1;
}

bool DockLayoutItem::isRowBouncing(int row) const {
    if (!m_model || row < 0 || row >= m_model->rowCount()) {
        return false;
    }
    const QModelIndex idx = m_model->index(row, 0);
    const int startupRole = roleIdByName(QByteArrayLiteral("IsStartup"));
    const int attentionRole = roleIdByName(QByteArrayLiteral("IsDemandingAttention"));
    const bool startup = startupRole >= 0 && m_model->data(idx, startupRole).toBool();
    const bool attention = attentionRole >= 0 && m_model->data(idx, attentionRole).toBool();
    return startup || attention;
}

bool DockLayoutItem::isRowDemandingAttention(int row) const {
    if (!m_model || row < 0 || row >= m_model->rowCount()) {
        return false;
    }
    const QModelIndex idx = m_model->index(row, 0);
    const int attentionRole = roleIdByName(QByteArrayLiteral("IsDemandingAttention"));
    return attentionRole >= 0 && m_model->data(idx, attentionRole).toBool();
}

} // namespace fluctus::quick
