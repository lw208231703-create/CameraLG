#include "image_acquisition.h"

#ifdef ENABLE_SAPERA_CAMERA

#include "image_depth_converter.h"
#include <opencv2/opencv.hpp>
#include <cstring>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include "capieventinfo.h"
#include <QtCore/QThread>
#include <QImageWriter>

// 常量定义
const QString Image_Acquisition::NoServerFoundMessage = QStringLiteral("No Acquisition Server Found");
const QString Image_Acquisition::NoConfigFoundMessage = QStringLiteral("No Configuration Files Found");

void XferCallBack(SapXferCallbackInfo *pInfo)
{
    Image_Acquisition *myCamera = static_cast<Image_Acquisition *>(pInfo->GetContext());
    if (myCamera) {
        myCamera->processNewImage();
    }
}

Image_Acquisition::Image_Acquisition(QObject *parent)
    : QObject(parent)
    , m_BufferCount(10) // 增加默认缓冲区数量以防止丢帧
    , m_CurrentBufferIndex(0)
    , m_FrameCount(0)
{
    // 不在构造函数中初始化相机，由外部调用init_camera()
}

Image_Acquisition::~Image_Acquisition()
{
    free();
}

QStringList Image_Acquisition::getAvailableServers()
{
    QStringList servers;
    
    int serverCount = SapManager::GetServerCount(SapManager::ResourceAcq);
    
    for (int i = 0; i < serverCount; ++i) {
        char serverName[CORSERVER_MAX_STRLEN];
        if (SapManager::GetServerName(i, SapManager::ResourceAcq, serverName)) {
            servers.append(QString::fromLatin1(serverName));
        }
    }
    
    if (servers.isEmpty()) {
        servers.append(NoServerFoundMessage);
    }
    
    return servers;
}

QStringList Image_Acquisition::getConfigurationFiles(const QString &directory)
{
    QStringList configFiles;
    QDir dir(directory);
    
    if (dir.exists()) {
        // 查找.ccf配置文件
        QStringList filters;
        filters << "*.ccf" << "*.CCF";
        dir.setNameFilters(filters);
        
        QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::Readable);
        for (const QFileInfo &fileInfo : fileList) {
            configFiles.append(fileInfo.absoluteFilePath());
        }
    }
    
    if (configFiles.isEmpty()) {
        configFiles.append(NoConfigFoundMessage);
    }
    
    return configFiles;
}

void Image_Acquisition::setSelectedServer(const QString &serverName)
{
    m_SelectedServer = serverName;
}

void Image_Acquisition::setConfigPath(const QString &ccfPath)
{
    m_ConfigPath = ccfPath;
}

void Image_Acquisition::setBufferCount(int count)
{
    if (count < 1 || count > 100) return;  // 允许 1-100 个缓冲区
    
    // 如果数量没变，直接返回
    if (m_BufferCount == count) return;

    bool wasInitialized = isInitialized();
    bool wasGrabbing = false;

    //1. 如果Sapera设备已初始化，先清理旧资源
    if (wasInitialized) {
        if (m_Xfer && m_Xfer->IsGrabbing()) {
            wasGrabbing = true;
            stopGrab();
        }
        
        // 销毁传输对象
        if (m_Xfer) {
            m_Xfer->Destroy();
            delete m_Xfer;
            m_Xfer = nullptr;
        }
        
        // 销毁缓冲区对象
        if (m_Buffers) {
            m_Buffers->Destroy();
            delete m_Buffers;
            m_Buffers = nullptr;
        }
    }

    //2. 更新计数（同时支持GigE和Sapera）
    {
        QMutexLocker locker(&m_CacheMutex);
        m_BufferCount = count;
        
        // 调整软件缓冲区大小
        m_SoftBuffers.resize(m_BufferCount);
        // 清除软件缓冲区内容
        for(auto& frame : m_SoftBuffers) {
            frame.isEmpty = true;
            frame.rawData.clear();
            frame.rawData8.clear();
        }
        // 重置索引
        if (m_ImageSourceType == ImageSourceType::GigE) {
            m_CurrentBufferIndex = 0;
        }
    }

    //3. 如果之前已初始化Sapera，则按新数量重建底层资源
    if (wasInitialized && m_Acquisition) {
        // 使用新的数量创建 SapBuffer
        m_Buffers = new SapBufferWithTrash(m_BufferCount, m_Acquisition);
        // 重新创建传输对象
        m_Xfer = new SapAcqToBuf(m_Acquisition, m_Buffers, XferCallBack, this);

        // 创建底层资源
        if (m_Buffers && !*m_Buffers) {
            if (!m_Buffers->Create()) {
                emit acquisitionError(tr("Failed to recreate buffers"));
                return;
            }
            m_Buffers->Clear();
        }

        // 设置循环模式
        if (m_Xfer && m_Xfer->GetPair(0)) {
            m_Xfer->GetPair(0)->SetCycleMode(SapXferPair::CycleNextWithTrash);
        }
        
        if (m_Xfer && !*m_Xfer) {
            if (!m_Xfer->Create()) {
                emit acquisitionError(tr("Failed to recreate transfer object"));
                return;
            }
        }

        // 如果之前正在采集，恢复采集
        if (wasGrabbing) {
            grabContinues();
        }
    }
}

