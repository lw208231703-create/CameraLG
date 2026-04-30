#ifndef OPENCV_QT_BRIDGE_H
#define OPENCV_QT_BRIDGE_H

#include "build_config.h"

#include <QImage>
#include <QVector>
#include <QtGlobal>
#include <cstdint>
#include <limits>
#include <opencv2/core.hpp>

#if OPENCVQTBRIDGE_ENABLE_RAW16_ALIGNMENT_QDEBUG
#include <QDebug>
#include <QString>
#endif

// 说明：
// - View/Wrap 函数返回的 cv::Mat / QImage 都是“视图”，生命周期依赖源数据。
// - Copy 函数会做深拷贝，适合跨线程/异步保存/缓存。

namespace OpenCvQtBridge {

// Forward declarations (helpers are defined later in this header)
inline cv::Mat raw16VectorToMatView(const QVector<uint16_t>& data, int width, int height);

enum class Raw16Alignment {
    LsbAligned,
    MsbAlignedShifted,
    Unknown
};

struct Raw16AlignmentStats {
    Raw16Alignment alignment = Raw16Alignment::Unknown;
    int bitDepthHint = 0;
    int effectiveBitDepth = 0; // guessed or equals bitDepthHint when provided
    int assumedShift = 0; // 16 - effectiveBitDepth when effectiveBitDepth in (8,16)

    int width = 0;
    int height = 0;
    int type = 0;

    uint16_t minVal = 0;
    uint16_t maxVal = 0;

