#include "image_algorithm_dock.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QSpinBox>
#include <xlsxdocument.h>
#include <xlsxworksheet.h>
#include <opencv2/opencv.hpp>

void ImageAlgorithmDock::onSaveResults()
{
    QString fileName;
    bool isNewFile = false;

    if (lastSavePath_.isEmpty() || !QFile::exists(lastSavePath_)) {
        fileName = QFileDialog::getSaveFileName(this, tr("保存Excel文件"), "", tr("Excel Files (*.xlsx)"));
        if (fileName.isEmpty()) return;
        lastSavePath_ = fileName;
        isNewFile = true;
    } else {
        fileName = lastSavePath_;
    }

    QXlsx::Document xlsx(isNewFile ? QString() : fileName);

    if (isNewFile) {
        xlsx.addSheet("测试信息");
        QXlsx::Worksheet *infoSheet = xlsx.currentWorksheet();
        infoSheet->write(1, 1, "创建时间");
        infoSheet->write(1, 2, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        infoSheet->write(2, 1, "相机SN");
        infoSheet->write(2, 2, cameraSN_);
        infoSheet->write(3, 1, "相机位数");
        infoSheet->write(3, 2, bitDepth_);
    } else {
        if (!xlsx.sheetNames().contains("测试信息")) {
            xlsx.addSheet("测试信息");
            QXlsx::Worksheet *infoSheet = xlsx.currentWorksheet();
            infoSheet->write(1, 1, "创建时间");
            infoSheet->write(1, 2, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
            infoSheet->write(2, 1, "相机SN");
            infoSheet->write(2, 2, cameraSN_);
            infoSheet->write(3, 1, "相机位数");
            infoSheet->write(3, 2, bitDepth_);
        }
    }

    analysisCount_++;
    QString sheetName = QString("读出噪声_%1").arg(analysisCount_);
    xlsx.addSheet(sheetName);
    QXlsx::Worksheet *noiseSheet = xlsx.currentWorksheet();

    int row = 1;
    int seq = 1;
    for (int gainIdx = 0; gainIdx < gainResults_.size(); ++gainIdx) {
        const NoiseAnalysisResult &result = gainResults_[gainIdx];
        if (!result.valid) continue;

        double avgStd = 0;
        for (double s : result.stdDevs) avgStd += s;
        avgStd /= result.stdDevs.size();

        noiseSheet->write(row, 1, seq++);
        noiseSheet->write(row, 2, "增益类型");
        noiseSheet->write(row, 3, result.gainName);
        row++;

        noiseSheet->write(row, 1, seq++);
        noiseSheet->write(row, 2, "Exp");
        noiseSheet->write(row, 3, result.stdDevs.size());
        row++;

        int startDataRow = row;
        int startDataCol = 3;
        for (int r = 0; r < result.height; ++r) {
            noiseSheet->write(startDataRow + r, 1, seq++);
            for (int c = 0; c < result.width; ++c) {
                double value = result.stdDevs[r * result.width + c];
                noiseSheet->write(startDataRow + r, startDataCol + c, value);
            }
        }
        row += result.height;

        noiseSheet->write(row, 1, seq++);
        noiseSheet->write(row, 2, "STD");
        noiseSheet->write(row, 3, avgStd);
        row++;

        row += 2;
    }

    xlsx.saveAs(fileName);

    QFileInfo fileInfo(fileName);
    QString baseDir = fileInfo.absolutePath();
    saveSampleImages(baseDir, analysisCount_);
}

void ImageAlgorithmDock::onSaveNew()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("保存Excel文件"), "", tr("Excel Files (*.xlsx)"));
    if (fileName.isEmpty()) return;

    QXlsx::Document xlsx;

    xlsx.addSheet("测试信息");
    QXlsx::Worksheet *infoSheet = xlsx.currentWorksheet();
    infoSheet->write(1, 1, "创建时间");
    infoSheet->write(1, 2, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    infoSheet->write(2, 1, "相机SN");
    infoSheet->write(2, 2, cameraSN_);
    infoSheet->write(3, 1, "相机位数");
    infoSheet->write(3, 2, bitDepth_);

    analysisCount_++;
    QString sheetName = QString("读出噪声_%1").arg(analysisCount_);
    xlsx.addSheet(sheetName);
    QXlsx::Worksheet *noiseSheet = xlsx.currentWorksheet();

    int row = 1;
    int seq = 1;
    for (int gainIdx = 0; gainIdx < gainResults_.size(); ++gainIdx) {
        const NoiseAnalysisResult &result = gainResults_[gainIdx];
        if (!result.valid) continue;

        double avgStd = 0;
        for (double s : result.stdDevs) avgStd += s;
        avgStd /= result.stdDevs.size();

        noiseSheet->write(row, 1, seq++);
        noiseSheet->write(row, 2, "增益类型");
        noiseSheet->write(row, 3, result.gainName);
        row++;

        noiseSheet->write(row, 1, seq++);
        noiseSheet->write(row, 2, "Exp");
        noiseSheet->write(row, 3, result.stdDevs.size());
        row++;

        int startDataRow = row;
        int startDataCol = 3;
        for (int r = 0; r < result.height; ++r) {
            noiseSheet->write(startDataRow + r, 1, seq++);
            for (int c = 0; c < result.width; ++c) {
                double value = result.stdDevs[r * result.width + c];
                noiseSheet->write(startDataRow + r, startDataCol + c, value);
            }
        }
        row += result.height;

        noiseSheet->write(row, 1, seq++);
        noiseSheet->write(row, 2, "STD");
        noiseSheet->write(row, 3, avgStd);
        row++;

        row += 2;
    }

    xlsx.saveAs(fileName);

    lastSavePath_ = fileName;

    QFileInfo fileInfo(fileName);
    QString baseDir = fileInfo.absolutePath();
    saveSampleImages(baseDir, analysisCount_);
}

void ImageAlgorithmDock::saveSampleImages(const QString &baseDir, int analysisIndex)
{
    if (gainResults_.isEmpty()) {
        return;
    }

    QString sampleDirPath = baseDir + QString("/sample_%1").arg(analysisIndex);
    QDir sampleDir;
    if (!sampleDir.exists(sampleDirPath)) {
        if (!sampleDir.mkpath(sampleDirPath)) {
            qWarning() << "Failed to create sample directory:" << sampleDirPath;
            return;
        }
    }

    const QString roiRootPath = sampleDirPath + "/ROI";
    const QString fullRootPath = sampleDirPath + "/FULL";

    if (!sampleDir.exists(roiRootPath) && !sampleDir.mkpath(roiRootPath)) {
        qWarning() << "Failed to create ROI directory:" << roiRootPath;
        return;
    }
    if (!sampleDir.exists(fullRootPath) && !sampleDir.mkpath(fullRootPath)) {
        qWarning() << "Failed to create FULL directory:" << fullRootPath;
        return;
    }

    const QStringList formats = {"png", "jpg", "bmp", "tiff", "raw"};

    for (const QString &format : formats) {
        const QString roiFormatDirPath = roiRootPath + "/" + format;
        const QString fullFormatDirPath = fullRootPath + "/" + format;

        if (!sampleDir.exists(roiFormatDirPath) && !sampleDir.mkpath(roiFormatDirPath)) {
            qWarning() << "Failed to create ROI format directory:" << roiFormatDirPath;
        }
        if (!sampleDir.exists(fullFormatDirPath) && !sampleDir.mkpath(fullFormatDirPath)) {
            qWarning() << "Failed to create FULL format directory:" << fullFormatDirPath;
        }
    }

    auto saveMatToFile = [](const cv::Mat &img, const QString &format, const QString &filePath) {
        try {
            if (format == "raw") {
                QFile file(filePath);
                if (file.open(QIODevice::WriteOnly)) {
                    if (img.isContinuous()) {
                        file.write(reinterpret_cast<const char*>(img.data), img.total() * img.elemSize());
                    } else {
                        for (int r = 0; r < img.rows; ++r) {
                            file.write(reinterpret_cast<const char*>(img.ptr(r)), img.cols * img.elemSize());
                        }
                    }
                    file.close();
                } else {
                    qWarning() << "Failed to open file for writing:" << filePath;
                }
                return;
            }

            std::vector<int> params;
            if (format == "png") {
                params.push_back(cv::IMWRITE_PNG_COMPRESSION);
                params.push_back(3);
            } else if (format == "jpg") {
                params.push_back(cv::IMWRITE_JPEG_QUALITY);
                params.push_back(95);
            } else if (format == "tiff") {
                params.push_back(cv::IMWRITE_TIFF_COMPRESSION);
                params.push_back(1);
            }

            std::vector<uchar> buf;
            const bool encodeSuccess = cv::imencode("." + format.toStdString(), img, buf, params);

            if (encodeSuccess) {
                QFile file(filePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(reinterpret_cast<const char*>(buf.data()), buf.size());
                    file.close();
                } else {
                    qWarning() << "Failed to open file for writing:" << filePath;
                }
            } else {
                qWarning() << "Failed to encode image:" << filePath;
            }
        } catch (const cv::Exception& e) {
            qWarning() << "OpenCV error saving image:" << filePath << "-" << e.what();
        }
    };

    for (int gainIdx = 0; gainIdx < gainResults_.size(); ++gainIdx) {
        const NoiseAnalysisResult &result = gainResults_[gainIdx];
        if (!result.valid || result.sampleImages.isEmpty()) {
            continue;
        }

        if (result.sampleFullImages.isEmpty()) {
            qWarning() << "No FULL sample images for gain:" << result.gainName
                       << "- only ROI images will be saved.";
        }

        const int pairCount = std::min(result.sampleImages.size(), result.sampleFullImages.size());
        if (pairCount <= 0) {
            continue;
        }

        for (int imgIdx = 0; imgIdx < pairCount; ++imgIdx) {
            const cv::Mat &roiImg = result.sampleImages[imgIdx];
            const cv::Mat &fullImg = result.sampleFullImages[imgIdx];

            QString gainNameForFile = result.gainName;
            gainNameForFile.replace(" ", "_");
            QString baseName = QString("%1_%2us_sample%3")
                                   .arg(gainNameForFile)
                                   .arg(result.exposureTime)
                                   .arg(imgIdx + 1, 3, 10, QChar('0'));

            for (const QString &format : formats) {
                const QString roiFilePath = QString("%1/%2/%3.%4")
                .arg(roiRootPath)
                    .arg(format)
                    .arg(baseName)
                    .arg(format);

                const QString fullFilePath = QString("%1/%2/%3.%4")
                                                 .arg(fullRootPath)
                                                 .arg(format)
                                                 .arg(baseName)
                                                 .arg(format);

                saveMatToFile(roiImg, format, roiFilePath);
                saveMatToFile(fullImg, format, fullFilePath);
            }
        }
    }
}
