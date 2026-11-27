#pragma once

#include "core/DockPhysicsEngine.hpp"
#include "quick/BlurRegionController.hpp"
#include "quick/DockAnimationDriver.hpp"

#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickItem>
#include <QRectF>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <cstdint>
#include <vector>

namespace fluctus::quick {

/// C++-owned macOS dock layout & animation container.
/// Manages delegate lifecycle directly from `model` + `delegate` (no Repeater),
/// animates strictly on the GUI thread via DockAnimationDriver, and drives
/// delegates using setPosition() + setScale() only (fixed logical size).
class DockLayoutItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QAbstractItemModel *model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)

    Q_PROPERTY(int edge READ edge WRITE setEdge NOTIFY configChanged)
    Q_PROPERTY(bool reversePrimary READ reversePrimary WRITE setReversePrimary NOTIFY configChanged)
    Q_PROPERTY(qreal baseIconSize READ baseIconSize WRITE setBaseIconSize NOTIFY configChanged)
    Q_PROPERTY(qreal iconSpacing READ iconSpacing WRITE setIconSpacing NOTIFY configChanged)
    Q_PROPERTY(qreal maxScale READ maxScale WRITE setMaxScale NOTIFY configChanged)
    Q_PROPERTY(qreal influenceRadiusIcons READ influenceRadiusIcons WRITE setInfluenceRadiusIcons NOTIFY configChanged)
    Q_PROPERTY(int magnificationCurve READ magnificationCurve WRITE setMagnificationCurve NOTIFY configChanged)
    Q_PROPERTY(bool bounceEnabled READ bounceEnabled WRITE setBounceEnabled NOTIFY configChanged)
    Q_PROPERTY(qreal bounceRatio READ bounceRatio WRITE setBounceRatio NOTIFY configChanged)
    Q_PROPERTY(bool magnificationEnabled READ magnificationEnabled WRITE setMagnificationEnabled NOTIFY configChanged)
    Q_PROPERTY(bool editModeFrozen READ editModeFrozen WRITE setEditModeFrozen NOTIFY configChanged)
    Q_PROPERTY(bool autoScaleToPanel READ autoScaleToPanel WRITE setAutoScaleToPanel NOTIFY configChanged)
    Q_PROPERTY(qreal panelIconRatio READ panelIconRatio WRITE setPanelIconRatio NOTIFY configChanged)
    Q_PROPERTY(qreal maxZoomPanelRatio READ maxZoomPanelRatio WRITE setMaxZoomPanelRatio NOTIFY configChanged)
    Q_PROPERTY(qreal iconSpacingRatio READ iconSpacingRatio WRITE setIconSpacingRatio NOTIFY configChanged)
    Q_PROPERTY(bool clampToContainer READ clampToContainer WRITE setClampToContainer NOTIFY configChanged)
    Q_PROPERTY(bool blurEnabled READ blurEnabled WRITE setBlurEnabled NOTIFY blurConfigChanged)
    Q_PROPERTY(int blurStyle READ blurStyle WRITE setBlurStyle NOTIFY blurConfigChanged)
    Q_PROPERTY(qreal pillCornerRadius READ pillCornerRadius WRITE setPillCornerRadius NOTIFY blurConfigChanged)
    Q_PROPERTY(qreal edgePadding READ edgePadding WRITE setEdgePadding NOTIFY configChanged)
    Q_PROPERTY(qreal pillPaddingPrimary READ pillPaddingPrimary WRITE setPillPaddingPrimary NOTIFY configChanged)
    Q_PROPERTY(qreal pillPaddingCross READ pillPaddingCross WRITE setPillPaddingCross NOTIFY configChanged)
    Q_PROPERTY(qreal topMargin READ topMargin WRITE setTopMargin NOTIFY configChanged)

    Q_PROPERTY(QRectF pillRect READ pillRect NOTIFY pillRectChanged)
    Q_PROPERTY(qreal hoverAlpha READ hoverAlpha NOTIFY hoverAlphaChanged)
    Q_PROPERTY(int recommendedPanelThickness READ recommendedPanelThickness NOTIFY configChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(qreal delegateLogicalSize READ delegateLogicalSize NOTIFY configChanged)
    Q_PROPERTY(qreal effectiveBaseIconSize READ effectiveBaseIconSize NOTIFY configChanged)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio NOTIFY configChanged)
    Q_PROPERTY(bool hasAttentionTask READ hasAttentionTask NOTIFY hasAttentionTaskChanged)

