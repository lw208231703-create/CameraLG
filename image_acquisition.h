#ifndef IMAGE_ACQUISITION_H
#define IMAGE_ACQUISITION_H
#pragma once

#ifdef ENABLE_SAPERA_CAMERA
#include <SapClassBasic.h>
#include <sapBuffer.h>
#endif

#include <QObject>
#include <QImage>
#include <QStringList>
#include <QVector>
#include <QMutex>
#include <atomic>

// Image source type enumeration
enum class ImageSourceType {
    Sapera,    // Sapera capture card
    GigE       // GigE camera
};

class Image_Acquisition : public QObject
{
    Q_OBJECT
public:
    // 常量定义
    static const QString NoServerFoundMessage;
    static const QString NoConfigFoundMessage;
    
    Image_Acquisition(QObject *parent = nullptr);
    ~Image_Acquisition();
    
    void free();                                              //释放资源
    bool init_camera();                                       //初始化相机（使用当前选择的服务器和配置）
    bool initDevice(const QString &serverName, const QString &ccfPath); //初始化采集卡
    bool grabOnce();                                          //单次采图（不保存，存入内存）
    bool grabContinues();                                     //连续采图
    bool stopGrab();                                          //停止采图
    
    // 新增功能
    static QStringList getAvailableServers();                 //获取所有可用的采集卡服务器名称
    static QStringList getConfigurationFiles(const QString &directory);  //获取配置文件列表
    
    void setBufferCount(int count);                           //设置缓冲区数量
    int getBufferCount() const;                               //获取缓冲区数量
    
    QImage getBufferImage(int bufferIndex);                   //获取指定缓冲区的图像
    int getCurrentBufferIndex() const;                        //获取当前缓冲区索引
    
    // 获取指定缓冲区的原始数据 (16位)
    QVector<uint16_t> getBufferRawData(int bufferIndex, int &width, int &height, int &bitDepth);

    bool saveImageToFile(int bufferIndex, const QString &filePath);  //保存指定缓冲区图像到文件
    
    void setSelectedServer(const QString &serverName);        //设置选择的服务器
    void setConfigPath(const QString &ccfPath);               //设置配置文件路径
    
    QString getSelectedServer() const { return m_SelectedServer; }
    QString getConfigPath() const { return m_ConfigPath; }
    
    // 清理缓冲区内存
    void clearBuffers();
    
    // 检查是否已初始化
    bool isInitialized() const { 
        if (m_ImageSourceType == ImageSourceType::GigE) {
            return !m_SoftBuffers.isEmpty();
        }
        return m_Acquisition != nullptr && m_Buffers != nullptr; 
    }
    
    // 检查是否正在采集
    bool isGrabbing() const;

    // 设置图像大小和起始位置
    bool setImageGeometry(int width, int height, int x, int y);
    
    // 获取当前图像几何参数
    int getWidth() const;
    int getHeight() const;
    int getX() const;
    int getY() const;

    // 设置位段提取的起始位 (0-8)
    void setBitShift(int shift);
    int getBitShift() const { return m_BitShift; }

    // GigE camera image injection interface (Zero-Copy optimization)
    void injectGigeImage(const uchar* pData, int width, int height, int bitDepth);

    // Set and get current image source type
    void setImageSourceType(ImageSourceType type) { m_ImageSourceType = type; }
    ImageSourceType getImageSourceType() const { return m_ImageSourceType; }

signals:
    void imageReady(int bufferIndex);                          //图像就绪信号（仅发送索引，避免大对象拷贝）
    void acquisitionError(const QString &error);               //采集错误信号
    void acquisitionStarted();                                 //采集开始信号
    void acquisitionStopped();                                 //采集停止信号

private:
    void processNewImage();                                    //处理新图像并发送信号
    QImage convertBufferToQImage(int bufferIndex);            //将缓冲区数据转换为QImage
    
    // Sapera SDK对象 - 私有成员
    QByteArray m_ServerNameBuffer;                             //服务器名称缓冲区
    char *m_ServerName{nullptr};                               //服务器名称指针
#ifdef ENABLE_SAPERA_CAMERA
    SapAcquisition *m_Acquisition{nullptr};                    //控制与板卡相连接的设备
    SapBufferWithTrash *m_Buffers{nullptr};                    //垃圾缓冲区
    SapView *m_View{nullptr};                                  //在窗口中显示SapBuffer对象的资源
    SapTransfer *m_Xfer{nullptr};                              //管理通用传输过程的功能
#endif
    
    QString m_SelectedServer;                                  //当前选择的服务器名称
    QString m_ConfigPath;                                      //当前配置文件路径
    int m_BufferCount{2};                                      //缓冲区数量，默认2个
    int m_CurrentBufferIndex{0};                               //当前缓冲区索引
    int m_BitShift{6};                                         //位段提取起始位，默认6 (6-13位)
    
    // 帧计数器，使用原子变量保证线程安全
    std::atomic<int> m_FrameCount{0};
    
    // 存储在内存中的图像数据
    // 移除 m_CachedImages 和 m_RawBuffers，直接使用底层 SapBuffer
    mutable QMutex m_CacheMutex;                              // SapBuffer访问互斥锁
    
    // GigE camera image data storage
    ImageSourceType m_ImageSourceType{ImageSourceType::Sapera};
    
    // 软件缓冲区结构体，用于存储GigE历史帧
    struct SoftBufferFrame {
        QVector<uint16_t> rawData;   // 用于>8位图像
        QVector<uint8_t> rawData8;   // 用于8位图像
        int width{0};
        int height{0};
        int bitDepth{0};
        bool isEmpty{true};
    };

    QVector<SoftBufferFrame> m_SoftBuffers;

#ifdef ENABLE_SAPERA_CAMERA
    friend void XferCallBack(SapXferCallbackInfo *pInfo);
#endif
};

#endif // IMAGE_ACQUISITION_H