void Image_Acquisition::setBitShift(int shift)
{
    if (shift < 0) shift = 0;
    if (shift > 8) shift = 8;
    m_BitShift = shift;
}

int Image_Acquisition::getBufferCount() const
{
    return m_BufferCount;
}

int Image_Acquisition::getCurrentBufferIndex() const
{
    return m_CurrentBufferIndex;
}

bool Image_Acquisition::init_camera()
{
    // 使用设置的服务器和配置文件路径
    if (m_SelectedServer.isEmpty() || m_SelectedServer == NoServerFoundMessage) {
        // 如果没有选择服务器，尝试获取第一个可用的
        QStringList servers = getAvailableServers();
        if (servers.isEmpty() || servers.first() == NoServerFoundMessage) {
            emit acquisitionError(tr("No acquisition server available"));
            return false;
        }
        m_SelectedServer = servers.first();
    }
    
    if (m_ConfigPath.isEmpty() || m_ConfigPath == NoConfigFoundMessage) {
        emit acquisitionError(tr("No configuration file selected"));
        return false;
    }
    
    // 使用QByteArray管理内存，避免手动内存管理
    m_ServerNameBuffer = m_SelectedServer.toLatin1();
    m_ServerName = m_ServerNameBuffer.data();
    
    if (initDevice(m_SelectedServer, m_ConfigPath)) {
        // 小延时以确保底层资源稳定
        QThread::msleep(150);
        return true;
    } else {
        emit acquisitionError(tr("Failed to open device: %1").arg(m_SelectedServer));
        free();
        return false;
    }
}

bool Image_Acquisition::initDevice(const QString &serverName, const QString &ccfPath)
{
    QByteArray serverNameBytes = serverName.toLatin1();
    QByteArray ccfPathBytes = ccfPath.toLatin1();
    
    SapLocation loc(serverNameBytes.constData(), 0);
    
    int resourceCount = SapManager::GetResourceCount(serverNameBytes.constData(), SapManager::ResourceAcq);
    
    if (resourceCount > 0) {
        m_Acquisition = new SapAcquisition(loc, ccfPathBytes.constData());
        // 使用配置的缓冲区数量
        m_Buffers = new SapBufferWithTrash(m_BufferCount, m_Acquisition);
        m_Xfer = new SapAcqToBuf(m_Acquisition, m_Buffers, XferCallBack, this);
    } else {
        emit acquisitionError(tr("No acquisition resources available"));
        return false;
    }
    
    // 创建底层资源
    if (m_Acquisition && !*m_Acquisition && !m_Acquisition->Create()) {
        emit acquisitionError(tr("Failed to create acquisition object"));
        return false;
    }
    
    if (m_Buffers && !*m_Buffers) {
        if (!m_Buffers->Create()) {
            emit acquisitionError(tr("Failed to create buffer object"));
            return false;
        } else {
            m_Buffers->Clear();
        }
    }
    
    if (m_Xfer && m_Xfer->GetPair(0)) {
        if (!m_Xfer->GetPair(0)->SetCycleMode(SapXferPair::CycleNextWithTrash)) {
            emit acquisitionError(tr("Failed to set cycle mode"));
            return false;
        }
    }
    
    if (m_Xfer && !*m_Xfer && !m_Xfer->Create()) {
        emit acquisitionError(tr("Failed to create transfer object"));
        return false;
    }
    
    return true;
}

