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

    m_driver->setTickCallback([this](double dtSeconds) {
        const bool active = m_engine.step(dtSeconds, width(), height());
        const core::DockFrame frame = m_engine.computeFrame(width(), height());
        applyCurrentFrame(frame);

        // Clean up delegates whose ghost slots have finished animating out.
        std::erase_if(m_delegates, [&frame](const ManagedDelegate &entry) {
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

        const bool settled = m_engine.isSettled();
        if (settled) {
            publishDelegateGeometries();
        }
        return active && !settled;
    });
}

DockLayoutItem::~DockLayoutItem() {
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
    m_driver->ensureRunning();
    Q_EMIT configChanged();
}

void DockLayoutItem::setEditModeFrozen(bool frozen) {
    if (m_config.editModeFrozen == frozen) {
        return;
    }
    m_config.editModeFrozen = frozen;
    m_engine.setConfig(m_config);
    m_driver->ensureRunning();
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
            Q_ARG(QQuickItem *, entry.item.data())
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
        polish();
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
    const core::DockFrame frame = m_engine.computeFrame(width(), height());
    applyCurrentFrame(frame);
    if (m_engine.isSettled()) {
        publishDelegateGeometries();
    }
}

void DockLayoutItem::onModelReset() {
    rebuildAllDelegates();
}

void DockLayoutItem::onRowsInserted(const QModelIndex &parent, int first, int last) {
    if (parent.isValid() || !m_model || !m_delegate) {
        return;
    }

    for (int row = first; row <= last; ++row) {
        const std::uint64_t slotId = m_engine.insertSlot(static_cast<std::size_t>(row), true);
        QQuickItem *item = createDelegateForRow(row, slotId);

        ManagedDelegate entry{};
        entry.slotId = slotId;
        entry.modelIndex = QPersistentModelIndex(m_model->index(row, 0));
        entry.item = item;
        entry.ghost = false;

        // Insert into m_delegates at position matching active index `row`.
        int activeSeen = 0;
        std::size_t insertPos = m_delegates.size();
        for (std::size_t i = 0; i < m_delegates.size(); ++i) {
            if (!m_delegates[i].ghost) {
                if (activeSeen == row) {
                    insertPos = i;
                    break;
                }
                ++activeSeen;
            }
        }
        m_delegates.insert(m_delegates.begin() + static_cast<std::ptrdiff_t>(insertPos), entry);
        m_engine.setSlotBouncing(static_cast<std::size_t>(row), isRowBouncing(row));
    }

    // Re-sync taskIndex for all active delegates.
    int activeIdx = 0;
    for (auto &entry : m_delegates) {
        if (!entry.ghost) {
            syncDelegateProperties(entry, activeIdx++);
        }
    }

    updateStructuralImplicitSize();
    updateAttentionState();
    Q_EMIT countChanged();
    m_driver->ensureRunning();
}

void DockLayoutItem::onRowsRemoved(const QModelIndex &parent, int first, int last) {
    if (parent.isValid()) {
        return;
    }

    for (int row = last; row >= first; --row) {
        m_engine.removeSlot(static_cast<std::size_t>(row), true);

        int activeSeen = 0;
        for (auto &entry : m_delegates) {
            if (!entry.ghost) {
                if (activeSeen == row) {
                    entry.ghost = true;
                    break;
                }
                ++activeSeen;
            }
        }
    }

    int activeIdx = 0;
    for (auto &entry : m_delegates) {
        if (!entry.ghost) {
            syncDelegateProperties(entry, activeIdx++);
        }
    }

    updateStructuralImplicitSize();
    updateAttentionState();
    Q_EMIT countChanged();
    m_driver->ensureRunning();
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

    const int moveCount = end - start + 1;
    for (int i = 0; i < moveCount; ++i) {
        const int fromIdx = (row > start) ? start : (start + i);
        const int toIdx = (row > start) ? (row - 1) : (row + i);
        m_engine.moveSlot(static_cast<std::size_t>(fromIdx), static_cast<std::size_t>(toIdx));

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

    int activeIdx = 0;
    for (auto &entry : m_delegates) {
        if (!entry.ghost) {
            syncDelegateProperties(entry, activeIdx++);
        }
    }

    polish();
}

void DockLayoutItem::onDataChanged(
    const QModelIndex &topLeft,
    const QModelIndex &bottomRight,
    const QList<int> &roles
) {
    Q_UNUSED(roles);
    if (!m_model || topLeft.parent().isValid()) {
        return;
    }

    const int first = topLeft.row();
    const int last = bottomRight.row();
    int activeIdx = 0;
    for (auto &entry : m_delegates) {
        if (!entry.ghost) {
            if (activeIdx >= first && activeIdx <= last) {
                syncDelegateProperties(entry, activeIdx);
                m_engine.setSlotBouncing(static_cast<std::size_t>(activeIdx), isRowBouncing(activeIdx));
            }
            ++activeIdx;
        }
    }

    updateAttentionState();
    if (!m_engine.isSettled()) {
        m_driver->ensureRunning();
    }
}

void DockLayoutItem::rebuildAllDelegates() {
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
        m_delegates.push_back(entry);

        m_engine.setSlotBouncing(static_cast<std::size_t>(row), isRowBouncing(row));
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

QQuickItem *DockLayoutItem::createDelegateForRow(int row, std::uint64_t slotId) {
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
    const double delSize = m_config.delegateLogicalSize();
    item->setSize(QSizeF(delSize, delSize));
    item->setTransformOrigin(transformOriginForEdge());

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
    entry.item->setProperty("isLauncher", readRole(QByteArrayLiteral("IsLauncher"), false).toBool());
    entry.item->setProperty("isWindow", readRole(QByteArrayLiteral("IsWindow"), false).toBool());
    entry.item->setProperty("isStartup", readRole(QByteArrayLiteral("IsStartup"), false).toBool());
    entry.item->setProperty("isDemandingAttention", readRole(QByteArrayLiteral("IsDemandingAttention"), false).toBool());
    entry.item->setProperty("isMinimized", readRole(QByteArrayLiteral("IsMinimized"), false).toBool());
    entry.item->setProperty("isGroupParent", readRole(QByteArrayLiteral("IsGroupParent"), false).toBool());
    entry.item->setProperty("childCount", readRole(QByteArrayLiteral("ChildCount"), 0).toInt());
    entry.item->setProperty("appId", readRole(QByteArrayLiteral("AppId"), QString()).toString());
    entry.item->setProperty("launcherUrl", readRole(QByteArrayLiteral("LauncherUrlWithoutIcon"), QUrl()));
    entry.item->setProperty("dockEdge", static_cast<int>(m_config.axis.edge));
    entry.item->setProperty("delegateLogicalSize", m_config.delegateLogicalSize());
}

void DockLayoutItem::syncAllDelegateSizesAndOrigins() {
    const double delSize = m_config.delegateLogicalSize();
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
    const std::size_t n = m_engine.activeSlotCount();
    const double restingLength = (n > 0)
        ? (static_cast<double>(n) * m_config.baseIconSize
           + static_cast<double>(n - 1) * m_config.iconSpacing)
        : 0.0;
    const double primary = restingLength
        + 2.0 * (m_config.pillPaddingPrimary + m_config.sideSlack());
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
        item->setOpacity(slot.weight);
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
