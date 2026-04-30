/**
 * @file test_spot_detection.cpp
 * @brief Unit tests for SpotDetectionWorker
 * 
 * This file contains unit tests to validate the spot detection implementation.
 * Tests cover:
 * 1. Kalman filter prediction
 * 2. ROI extraction
 * 3. Centroid calculation
 * 4. Measurement quality assessment
 * 
 * Note: These tests require Qt Test framework and OpenCV.
 * Compile with: qmake && make
 */

#include "spot_detection_worker.h"
#include <QTest>
#include <QSignalSpy>
#include <opencv2/opencv.hpp>

class TestSpotDetection : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Test individual components
    void testKalmanPrediction();
    void testROIExtraction();
    void testCentroidCalculation();
    void testMeasurementQuality();
    void testDynamicR();
    
    // Integration tests
    void testFullPipeline();
    void testThreadSafety();
    
private:
    cv::Mat createSyntheticSpot(int width, int height, double cx, double cy, double radius, double intensity);
    SpotDetectionWorker *worker_;
};

void TestSpotDetection::initTestCase()
{
    worker_ = new SpotDetectionWorker(this);
    
    // Set default parameters
    SpotDetectionParams params;
    params.roiSize = 12;
    params.thresholdRatio = 0.25;
    params.saturationRatio = 0.95;
    params.useBackgroundRemoval = true;
    params.useSquareWeights = true;
    params.dt = 0.033;
    params.processNoise = 1.0;
    params.baseR = 1.0;
    worker_->setParams(params);
}

void TestSpotDetection::cleanupTestCase()
{
    delete worker_;
}

cv::Mat TestSpotDetection::createSyntheticSpot(int width, int height, 
                                               double cx, double cy, 
                                               double radius, double intensity)
{
    cv::Mat image = cv::Mat::zeros(height, width, CV_16UC1);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double dx = x - cx;
            double dy = y - cy;
            double dist = std::sqrt(dx*dx + dy*dy);
            
            if (dist < radius) {
                // Gaussian-like spot
                double value = intensity * std::exp(-dist*dist / (2*radius*radius));
                image.at<uint16_t>(y, x) = static_cast<uint16_t>(value);
            }
        }
    }
    
    return image;
}

void TestSpotDetection::testKalmanPrediction()
{
    // Reset and initialize Kalman filter at (50, 50)
    worker_->reset();
    
    cv::Mat image = createSyntheticSpot(100, 100, 50, 50, 3.0, 1000.0);
    
    QSignalSpy spy(worker_, &SpotDetectionWorker::detectionFinished);
    
    // Process first frame (initialization)
    worker_->processImage(image, 50.0, 50.0);
    
    // Wait for signal
    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    
    // Get result
    SpotDetectionResult result = spy.at(0).at(0).value<SpotDetectionResult>();
    
    // Verify initial position is close to expected
    QVERIFY(std::abs(result.centerX - 50.0) < 2.0);
    QVERIFY(std::abs(result.centerY - 50.0) < 2.0);
    
    // Process second frame with spot moved to (52, 52)
    spy.clear();
    image = createSyntheticSpot(100, 100, 52, 52, 3.0, 1000.0);
    worker_->processImage(image);
    
    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.count(), 1);
    
    result = spy.at(0).at(0).value<SpotDetectionResult>();
    
    // Verify position tracking
    QVERIFY(std::abs(result.centerX - 52.0) < 2.0);
    QVERIFY(std::abs(result.centerY - 52.0) < 2.0);
    
    // Verify velocity is detected (should be positive)
    QVERIFY(result.velocityX > 0);
    QVERIFY(result.velocityY > 0);
}

void TestSpotDetection::testROIExtraction()
{
    worker_->reset();
    
    // Create spot at (60, 60)
    cv::Mat image = createSyntheticSpot(200, 200, 60, 60, 3.0, 1000.0);
    
    QSignalSpy spy(worker_, &SpotDetectionWorker::detectionFinished);
    
    worker_->processImage(image, 60.0, 60.0);
    QVERIFY(spy.wait(1000));
    
    SpotDetectionResult result = spy.at(0).at(0).value<SpotDetectionResult>();
    
    // Verify ROI is centered around the spot
    int expectedX = 60 - 6;  // roiSize/2 = 12/2 = 6
    int expectedY = 60 - 6;
    
    QVERIFY(std::abs(result.roi.x() - expectedX) <= 1);
    QVERIFY(std::abs(result.roi.y() - expectedY) <= 1);
    QCOMPARE(result.roi.width(), 12);
    QCOMPARE(result.roi.height(), 12);
}