bool Image_Acquisition::grabContinues()
{
    if (!m_Xfer) {
        emit acquisitionError(tr("Transfer object not initialized"));
        return false;
    }
    
    m_FrameCount = 0;
    bool isGrabbing = m_Xfer->IsGrabbing();
    
    if (!isGrabbing) {
        if (m_Xfer->Grab()) {
            emit acquisitionStarted();
            return true;
        } else {
            emit acquisitionError(tr("Failed to start continuous grab"));
            return false;
        }
    }
    
    return true;
}

bool Image_Acquisition::grabOnce()
{
    if (!m_Xfer) {
        emit acquisitionError(tr("Transfer object not initialized"));
        return false;
    }
    
    m_FrameCount = 0;
    
    if (m_Xfer->IsGrabbing()) {
        m_Xfer->Freeze();
        if (!m_Xfer->Wait(1000)) {
            m_Xfer->Abort();
        }
        return false;
    }
    
    if (m_Xfer->Snap(1)) {
        bool waitOk = m_Xfer->Wait(1000);
        if (waitOk) {
            // emit acquisitionStarted();
            return true;
        }
    }
    emit acquisitionError(tr("Failed to capture single frame"));
    return false;
}

bool Image_Acquisition::stopGrab()
{
    if (!m_Xfer) {
        return false;
    }
    
    bool isGrabbing = m_Xfer->IsGrabbing();
    
    if (isGrabbing) {
        if (m_Xfer->Freeze()) {
            if (!m_Xfer->Wait(5000)) {
                m_Xfer->Abort();
            }
            emit acquisitionStopped();
            return true;
        }
    }
    
    return !isGrabbing;
}

void Image_Acquisition::processNewImage()
{
    if (!m_Buffers) {
        return;
    }
    
    // Protect shared state change
    {
        QMutexLocker locker(&m_CacheMutex);
        // Ensure source type is set to Sapera when callback is triggered
        if (m_ImageSourceType != ImageSourceType::Sapera) {
             m_ImageSourceType = ImageSourceType::Sapera;
        }
    }
    
    // 获取当前缓冲区索引
    int bufferIndex = m_Buffers->GetIndex();
    m_CurrentBufferIndex = bufferIndex % m_BufferCount;
    
    // 发送图像就绪信号（仅发送索引，UI线程根据需要直接从 SapBuffer 读取）
    emit imageReady(m_CurrentBufferIndex);
    
    m_FrameCount++;
}

void Image_Acquisition::injectGigeImage(const uchar* pData, int width, int height, int bitDepth)
{
    // Optimization 3: Reduce lock granularity
    int currentIndex;
    {
        QMutexLocker locker(&m_CacheMutex);
        
        // Ensure buffer size is correct (rarely happens if setBufferCount is called properly)
        if (m_SoftBuffers.size() != m_BufferCount) {
            m_SoftBuffers.resize(m_BufferCount);
        }

        // Calculate next index
        currentIndex = (m_CurrentBufferIndex + 1) % m_BufferCount;
    }

    SoftBufferFrame& frame = m_SoftBuffers[currentIndex];
    int pixelCount = width * height;

    // Optimization 4 (Partial): Pre-allocate/resize with lock
    {
        QMutexLocker locker(&m_CacheMutex);
        if (bitDepth <= 8) {
            if (frame.rawData8.size() != pixelCount) {
                frame.rawData8.resize(pixelCount);
            }
        } else {
            if (frame.rawData.size() != pixelCount) {
                frame.rawData.resize(pixelCount);
            }
        }
    }

    // Optimization 2: Store 8-bit directly & Copy data without lock
    if (pData) {
        if (bitDepth <= 8) {
            // 8-bit image: Store directly as 8-bit
            memcpy(frame.rawData8.data(), pData, pixelCount);
        } else {
             // >8-bit: Store as 16-bit
             memcpy(frame.rawData.data(), pData, pixelCount * sizeof(uint16_t));
        }
    }

    // Update metadata and global state with lock
    {
        QMutexLocker locker(&m_CacheMutex);
        frame.width = width;
        frame.height = height;
        frame.bitDepth = bitDepth;
        frame.isEmpty = false;

        m_CurrentBufferIndex = currentIndex;
        m_ImageSourceType = ImageSourceType::GigE;
    }

    // Emit signal without lock
    emit imageReady(m_CurrentBufferIndex);
}