    int sampled = 0;
    double lsbZeroRatio = 0.0; // ratio of samples whose lowest assumedShift bits are all zero
};

inline Raw16AlignmentStats analyzeRaw16Alignment16U(const cv::Mat& mat16u1, int bitDepthHint = 0, int maxSamples = 20000) {
    Raw16AlignmentStats s;
    s.type = mat16u1.type();
    s.width = mat16u1.cols;
    s.height = mat16u1.rows;
    s.bitDepthHint = bitDepthHint;

    if (mat16u1.empty() || mat16u1.type() != CV_16UC1) {
        return s;
    }

    const int total = mat16u1.rows * mat16u1.cols;
    const int targetSamples = qBound(1, maxSamples, qMax(1, total));
    const int step = qMax(1, total / targetSamples);

    uint16_t minV = std::numeric_limits<uint16_t>::max();
    uint16_t maxV = 0;
    int sampled = 0;

    // Pre-compute how often the lowest k bits are all zero (k=1..8)
    int zeroCountByBits[9] = {0};

    // Sample by flatten index to support non-contiguous mats.
    int flatIndex = 0;
    for (int y = 0; y < mat16u1.rows; ++y) {
        const uint16_t* row = mat16u1.ptr<uint16_t>(y);
        for (int x = 0; x < mat16u1.cols; ++x, ++flatIndex) {
            if ((flatIndex % step) != 0) continue;

            const uint16_t v = row[x];
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;

            for (int k = 1; k <= 8; ++k) {
                const uint16_t mask = static_cast<uint16_t>((1u << k) - 1u);
                if ((v & mask) == 0) {
                    ++zeroCountByBits[k];
                }
            }

            ++sampled;
            if (sampled >= targetSamples) break;
        }
        if (sampled >= targetSamples) break;
    }

    s.sampled = sampled;
    s.minVal = (sampled > 0) ? minV : 0;
    s.maxVal = (sampled > 0) ? maxV : 0;

    auto decideForBitDepth = [&](int effBits) -> Raw16Alignment {
        if (effBits <= 8 || effBits >= 16 || sampled <= 0) return Raw16Alignment::Unknown;
        const int shift = 16 - effBits;
        const uint16_t maxNative = static_cast<uint16_t>((1u << effBits) - 1u);
        const uint16_t maxShifted = static_cast<uint16_t>(maxNative << shift);
        const double ratio = (shift >= 1 && shift <= 8) ? (static_cast<double>(zeroCountByBits[shift]) / sampled) : 0.0;

        if (s.maxVal <= maxNative) {
            return Raw16Alignment::LsbAligned;
        }
        if (s.maxVal <= maxShifted && ratio > 0.90) {
            return Raw16Alignment::MsbAlignedShifted;
        }
        return Raw16Alignment::Unknown;
    };

    // 1) If caller gives a useful hint (9..15), use it.
    // 2) Otherwise (hint=0 or 16), try common sensor depths and pick the first match.
    int effectiveBits = (bitDepthHint > 8 && bitDepthHint < 16) ? bitDepthHint : 0;
    Raw16Alignment align = Raw16Alignment::Unknown;
    if (effectiveBits) {
        align = decideForBitDepth(effectiveBits);
    } else {
        const int candidates[] = {14, 12, 10};
        for (int b : candidates) {
            Raw16Alignment a = decideForBitDepth(b);
            if (a == Raw16Alignment::MsbAlignedShifted) {
                effectiveBits = b;
                align = a;
                break;
            }
        }
        if (align == Raw16Alignment::Unknown) {
            for (int b : candidates) {
                Raw16Alignment a = decideForBitDepth(b);
                if (a == Raw16Alignment::LsbAligned) {
                    effectiveBits = b;
                    align = a;
                    break;
                }
            }
        }
    }

    s.effectiveBitDepth = effectiveBits;
    s.alignment = align;
    if (s.effectiveBitDepth > 8 && s.effectiveBitDepth < 16) {
        s.assumedShift = 16 - s.effectiveBitDepth;
        s.lsbZeroRatio = (s.assumedShift >= 1 && s.assumedShift <= 8 && sampled > 0)
                             ? (static_cast<double>(zeroCountByBits[s.assumedShift]) / sampled)
                             : 0.0;
    } else {
        s.assumedShift = 0;
        s.lsbZeroRatio = 0.0;
    }

    return s;
}

// Normalize raw16 to a consistent representation for algorithms:
// - If input is MSB-aligned(left-shifted) (common for 10/12/14-bit in 16-bit container), shift right to native.
// - Output bit depth becomes effectiveBitDepth when it can be inferred.
// - If already LSB-aligned, returns original raw data (no copy).
inline const QVector<uint16_t>& normalizeRaw16VectorToNative(
    const QVector<uint16_t>& rawData,
    int width,
    int height,
    int bitDepthHint,
    QVector<uint16_t>& outNormalized,
    int& outBitDepth,
    Raw16AlignmentStats* outStats = nullptr)
{
    outBitDepth = bitDepthHint;
    outNormalized.clear();

    if (rawData.isEmpty() || width <= 0 || height <= 0 || bitDepthHint <= 8) {
        if (outStats) *outStats = Raw16AlignmentStats();
        return rawData;
    }

    const cv::Mat mat16 = raw16VectorToMatView(rawData, width, height);
    const Raw16AlignmentStats stats = analyzeRaw16Alignment16U(mat16, bitDepthHint);
    if (outStats) *outStats = stats;

    if (stats.effectiveBitDepth > 0) {
        outBitDepth = stats.effectiveBitDepth;
    }

    if (stats.alignment == Raw16Alignment::MsbAlignedShifted && stats.assumedShift > 0) {
        outNormalized = rawData;
        const int shift = stats.assumedShift;
        for (int i = 0; i < outNormalized.size(); ++i) {
            outNormalized[i] = static_cast<uint16_t>(outNormalized[i] >> shift);
        }
        return outNormalized;
    }

    return rawData;
}

#if OPENCVQTBRIDGE_ENABLE_RAW16_ALIGNMENT_QDEBUG
inline void qDebugRaw16Alignment16U(const cv::Mat&, int = 0, const char* = nullptr) {}
inline void qDebugRaw16AlignmentStats(const Raw16AlignmentStats&, const char* = nullptr) {}
#else
inline void qDebugRaw16Alignment16U(const cv::Mat&, int = 0, const char* = nullptr) {}
inline void qDebugRaw16AlignmentStats(const Raw16AlignmentStats&, const char* = nullptr) {}
#endif

inline bool isGray8(const QImage& img) {
    return img.format() == QImage::Format_Grayscale8 || img.format() == QImage::Format_Indexed8;
}

inline cv::Mat qimageGray8ToMatView(const QImage& gray8) {
    // 要求输入是灰度8位；若不是，请先在调用点 convertToFormat。
    return cv::Mat(gray8.height(), gray8.width(), CV_8UC1,
                   const_cast<uchar*>(gray8.bits()), gray8.bytesPerLine());
}

inline QImage mat8ToQImageCopy(const cv::Mat& mat8) {
    if (mat8.empty()) return QImage();
    CV_Assert(mat8.type() == CV_8UC1);

    QImage img(mat8.cols, mat8.rows, QImage::Format_Grayscale8);
    if (img.isNull()) return QImage();

    const int bytesPerRow = mat8.cols; // CV_8UC1
    for (int y = 0; y < mat8.rows; ++y) {
        memcpy(img.scanLine(y), mat8.ptr<uchar>(y), bytesPerRow);
    }
    return img;
}

inline cv::Mat raw16ToMatView(const uint16_t* data, int width, int height, int pitchBytes = 0) {
    if (!data || width <= 0 || height <= 0) return cv::Mat();
    const int step = (pitchBytes > 0) ? pitchBytes : (width * static_cast<int>(sizeof(uint16_t)));
    return cv::Mat(height, width, CV_16UC1, const_cast<uint16_t*>(data), step);
}

inline cv::Mat raw16VectorToMatView(const QVector<uint16_t>& data, int width, int height) {
    if (data.isEmpty() || width <= 0 || height <= 0) return cv::Mat();
    if (data.size() < width * height) return cv::Mat();
    return cv::Mat(height, width, CV_16UC1, const_cast<uint16_t*>(data.constData()));
}

} // namespace OpenCvQtBridge

#endif // OPENCV_QT_BRIDGE_H