void TestSpotDetection::testCentroidCalculation()
{
    worker_->reset();
    
    // Create a bright spot
    cv::Mat image = createSyntheticSpot(100, 100, 50.5, 50.5, 3.0, 2000.0);
    
    QSignalSpy spy(worker_, &SpotDetectionWorker::detectionFinished);
    
    worker_->processImage(image, 50.0, 50.0);
    QVERIFY(spy.wait(1000));
    
    SpotDetectionResult result = spy.at(0).at(0).value<SpotDetectionResult>();
    
    // Verify sub-pixel accuracy
    QVERIFY(std::abs(result.centerX - 50.5) < 0.5);
    QVERIFY(std::abs(result.centerY - 50.5) < 0.5);
    
    // Verify energy is reasonable
    QVERIFY(result.energy > 100.0);
    
    // Verify valid pixels count
    QVERIFY(result.validPixelCount > 5);
}

void TestSpotDetection::testMeasurementQuality()
{
    worker_->reset();
    
    // Test 1: High quality spot (bright, clear)
    cv::Mat brightSpot = createSyntheticSpot(100, 100, 50, 50, 3.0, 3000.0);
    
    QSignalSpy spy(worker_, &SpotDetectionWorker::detectionFinished);
    
    worker_->processImage(brightSpot, 50.0, 50.0);
    QVERIFY(spy.wait(1000));
    
    SpotDetectionResult brightResult = spy.at(0).at(0).value<SpotDetectionResult>();
    
    // High energy should result in low R
    QVERIFY(brightResult.energy > 1000.0);
    QVERIFY(brightResult.measurementR < 10.0);
    
    // Test 2: Low quality spot (dim, noisy)
    spy.clear();
    worker_->reset();
    
    cv::Mat dimSpot = createSyntheticSpot(100, 100, 50, 50, 3.0, 200.0);
    
    worker_->processImage(dimSpot, 50.0, 50.0);
    QVERIFY(spy.wait(1000));
    
    SpotDetectionResult dimResult = spy.at(0).at(0).value<SpotDetectionResult>();
    
    // Low energy should result in higher R
    QVERIFY(dimResult.energy < 1000.0);
    QVERIFY(dimResult.measurementR > brightResult.measurementR);
}

void TestSpotDetection::testDynamicR()
{
    SpotDetectionParams params = worker_->getParams();
    params.baseR = 1.0;
    params.minEnergy = 100.0;
    params.minValidPixels = 5;
    params.maxVariance = 100.0;
    worker_->setParams(params);
    
    // We can't directly test assessMeasurementQuality as it's private,
    // but we can verify the effect through the results
    
    worker_->reset();
    
    // Create a very dim spot (energy << minEnergy)
    cv::Mat dimSpot = createSyntheticSpot(100, 100, 50, 50, 2.0, 50.0);
    
    QSignalSpy spy(worker_, &SpotDetectionWorker::detectionFinished);
    
    worker_->processImage(dimSpot, 50.0, 50.0);
    QVERIFY(spy.wait(1000));
    
    SpotDetectionResult result = spy.at(0).at(0).value<SpotDetectionResult>();
    
    // Very low energy should significantly increase R
    QVERIFY(result.measurementR > params.baseR);
}

void TestSpotDetection::testFullPipeline()
{
    worker_->reset();
    
    QSignalSpy spy(worker_, &SpotDetectionWorker::detectionFinished);
    
    // Simulate moving spot over several frames
    double x = 50.0, y = 50.0;
    double vx = 1.0, vy = 0.5;  // Constant velocity
    
    // Initialize
    cv::Mat image = createSyntheticSpot(200, 200, x, y, 3.0, 2000.0);
    worker_->processImage(image, x, y);
    QVERIFY(spy.wait(1000));
    
    // Process 10 frames
    for (int i = 0; i < 10; ++i) {
        spy.clear();
        x += vx;
        y += vy;
        
        image = createSyntheticSpot(200, 200, x, y, 3.0, 2000.0);
        worker_->processImage(image);
        
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);
        
        SpotDetectionResult result = spy.at(0).at(0).value<SpotDetectionResult>();
        
        // Verify tracking accuracy
        QVERIFY(std::abs(result.centerX - x) < 2.0);
        QVERIFY(std::abs(result.centerY - y) < 2.0);
        
        // Verify velocity estimation converges
        if (i > 5) {
            QVERIFY(std::abs(result.velocityX - vx) < 0.5);
            QVERIFY(std::abs(result.velocityY - vy) < 0.5);
        }
        
        // Verify processing time is reasonable
        QVERIFY(result.processingTimeUs < 10000);  // < 10ms
    }
}

void TestSpotDetection::testThreadSafety()
{
    // This test verifies that the worker can handle rapid calls
    worker_->reset();
    
    cv::Mat image = createSyntheticSpot(100, 100, 50, 50, 3.0, 2000.0);
    
    QSignalSpy spy(worker_, &SpotDetectionWorker::detectionFinished);
    
    // Send multiple images in quick succession
    for (int i = 0; i < 5; ++i) {
        worker_->processImage(image, 50.0 + i, 50.0 + i);
    }
    
    // Should receive at least one result (may skip some due to processing)
    QVERIFY(spy.wait(2000));
    QVERIFY(spy.count() >= 1);
}

QTEST_MAIN(TestSpotDetection)
#include "test_spot_detection.moc"