QImage Image_Acquisition::getBufferImage(int bufferIndex)
{
    // 使用互斥锁保护 SapBuffer 访问，防止与重建缓冲区冲突
    QMutexLocker locker(&m_CacheMutex);
    
    // 如果是GigE相机图像源，从软件缓冲区读取
    if (m_ImageSourceType == ImageSourceType::GigE) {
        if (bufferIndex < 0 || bufferIndex >= m_SoftBuffers.size()) {
            return QImage();
        }
        
        const SoftBufferFrame& frame = m_SoftBuffers[bufferIndex];
        if (frame.isEmpty) {
            return QImage();
        }

        // 8位图直接返回 (Optimization 2)
        if (frame.bitDepth <= 8) {
             if (frame.rawData8.isEmpty()) return QImage(); // Safety check
             
             QImage img(frame.width, frame.height, QImage::Format_Grayscale8);
             if (img.isNull()) return QImage();
             
             // Directly copy line by line to handle padding/stride if needed
             // QImage lines are 32-bit aligned.
             const uint8_t* srcPtr = frame.rawData8.constData();
             for (int y = 0; y < frame.height; ++y) {
                 memcpy(img.scanLine(y), srcPtr + y * frame.width, frame.width);
             }
             return img;
        } 
        
        // 高位深图需进行位提取
        if (frame.bitDepth > 8) {
            if (frame.rawData.isEmpty()) return QImage();
            return ImageDepthConverter::bitExtract(frame.rawData.constData(), 
                                                 frame.width, 
                                                 frame.height, 
                                                 frame.width * sizeof(uint16_t),  // pitch 必须是字节数
                                                 frame.bitDepth, 
                                                 m_BitShift);
        }
        return QImage();
    }
    
    if (!m_Buffers) {
        return QImage();
    }
    
    // 直接从底层 SapBuffer 读取数据转换为 QImage
    // 这只在 UI 需要显示时调用，不会影响采集线程的性能
    
    // 设置缓冲区索引
    m_Buffers->SetIndex(bufferIndex);
    
    // 获取缓冲区信息
    int width = m_Buffers->GetWidth();
    int height = m_Buffers->GetHeight();
    int pitch = m_Buffers->GetPitch();
    int pixelDepth = m_Buffers->GetPixelDepth();
    
    // 获取缓冲区数据指针
    void *pData = nullptr;
    if (!m_Buffers->GetAddress(&pData) || pData == nullptr) {
        return QImage();
    }
    
    // 创建结果图像（深拷贝，因为 SapBuffer 的数据可能会变）
    QImage resultImage;
    
    if (pixelDepth == 8) {
        // 8位灰度图像 - 无需转换
        QImage tempImage(static_cast<const uchar*>(pData), width, height, pitch, QImage::Format_Grayscale8);
        resultImage = tempImage.copy();
    } else if (pixelDepth > 8 && pixelDepth <= 16) {
        // 高位深图像 (10/12/14/16-bit) 转 8-bit 显示
        // 使用 ImageDepthConverter 进行转换，使用位段提取法
        const uint16_t *srcData = static_cast<const uint16_t*>(pData);
        resultImage = ImageDepthConverter::bitExtract(srcData, width, height, pitch, pixelDepth, m_BitShift);
    } else if (pixelDepth == 24 || pixelDepth == 32) {
        // RGB image
        QImage::Format qFormat = (pixelDepth == 24) ? QImage::Format_RGB888 : QImage::Format_ARGB32;
        QImage tempImage(static_cast<const uchar*>(pData), width, height, pitch, qFormat);
        resultImage = tempImage.copy();
    }
    
    // 释放缓冲区地址
    m_Buffers->ReleaseAddress(pData);
    
    return resultImage;
}

