//
// Created by BLab on 2021/10/5.
//

#include <Arduino.h>
#include "driver/i2s.h"
#include "AudioSampler.h"

void AudioSampler::processData(int16_t *i2sData, size_t bytesRead) {
  for (int i = 0; i < bytesRead / 2; i++) {
    addSingleData(i2sData[i]);
  }
}

void AudioSampler::addSingleData(int16_t singleData) {
  this->currentAudioBuffer[this->bufferPointer++] = singleData;
  if (this->bufferPointer == this->transmitPackageSize) {
    //过滤器处理

    //如果采样缓冲池满了就交换
    std::swap(this->currentAudioBuffer, this->transmitAudioBuffer);
    this->bufferPointer = 0;
    xTaskNotify(this->transmitHandle, 1, eIncrement);
  }
}
void i2sSamplerTask(void *param) {
  AudioSampler *audioSampler = (AudioSampler *)param;
  while (true) {
    // 等待队列中有数据再处理
    i2s_event_t event;
    if (xQueueReceive(audioSampler->i2sQueue, &event, portMAX_DELAY) == pdPASS) {
      if (event.type == I2S_EVENT_RX_DONE) {
        size_t bytesRead = 0;
        do {
          // 从i2s中读取数据
          int16_t readData[1024];
          i2s_read(audioSampler->i2sPort, readData, 1024, &bytesRead, 10);
          // 处理原始数据
          audioSampler->processData(readData, bytesRead);
        } while (bytesRead > 0);
      }
    }
  }
}
void AudioSampler::start(i2s_config_t i2SConfig, i2s_pin_config_t i2SPinConfig,
                         int32_t transmitPackageSize, TaskHandle_t transmitHandle) {
  this->i2SConfig = i2SConfig;
  this->i2SPinConfig = i2SPinConfig;
  this->transmitPackageSize = transmitPackageSize / sizeof(int16_t);
  //保存通知句柄
  this->transmitHandle = transmitHandle;
  //分配buffer大小
  this->currentAudioBuffer = (int16_t *)malloc(transmitPackageSize);
  this->transmitAudioBuffer = (int16_t *)malloc(transmitPackageSize);
  //安装i2s驱动
  i2s_driver_install(this->i2sPort, &i2SConfig, 4, &i2sQueue);
  //设置i2spin
  i2s_set_pin(this->i2sPort, &i2SPinConfig);
  //在0核心中读取数据
  xTaskCreatePinnedToCore(i2sSamplerTask, "AudioSampler", 10240, this, 1, NULL,
                          0);
}
