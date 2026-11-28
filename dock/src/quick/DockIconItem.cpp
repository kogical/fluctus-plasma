#include "quick/DockIconItem.hpp"

#include <QIcon>
#include <QPixmap>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace fluctus::quick {

namespace {

[[nodiscard]] bool sameIcon(const QIcon &a, const QIcon &b) {
    if (a.isNull() || b.isNull()) {
        return a.isNull() == b.isNull();
    }
    if (a.cacheKey() == b.cacheKey()) {
        return true;
    }
    return !a.name().isEmpty() && a.name() == b.name();
}

[[nodiscard]] bool sameSourceVariant(const QVariant &a, const QVariant &b) {
    if (a.metaType() == QMetaType::fromType<QIcon>() && b.metaType() == QMetaType::fromType<QIcon>()) {
        return sameIcon(a.value<QIcon>(), b.value<QIcon>());
    }
    if (a.metaType() != b.metaType()) {
        return false;
    }
    return a == b;
}

[[nodiscard]] inline float lanczos3(float x) noexcept {
    constexpr float kA = 3.0f;
    const float ax = std::abs(x);
    if (ax < 1e-6f) {
        return 1.0f;
    }
    if (ax >= kA) {
        return 0.0f;
    }
    const float pix = std::numbers::pi_v<float> * ax;
    const float pixOverA = pix / kA;
    return (std::sin(pix) / pix) * (std::sin(pixOverA) / pixOverA);
}

struct TapTable {
    struct Row {
        int start{0};
        int count{0};
        int weightOffset{0};
    };
    std::vector<Row> rows;
    std::vector<float> weights;
};

[[nodiscard]] TapTable buildLanczos3Table(int srcLen, int dstLen) {
    constexpr float kA = 3.0f;
    TapTable table;
    table.rows.resize(static_cast<std::size_t>(dstLen));

    const float scale = static_cast<float>(srcLen) / static_cast<float>(dstLen);
    const float filterScale = std::max(1.0f, scale);
    const float support = kA * filterScale;
    const int maxTaps = static_cast<int>(std::ceil(support * 2.0f)) + 2;
    table.weights.reserve(static_cast<std::size_t>(dstLen) * static_cast<std::size_t>(maxTaps));

    for (int dst = 0; dst < dstLen; ++dst) {
        const float center = (static_cast<float>(dst) + 0.5f) * scale - 0.5f;
        const int left = std::max(0, static_cast<int>(std::floor(center - support)));
        const int right = std::min(srcLen - 1, static_cast<int>(std::ceil(center + support)));

        const int offset = static_cast<int>(table.weights.size());
        float sum = 0.0f;
        for (int src = left; src <= right; ++src) {
            const float w = lanczos3((static_cast<float>(src) - center) / filterScale);
            table.weights.push_back(w);
            sum += w;
        }
        const int count = right - left + 1;
        if (std::abs(sum) > 1e-6f) {
            const float invSum = 1.0f / sum;
            for (int k = 0; k < count; ++k) {
                table.weights[static_cast<std::size_t>(offset + k)] *= invSum;
            }
        }
        table.rows[static_cast<std::size_t>(dst)] = TapTable::Row{left, count, offset};
    }

    return table;
}

} // namespace

DockIconItem::DockIconItem(QQuickItem *parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void DockIconItem::setSource(const QVariant &source) {
    if (sameSourceVariant(m_source, source)) {
        return;
    }
    m_source = source;
    m_imageDirty = true;
    polish();
    Q_EMIT sourceChanged();
}

void DockIconItem::setActive(bool active) {
    if (m_active == active) {
        return;
    }
    m_active = active;
    m_imageDirty = true;
    polish();
    Q_EMIT activeChanged();
}

void DockIconItem::setExactPixelSampling(bool exact) {
    if (m_exactPixelSampling == exact) {
        return;
    }
    m_exactPixelSampling = exact;
    update();
    Q_EMIT exactPixelSamplingChanged();
}

void DockIconItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        m_imageDirty = true;
        polish();
    }
}

void DockIconItem::itemChange(ItemChange change, const ItemChangeData &value) {
    QQuickItem::itemChange(change, value);
    if (change == ItemSceneChange || change == ItemDevicePixelRatioHasChanged) {
        m_imageDirty = true;
        polish();
    }
}