QVector<uint16_t> Image_Acquisition::getBufferRawData(int bufferIndex, int &width, int &height, int &bitDepth)
{
    QMutexLocker locker(&m_CacheMutex);
    
    // 如果是GigE相机图像源，从软件缓冲区读取
    if (m_ImageSourceType == ImageSourceType::GigE) {
        if (bufferIndex >= 0 && bufferIndex < m_SoftBuffers.size()) {
            const SoftBufferFrame& frame = m_SoftBuffers[bufferIndex];
            if (!frame.isEmpty) {
                width = frame.width;
                height = frame.height;
                bitDepth = frame.bitDepth;
                
                if (bitDepth <= 8) {
                    // Convert 8-bit storage to 16-bit vector for compatibility
                    // This is only used for analysis, so performance cost is acceptable here
                    QVector<uint16_t> convertedData(frame.rawData8.size());
                    const uint8_t* src = frame.rawData8.constData();
                    uint16_t* dst = convertedData.data();
                    int size = frame.rawData8.size();
                    // Simple loop (compiler will likely vectorize) or use std::copy
                    for(int i=0; i<size; ++i) {
                        dst[i] = static_cast<uint16_t>(src[i]);
                    }
                    return convertedData;
                } else {
                    return frame.rawData;
                }
            }
        }
        width = 0; height = 0; bitDepth = 0;
        return QVector<uint16_t>();
    }
    
    if (!m_Buffers) {
        width = 0;
        height = 0;
        bitDepth = 0;
        return QVector<uint16_t>();
    }
    
    m_Buffers->SetIndex(bufferIndex);
    width = m_Buffers->GetWidth();
    height = m_Buffers->GetHeight();
    int pixelDepth = m_Buffers->GetPixelDepth();
    bitDepth = pixelDepth;
    
    QVector<uint16_t> rawData;
    rawData.resize(width * height);
    
    if (pixelDepth > 8 && pixelDepth <= 16) {
        // 16-bit data
        // 优先使用 ReadRect 进行优化拷贝 (自动处理 Pitch)
        if (m_Buffers->ReadRect(0, 0, width, height, rawData.data())) {
            return rawData;
        }

        // 如果 ReadRect 失败，回退到手动拷贝
        void *pData = nullptr;
        if (m_Buffers->GetAddress(&pData) && pData) {
            int pitch = m_Buffers->GetPitch();
            const char* byteData = static_cast<const char*>(pData);
            for (int y = 0; y < height; ++y) {
                const uint16_t* srcRow = reinterpret_cast<const uint16_t*>(byteData + y * pitch);
                std::copy(srcRow, srcRow + width, rawData.begin() + y * width);
            }
            m_Buffers->ReleaseAddress(pData);
        }
    } else if (pixelDepth == 8) {
        // 8-bit data converted to 16-bit container
        void *pData = nullptr;
        if (m_Buffers->GetAddress(&pData) && pData) {
            int pitch = m_Buffers->GetPitch();
            
            // 使用 OpenCV 加速 8-bit -> 16-bit 转换
            cv::Mat src(height, width, CV_8UC1, pData, pitch);
            cv::Mat dst(height, width, CV_16UC1, rawData.data());
            src.convertTo(dst, CV_16UC1);

            m_Buffers->ReleaseAddress(pData);
        }
    }
    
    return rawData;
}

// 旧的 convertBufferToQImage 已被 getBufferImage 替代或不再需要
QImage Image_Acquisition::convertBufferToQImage(int bufferIndex)
{
    return getBufferImage(bufferIndex);
}


