//
// Created by BLab on 2021/10/5.
//

#ifndef WIFIAUDIOTX_AUDIOSAMPLER_H
#define WIFIAUDIOTX_AUDIOSAMPLER_H

#include "driver/i2s.h"
#include <WiFi.h>

class AudioSampler {
  private:
    // i2s序列
    i2s_port_t i2sPort = I2S_NUM_1;
    // i2s配置
    i2s_config_t i2SConfig;
    // i2s pin设置
    i2s_pin_config_t i2SPinConfig;
    //发送句柄
    TaskHandle_t transmitHandle;
    //两个缓冲buffer8个，2的3次方
    int16_t *currentAudioBuffer;
    //发送缓存
    int16_t *transmitAudioBuffer;
    // i2s队列
    QueueHandle_t i2sQueue;
    //数据指针
    int32_t bufferPointer = 0;
    //缓冲数据大小
    int32_t bufferArraySize;
    //发送数据包大小
    int32_t transmitPackageSize;
    /**
       处理数据方法，用于对剩余进行增益控制
       @param i2sData 原始数据
       @param bytesRead 读取到的数据数
    */
    void processData(int16_t *i2sData, size_t bytesRead);
    //添加单个数据到缓冲中
    void addSingleData(int16_t singleData);

  public:
    void start(i2s_config_t i2SConfig, i2s_pin_config_t i2SPinConfig,
               int32_t transmitPackageSize, TaskHandle_t transmitHandle);
    int16_t *getTransmitBuffer() {
      return transmitAudioBuffer;
    };
    int32_t getTransmitPackageSize() {
      return transmitPackageSize * sizeof(int16_t);
    };
    friend void i2sSamplerTask(void *param);
};
#endif // WIFIAUDIOTX_AUDIOSAMPLER_H
