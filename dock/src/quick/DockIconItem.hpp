#pragma once

#include <QImage>
#include <QQuickItem>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

namespace fluctus::quick {

/// High-fidelity icon item for compact & magnified dock slots.
/// Fetches high-resolution artwork from QIcon/theme/pixmap sources and downsamples
/// with a 2-pass separable Lanczos-3 filter in premultiplied RGBA space into an
/// exact physical-pixel texture (no GPU box-filter mipmap smear or Qt box-filter softening).
class DockIconItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariant source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool exactPixelSampling READ exactPixelSampling WRITE setExactPixelSampling NOTIFY exactPixelSamplingChanged)

public:
    explicit DockIconItem(QQuickItem *parent = nullptr);

    [[nodiscard]] QVariant source() const { return m_source; }
    void setSource(const QVariant &source);

    [[nodiscard]] bool active() const noexcept { return m_active; }
    void setActive(bool active);

    [[nodiscard]] bool exactPixelSampling() const noexcept { return m_exactPixelSampling; }
    void setExactPixelSampling(bool exact);

    /// Pure 2-pass separable Lanczos-3 resampler in premultiplied RGBA space (exposed for unit tests).
    [[nodiscard]] static QImage resampleLanczos3(const QImage &src, int targetWidth, int targetHeight);

Q_SIGNALS:
    void sourceChanged();
    void activeChanged();
    void exactPixelSamplingChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void updatePolish() override;
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private:
    void rebuildCachedImage();
    [[nodiscard]] QImage loadHighResSource(int targetPhysicalPx) const;

    QVariant m_source;
    bool m_active{false};
    bool m_exactPixelSampling{false};

    QImage m_cachedImage;
    bool m_imageDirty{true};
    bool m_textureDirty{true};
    int m_lastPhysicalW{0};
    int m_lastPhysicalH{0};
};

} // namespace fluctus::quick