bool Image_Acquisition::saveImageToFile(int bufferIndex, const QString &filePath)
{
    QString extension = QFileInfo(filePath).suffix().toLower();
    QString formatUpper = extension.toUpper();
    
    // GigE 模式：使用软件缓冲区保存
    if (m_ImageSourceType == ImageSourceType::GigE) {
        QMutexLocker locker(&m_CacheMutex);
        
        if (bufferIndex < 0 || bufferIndex >= m_SoftBuffers.size()) {
            qWarning() << "saveImageToFile: Invalid buffer index for GigE";
            return false;
        }
        
        const SoftBufferFrame& frame = m_SoftBuffers[bufferIndex];
        if (frame.isEmpty) {
            qWarning() << "saveImageToFile: Buffer is empty";
            return false;
        }
        
        // 检查Qt是否支持该格式
        QList<QByteArray> supportedFormats = QImageWriter::supportedImageFormats();
        bool qtSupportsFormat = false;
        for (const QByteArray &fmt : supportedFormats) {
            if (QString::fromLatin1(fmt).toLower() == extension) {
                qtSupportsFormat = true;
                break;
            }
        }
        
        if (!qtSupportsFormat && extension != "raw") {
            qWarning() << "Format not supported by Qt:" << extension;
            return false;
        }
        
        // RAW 格式特殊处理
        if (extension == "raw") {
            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                qWarning() << "saveImageToFile: Cannot open file for writing";
                return false;
            }
            
            QDataStream stream(&file);
            stream.setByteOrder(QDataStream::LittleEndian);
            stream << static_cast<qint32>(frame.width);
            stream << static_cast<qint32>(frame.height);
            
            if (frame.bitDepth <= 8) {
                stream.writeRawData(reinterpret_cast<const char*>(frame.rawData8.constData()),
                                   frame.rawData8.size());
            } else {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                stream.writeRawData(reinterpret_cast<const char*>(frame.rawData.constData()),
                                   frame.rawData.size() * sizeof(uint16_t));
#else
                for (const uint16_t &pixel : frame.rawData) {
                    stream << pixel;
                }
#endif
            }
            file.close();
            return true;
        }
        
        // 其他格式：使用 QImage 保存
        QImage image;
        if (frame.bitDepth <= 8) {
            image = QImage(frame.width, frame.height, QImage::Format_Grayscale8);
            for (int y = 0; y < frame.height; ++y) {
                memcpy(image.scanLine(y), 
                       frame.rawData8.constData() + y * frame.width, 
                       frame.width);
            }
        } else {
            // 高位深：尝试保存为16位或转换为8位
            QImage image16(frame.width, frame.height, QImage::Format_Grayscale16);
            for (int y = 0; y < frame.height; ++y) {
                uint16_t *line = reinterpret_cast<uint16_t*>(image16.scanLine(y));
                const uint16_t *src = frame.rawData.constData() + y * frame.width;
                memcpy(line, src, frame.width * sizeof(uint16_t));
            }
            
            // 尝试16位保存
            if (image16.save(filePath, formatUpper.toLatin1().constData())) {
                return true;
            }
            
            // 回退到8位转换
            image = ImageDepthConverter::bitExtract(frame.rawData.constData(),
                                                    frame.width, frame.height,
                                                    frame.width * sizeof(uint16_t),
                                                    frame.bitDepth, m_BitShift);
        }
        
        if (!image.isNull()) {
            return image.save(filePath, formatUpper.toLatin1().constData());
        }
        
        return false;
    }
    
    // Sapera 模式
    if (!m_Buffers) {
        qWarning() << "saveImageToFile: m_Buffers is null";
        return false;
    }
    
    // 1. 优先尝试使用 Sapera SDK 保存 (支持 BMP, TIFF, JPEG, RAW, CRC, AVI)
    QString sapOptions;
    bool sapSupported = false;
    
    if (extension == "bmp") {
        sapOptions = "-format bmp";
        sapSupported = true;
    } else if (extension == "tif" || extension == "tiff") {
        sapOptions = "-format tiff";
        sapSupported = true;
    } else if (extension == "jpg" || extension == "jpeg") {
        sapOptions = "-format jpeg";
        sapSupported = true;
    } else if (extension == "raw") {
        sapOptions = "-format raw";
        sapSupported = true;
    } else if (extension == "crc") {
        sapOptions = "-format crc";
        sapSupported = true;
    } else if (extension == "avi") {
        sapOptions = "-format avi";
        sapSupported = true;
    }
    
    if (sapSupported) {
        m_Buffers->SetIndex(bufferIndex);
        QByteArray pathBytes = filePath.toLatin1();
        QByteArray formatBytes = sapOptions.toLatin1();
        
        // SapBuffer::Save 能更好地处理高位深数据
        if (m_Buffers->Save(pathBytes.constData(), formatBytes.constData())) {
            return true;
        }
        qWarning() << "SapBuffer::Save failed for:" << filePath << ", trying Qt fallback...";
    }

    // 2. 如果 Sapera 不支持或保存失败，回退到 Qt 实现
    
    // 检查Qt是否支持该格式
    QList<QByteArray> supportedFormats = QImageWriter::supportedImageFormats();
    bool qtSupportsFormat = false;
    for (const QByteArray &fmt : supportedFormats) {
        if (QString::fromLatin1(fmt).toLower() == extension) {
            qtSupportsFormat = true;
            break;
        }
    }
    
    if (!qtSupportsFormat) {
         // 如果既不是 Sapera 支持的格式，也不是 Qt 支持的格式
         qWarning() << "Format not supported by Qt or Sapera:" << extension;
         return false;
    }
    
    // 尝试获取原始数据并保存为16位图像（如果支持且位深大于8）
    int w = 0, h = 0, bitDepth = 0;
    QVector<uint16_t> rawData = getBufferRawData(bufferIndex, w, h, bitDepth);
    
    if (!rawData.isEmpty() && bitDepth > 8) {
         QImage image(w, h, QImage::Format_Grayscale16);
         for (int y = 0; y < h; ++y) {
             uint16_t *line = reinterpret_cast<uint16_t*>(image.scanLine(y));
             const uint16_t *src = rawData.constData() + y * w;
             memcpy(line, src, w * sizeof(uint16_t));
         }
         bool success = image.save(filePath, formatUpper.toLatin1().constData());
         if (success) return true;
         
         qWarning() << "Failed to save 16-bit image, falling back to 8-bit conversion";
    }

    // 直接从底层缓冲区获取图像并保存 (这将获取8位显示图像)
    QImage image = getBufferImage(bufferIndex);
    
    if (!image.isNull()) {
        bool success = image.save(filePath, formatUpper.toLatin1().constData());
        if (!success) {
            qWarning() << "QImage::save failed for:" << filePath << "format:" << formatUpper;
        }
        return success;
    }
    
    return false;
}