public:
    explicit DockLayoutItem(QQuickItem *parent = nullptr);
    ~DockLayoutItem() override;

    [[nodiscard]] QAbstractItemModel *model() const noexcept { return m_model; }
    void setModel(QAbstractItemModel *model);

    [[nodiscard]] QQmlComponent *delegate() const noexcept { return m_delegate; }
    void setDelegate(QQmlComponent *delegate);

    [[nodiscard]] int edge() const noexcept { return static_cast<int>(m_config.axis.edge); }
    void setEdge(int edge);

    [[nodiscard]] bool reversePrimary() const noexcept { return m_config.axis.reversePrimary; }
    void setReversePrimary(bool reverse);

    [[nodiscard]] qreal baseIconSize() const noexcept { return m_config.baseIconSize; }
    void setBaseIconSize(qreal size);

    [[nodiscard]] qreal iconSpacing() const noexcept { return m_config.iconSpacing; }
    void setIconSpacing(qreal spacing);

    [[nodiscard]] qreal maxScale() const noexcept { return m_config.maxScale; }
    void setMaxScale(qreal scale);

    [[nodiscard]] qreal influenceRadiusIcons() const noexcept { return m_config.influenceRadiusIcons; }
    void setInfluenceRadiusIcons(qreal radiusIcons);

    [[nodiscard]] int magnificationCurve() const noexcept { return static_cast<int>(m_config.curve); }
    void setMagnificationCurve(int curve);

    [[nodiscard]] bool bounceEnabled() const noexcept { return m_config.bounceEnabled; }
    void setBounceEnabled(bool enabled);

    [[nodiscard]] qreal bounceRatio() const noexcept { return m_config.bounceRatio; }
    void setBounceRatio(qreal ratio);

    [[nodiscard]] bool magnificationEnabled() const noexcept { return m_config.magnificationEnabled; }
    void setMagnificationEnabled(bool enabled);

    [[nodiscard]] bool editModeFrozen() const noexcept { return m_config.editModeFrozen; }
    void setEditModeFrozen(bool frozen);

    [[nodiscard]] bool autoScaleToPanel() const noexcept { return m_config.autoScaleToPanel; }
    void setAutoScaleToPanel(bool enabled);

    [[nodiscard]] qreal panelIconRatio() const noexcept { return m_config.panelIconRatio; }
    void setPanelIconRatio(qreal ratio);

    [[nodiscard]] qreal maxZoomPanelRatio() const noexcept { return m_config.maxZoomPanelRatio; }
    void setMaxZoomPanelRatio(qreal ratio);

    [[nodiscard]] qreal iconSpacingRatio() const noexcept { return m_config.iconSpacingRatio; }
    void setIconSpacingRatio(qreal ratio);

    [[nodiscard]] bool clampToContainer() const noexcept { return m_config.clampToContainer; }
    void setClampToContainer(bool enabled);

    [[nodiscard]] bool blurEnabled() const noexcept { return m_blurEnabled; }
    void setBlurEnabled(bool enabled);

    [[nodiscard]] int blurStyle() const noexcept { return m_blurStyle; }
    void setBlurStyle(int style);

    [[nodiscard]] qreal pillCornerRadius() const noexcept { return m_pillCornerRadius; }
    void setPillCornerRadius(qreal radius);

    [[nodiscard]] qreal edgePadding() const noexcept { return m_config.edgePadding; }
    void setEdgePadding(qreal padding);

    [[nodiscard]] qreal pillPaddingPrimary() const noexcept { return m_config.pillPaddingPrimary; }
    void setPillPaddingPrimary(qreal padding);

    [[nodiscard]] qreal pillPaddingCross() const noexcept { return m_config.pillPaddingCross; }
    void setPillPaddingCross(qreal padding);

    [[nodiscard]] qreal topMargin() const noexcept { return m_config.topMargin; }
    void setTopMargin(qreal margin);

    [[nodiscard]] QRectF pillRect() const noexcept { return m_pillRect; }
    [[nodiscard]] qreal hoverAlpha() const noexcept { return m_hoverAlpha; }
    [[nodiscard]] int recommendedPanelThickness() const noexcept { return m_config.recommendedPanelThickness(); }
    [[nodiscard]] int count() const noexcept { return static_cast<int>(m_engine.activeSlotCount()); }
    [[nodiscard]] qreal delegateLogicalSize() const noexcept {
        return m_engine.effectiveDelegateLogicalSize(width(), height());
    }
    [[nodiscard]] qreal devicePixelRatio() const noexcept { return m_config.devicePixelRatio; }
    [[nodiscard]] qreal effectiveBaseIconSize() const noexcept {
        return m_engine.effectiveBaseIconSize(width(), height());
    }
    [[nodiscard]] bool hasAttentionTask() const noexcept { return m_hasAttentionTask; }

    Q_INVOKABLE void updatePointer(qreal x, qreal y);
    Q_INVOKABLE void clearPointer();
    Q_INVOKABLE int restingInsertionIndex(qreal x, qreal y) const;
    Q_INVOKABLE int restingDragTargetIndex(qreal x, qreal y, int currentIndex) const;
    Q_INVOKABLE QRectF insertionIndicatorRect(int insertionIndex) const;
    Q_INVOKABLE QQuickItem *itemAtIndex(int activeIndex) const;
    Q_INVOKABLE void reapplyBlur();
    Q_INVOKABLE void publishDelegateGeometries();