void DockIconItem::updatePolish() {
    if (!m_imageDirty) {
        return;
    }
    rebuildCachedImage();
    m_imageDirty = false;
    m_textureDirty = true;
    update();
}

QImage DockIconItem::resampleLanczos3(const QImage &src, int targetWidth, int targetHeight) {
    if (src.isNull() || targetWidth <= 0 || targetHeight <= 0) {
        return {};
    }

    const QImage rgba = (src.format() == QImage::Format_RGBA8888_Premultiplied)
        ? src
        : src.convertToFormat(QImage::Format_RGBA8888_Premultiplied);

    const int srcW = rgba.width();
    const int srcH = rgba.height();
    if (srcW <= 0 || srcH <= 0) {
        return {};
    }

    // Fit preserving aspect ratio inside targetWidth x targetHeight.
    const double scaleX = static_cast<double>(targetWidth) / static_cast<double>(srcW);
    const double scaleY = static_cast<double>(targetHeight) / static_cast<double>(srcH);
    const double fitScale = std::min(scaleX, scaleY);
    const int dstW = std::clamp(static_cast<int>(std::round(static_cast<double>(srcW) * fitScale)), 1, targetWidth);
    const int dstH = std::clamp(static_cast<int>(std::round(static_cast<double>(srcH) * fitScale)), 1, targetHeight);

    QImage out(targetWidth, targetHeight, QImage::Format_RGBA8888_Premultiplied);
    out.fill(Qt::transparent);

    const int offsetX = (targetWidth - dstW) / 2;
    const int offsetY = (targetHeight - dstH) / 2;

    if (srcW == dstW && srcH == dstH) {
        for (int y = 0; y < dstH; ++y) {
            const std::uint8_t *srcLine = rgba.constScanLine(y);
            std::uint8_t *dstLine = out.scanLine(offsetY + y) + static_cast<std::ptrdiff_t>(offsetX) * 4;
            std::copy_n(srcLine, static_cast<std::size_t>(dstW) * 4, dstLine);
        }
        return out;
    }

    // Pass 1: Horizontal Lanczos-3 (srcW x srcH -> dstW x srcH in float premultiplied RGBA).
    const TapTable hTable = buildLanczos3Table(srcW, dstW);
    std::vector<float> tmp(static_cast<std::size_t>(srcH) * static_cast<std::size_t>(dstW) * 4, 0.0f);

    for (int y = 0; y < srcH; ++y) {
        const std::uint8_t *srcLine = rgba.constScanLine(y);
        float *tmpRow = tmp.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(dstW) * 4;
        for (int x = 0; x < dstW; ++x) {
            const auto &row = hTable.rows[static_cast<std::size_t>(x)];
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            float a = 0.0f;
            for (int k = 0; k < row.count; ++k) {
                const float w = hTable.weights[static_cast<std::size_t>(row.weightOffset + k)];
                const std::uint8_t *px = srcLine + static_cast<std::ptrdiff_t>(row.start + k) * 4;
                r += w * static_cast<float>(px[0]);
                g += w * static_cast<float>(px[1]);
                b += w * static_cast<float>(px[2]);
                a += w * static_cast<float>(px[3]);
            }
            float *dstPx = tmpRow + static_cast<std::size_t>(x) * 4;
            dstPx[0] = r;
            dstPx[1] = g;
            dstPx[2] = b;
            dstPx[3] = a;
        }
    }

    // Pass 2: Vertical Lanczos-3 (dstW x srcH -> dstW x dstH).
    const TapTable vTable = buildLanczos3Table(srcH, dstH);
    for (int y = 0; y < dstH; ++y) {
        const auto &row = vTable.rows[static_cast<std::size_t>(y)];
        std::uint8_t *outLine = out.scanLine(offsetY + y) + static_cast<std::ptrdiff_t>(offsetX) * 4;
        for (int x = 0; x < dstW; ++x) {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            float a = 0.0f;
            for (int k = 0; k < row.count; ++k) {
                const float w = vTable.weights[static_cast<std::size_t>(row.weightOffset + k)];
                const float *px = tmp.data()
                    + (static_cast<std::size_t>(row.start + k) * static_cast<std::size_t>(dstW)
                       + static_cast<std::size_t>(x))
                        * 4;
                r += w * px[0];
                g += w * px[1];
                b += w * px[2];
                a += w * px[3];
            }
            // Enforce strict premultiplied alpha bounds: 0 <= a <= 255 and 0 <= rgb <= a.
            const float ca = std::clamp(a, 0.0f, 255.0f);
            const auto outA = static_cast<std::uint8_t>(std::lround(ca));
            const auto outR = static_cast<std::uint8_t>(std::lround(std::clamp(r, 0.0f, ca)));
            const auto outG = static_cast<std::uint8_t>(std::lround(std::clamp(g, 0.0f, ca)));
            const auto outB = static_cast<std::uint8_t>(std::lround(std::clamp(b, 0.0f, ca)));
            std::uint8_t *dstPx = outLine + static_cast<std::ptrdiff_t>(x) * 4;
            dstPx[0] = outR;
            dstPx[1] = outG;
            dstPx[2] = outB;
            dstPx[3] = outA;
        }
    }

    return out;
}