void Image_Acquisition::clearBuffers()
{
    QMutexLocker locker(&m_CacheMutex);
    // m_CachedImages.clear(); 
    // m_CachedImages.resize(m_BufferCount); 
    
    if (m_Buffers) {
        m_Buffers->Clear();
    }
}

bool Image_Acquisition::isGrabbing() const
{
    if (m_ImageSourceType == ImageSourceType::GigE) {
        return !m_SoftBuffers.isEmpty();
    }
    if (m_Xfer) {
        return m_Xfer->IsGrabbing();
    }
    return false;
}

bool Image_Acquisition::setImageGeometry(int width, int height, int x, int y)
{
    if (!m_Acquisition) {
        return false;
    }

    bool wasGrabbing = isGrabbing();
    if (wasGrabbing) {
        stopGrab();
    }

    // 销毁传输和缓冲区对象，因为图像大小改变需要重新分配内存
    if (m_Xfer) {
        m_Xfer->Destroy();
        delete m_Xfer;
        m_Xfer = nullptr;
    }

    if (m_Buffers) {
        m_Buffers->Destroy();
        delete m_Buffers;
        m_Buffers = nullptr;
    }

    // 设置参数
    bool success = true;
    success &= m_Acquisition->SetParameter(CORACQ_PRM_CROP_WIDTH, width);
    success &= m_Acquisition->SetParameter(CORACQ_PRM_CROP_HEIGHT, height);
    success &= m_Acquisition->SetParameter(CORACQ_PRM_CROP_LEFT, x);
    success &= m_Acquisition->SetParameter(CORACQ_PRM_CROP_TOP, y);

    if (!success) {
        emit acquisitionError(tr("Failed to set image geometry parameters"));
        // 尝试恢复... 但这里可能比较复杂，暂时只返回失败
        return false;
    }

    // 重新创建缓冲区和传输对象
    m_Buffers = new SapBufferWithTrash(m_BufferCount, m_Acquisition);
    m_Xfer = new SapAcqToBuf(m_Acquisition, m_Buffers, XferCallBack, this);

    if (m_Buffers && !*m_Buffers) {
        if (!m_Buffers->Create()) {
            emit acquisitionError(tr("Failed to recreate buffer object"));
            return false;
        } else {
            m_Buffers->Clear();
        }
    }

    if (m_Xfer && m_Xfer->GetPair(0)) {
        if (!m_Xfer->GetPair(0)->SetCycleMode(SapXferPair::CycleNextWithTrash)) {
            emit acquisitionError(tr("Failed to set cycle mode"));
            return false;
        }
    }

    if (m_Xfer && !*m_Xfer && !m_Xfer->Create()) {
        emit acquisitionError(tr("Failed to recreate transfer object"));
        return false;
    }

    if (wasGrabbing) {
        grabContinues();
    }

    return true;
}