Q_SIGNALS:
    void modelChanged();
    void delegateChanged();
    void configChanged();
    void blurConfigChanged();
    void pillRectChanged();
    void hoverAlphaChanged();
    void countChanged();
    void hasAttentionTaskChanged();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void updatePolish() override;

private Q_SLOTS:
    void onModelReset();
    void onRowsInserted(const QModelIndex &parent, int first, int last);
    void onRowsRemoved(const QModelIndex &parent, int first, int last);
    void onRowsMoved(
        const QModelIndex &parent,
        int start,
        int end,
        const QModelIndex &destination,
        int row
    );
    void onLayoutChanged(
        const QList<QPersistentModelIndex> &parents,
        QAbstractItemModel::LayoutChangeHint hint
    );
    void onDataChanged(
        const QModelIndex &topLeft,
        const QModelIndex &bottomRight,
        const QList<int> &roles
    );

private:
    struct TaskIdentity {
        bool hasTaskRoles{false};
        QString storageOrAppId;
        QString launcherUrl;
        QString appName;
    };

    struct ManagedDelegate {
        std::uint64_t slotId{0};
        QPersistentModelIndex modelIndex;
        QPointer<QQuickItem> item;
        bool ghost{false};
        bool isStartup{false};
        bool isLauncher{false};
        TaskIdentity identity{};
    };

    void scheduleReconcile();
    void reconcileModel();
    void rebuildAllDelegates();
    void clearAllDelegates();
    QQuickItem *createDelegateForRow(int row, std::uint64_t slotId, bool startHidden = false);
    void syncDelegateProperties(ManagedDelegate &entry, int activeIndex);
    void syncAllDelegateSizesAndOrigins();
    void updateStructuralImplicitSize();
    void applyCurrentFrame(const core::DockFrame &frame);
    void updateAttentionState();
    void refreshDevicePixelRatio();
    void updateBlurRegion();
    [[nodiscard]] bool hasTaskModelRoles() const;
    [[nodiscard]] TaskIdentity readTaskIdentity(int row) const;
    [[nodiscard]] static bool matchesTaskIdentity(const TaskIdentity &a, const TaskIdentity &b) noexcept;
    [[nodiscard]] QQuickItem::TransformOrigin transformOriginForEdge() const noexcept;
    [[nodiscard]] int roleIdByName(const QByteArray &name) const;
    [[nodiscard]] bool isRowBouncing(int row) const;
    [[nodiscard]] bool isRowDemandingAttention(int row) const;

    core::DockConfig m_config{};
    core::DockPhysicsEngine m_engine{};
    DockAnimationDriver *m_driver{nullptr};
    BlurRegionController *m_blurController{nullptr};
    QTimer m_reconcileTimer;

    QPointer<QAbstractItemModel> m_model;
    QPointer<QQmlComponent> m_delegate;
    std::vector<ManagedDelegate> m_delegates;

    QRectF m_pillRect;
    qreal m_hoverAlpha{0.0};
    bool m_blurEnabled{true};
    int m_blurStyle{0};
    qreal m_pillCornerRadius{16.0};
    bool m_hasAttentionTask{false};
    bool m_suppressNextLayoutReorder{false};
    double m_lastDelegateLogicalSize{0.0};
};

} // namespace fluctus::quick