QImage DockIconItem::loadHighResSource(int targetPhysicalPx) const {
    const QIcon::Mode mode = m_active ? QIcon::Active : QIcon::Normal;
    // Always request high-resolution artwork (at least 128 px, up to 256 px) so QIcon
    // never hands back a small pre-downscaled 22 px bitmap when higher-res art or SVG exists.
    const int requestPx = std::clamp(std::max(128, targetPhysicalPx * 4), 128, 256);

    auto fromQIcon = [&](const QIcon &icon) -> QImage {
        if (icon.isNull()) {
            return {};
        }
        const QPixmap pm = icon.pixmap(QSize(requestPx, requestPx), 1.0, mode);
        return pm.isNull() ? QImage{} : pm.toImage();
    };

    if (m_source.metaType() == QMetaType::fromType<QIcon>()) {
        QImage img = fromQIcon(m_source.value<QIcon>());
        if (!img.isNull()) {
            return img;
        }
    } else if (m_source.metaType() == QMetaType::fromType<QString>()) {
        const QString name = m_source.toString();
        if (!name.isEmpty()) {
            QImage img = fromQIcon(QIcon::fromTheme(name));
            if (!img.isNull()) {
                return img;
            }
            QImage fileImg(name);
            if (!fileImg.isNull()) {
                return fileImg;
            }
        }
    } else if (m_source.metaType() == QMetaType::fromType<QPixmap>()) {
        const QPixmap pm = m_source.value<QPixmap>();
        if (!pm.isNull()) {
            return pm.toImage();
        }
    } else if (m_source.metaType() == QMetaType::fromType<QImage>()) {
        const QImage img = m_source.value<QImage>();
        if (!img.isNull()) {
            return img;
        }
    }

    return fromQIcon(QIcon::fromTheme(QStringLiteral("application-x-executable")));
}

void DockIconItem::rebuildCachedImage() {
    const double dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const int physW = std::max(0, static_cast<int>(std::lround(width() * dpr)));
    const int physH = std::max(0, static_cast<int>(std::lround(height() * dpr)));

    if (physW <= 0 || physH <= 0) {
        m_cachedImage = {};
        m_lastPhysicalW = 0;
        m_lastPhysicalH = 0;
        return;
    }

    const QImage src = loadHighResSource(std::max(physW, physH));
    if (src.isNull()) {
        m_cachedImage = {};
        m_lastPhysicalW = physW;
        m_lastPhysicalH = physH;
        return;
    }

    m_cachedImage = resampleLanczos3(src, physW, physH);
    m_lastPhysicalW = physW;
    m_lastPhysicalH = physH;
}

QSGNode *DockIconItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) {
    Q_UNUSED(data);

    if (!window() || m_cachedImage.isNull() || width() <= 0.0 || height() <= 0.0) {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
        m_textureDirty = true;
    }

    if (m_textureDirty || !node->texture()) {
        QSGTexture *tex = window()->createTextureFromImage(
            m_cachedImage,
            QQuickWindow::TextureHasAlphaChannel
        );
        node->setTexture(tex);
        m_textureDirty = false;
    }

    node->setFiltering(m_exactPixelSampling ? QSGTexture::Nearest : QSGTexture::Linear);
    node->setRect(QRectF(0.0, 0.0, width(), height()));
    return node;
}

} // namespace fluctus::quick