int Image_Acquisition::getWidth() const
{
    // 如果是GigE模式，返回当前缓冲区的宽度
    if (m_ImageSourceType == ImageSourceType::GigE) {
        QMutexLocker locker(&m_CacheMutex);
        if (m_CurrentBufferIndex >= 0 && m_CurrentBufferIndex < m_SoftBuffers.size()) {
            return m_SoftBuffers[m_CurrentBufferIndex].width;
        }
        return 0;
    }
    
    if (!m_Acquisition) return 0;
    int value = 0;
    m_Acquisition->GetParameter(CORACQ_PRM_CROP_WIDTH, &value);
    return value;
}

int Image_Acquisition::getHeight() const
{
    // 如果是GigE模式，返回当前缓冲区的高度
    if (m_ImageSourceType == ImageSourceType::GigE) {
        QMutexLocker locker(&m_CacheMutex);
        if (m_CurrentBufferIndex >= 0 && m_CurrentBufferIndex < m_SoftBuffers.size()) {
            return m_SoftBuffers[m_CurrentBufferIndex].height;
        }
        return 0;
    }

    if (!m_Acquisition) return 0;
    int value = 0;
    m_Acquisition->GetParameter(CORACQ_PRM_CROP_HEIGHT, &value);
    return value;
}

int Image_Acquisition::getX() const
{
    if (!m_Acquisition) return 0;
    int value = 0;
    m_Acquisition->GetParameter(CORACQ_PRM_CROP_LEFT, &value);
    return value;
}

int Image_Acquisition::getY() const
{
    if (!m_Acquisition) return 0;
    int value = 0;
    m_Acquisition->GetParameter(CORACQ_PRM_CROP_TOP, &value);
    return value;
}

void Image_Acquisition::free()
{
    m_ServerNameBuffer.clear();
    m_ServerName = nullptr;

    if (m_Xfer && m_Xfer->IsGrabbing()) {
        stopGrab();
    }

    if (m_Xfer) {
        m_Xfer->Destroy();
        delete m_Xfer;
        m_Xfer = nullptr;
    }
    
    if (m_Buffers) {
        m_Buffers->Destroy();
        delete m_Buffers;
        m_Buffers = nullptr;
    }
    
    if (m_Acquisition) {
        m_Acquisition->Destroy();
        delete m_Acquisition;
        m_Acquisition = nullptr;
    }
}

#endif // ENABLE_SAPERA_CAMERA
