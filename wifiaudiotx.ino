#include <Arduino.h>
#include <WiFi.h>
#include "AudioSampler.h"
// wifi 的基本配置
#define SSID "OPPO Find X"
#define PASSWORD "12345678"

//#define SSID "BLabAudio"
//#define PASSWORD "dengyi1991"
bool wifiConnected = false;
bool serverConnected = false;
bool mute = false;
// i2s配置
i2s_config_t i2sConfig = { .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                           .sample_rate = 44100,
                           .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
                           .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
                           .communication_format =
                             i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
                           .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
                           .dma_buf_count = 16,
                           .dma_buf_len = 1024,
                           .use_apll = false,
                           .tx_desc_auto_clear = false,
                           .fixed_mclk = 0
                         };
// i2s pin配置
i2s_pin_config_t i2SPinConfig = { .bck_io_num = GPIO_NUM_5,
                                  .ws_io_num = GPIO_NUM_19,
                                  .data_out_num = I2S_PIN_NO_CHANGE,
                                  .data_in_num = GPIO_NUM_18
                                };
AudioSampler *audioSampler = NULL;


/**
   发送缓冲数组到服务器方法
   @param param
*/
void transmitTask(void *param) {
  AudioSampler *audioSampler = (AudioSampler *)param;
  //socket连接服务器
  WiFiClient *wifiClient = new WiFiClient();
  while (!wifiClient->connect("192.168.43.121", 8888)) {
    //  while (!wifiClient->connect("192.168.101.2", 8888)) {
    delay(500);
  }
  serverConnected = true;
  wifiClient->setNoDelay(true);
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
  unsigned long startTime;
  unsigned long endTime;
  while (true) {
    // 等待队列通知
    uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    if (ulNotificationValue > 0) {
      //wifi server 都连接上同时未静音才发送数据
      if (wifiConnected && serverConnected && !mute) {
        Serial.print("start-->");
        startTime = millis();
        Serial.print(startTime);
        Serial.print("---->");
        wifiClient->flush();
        int sendNum = wifiClient->write((uint8_t *)audioSampler->getTransmitBuffer(), audioSampler->getTransmitPackageSize());
        Serial.print("end-->");
        endTime = millis();
        Serial.print(endTime);
        Serial.print("---->");
        Serial.print("total--->");
        Serial.println(endTime - startTime);
      }
    }
  }
}
void wifiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_CONNECTED:
      wifiConnected = true;
      //led亮起
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      wifiConnected = false;
      //led灭并重启系统
      ESP.restart();
      break;
    default: break;
  }
}
void setup() {
  Serial.begin(115200);
  //初始化并连接WiFi
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(wifiEvent);
  WiFi.begin(SSID, PASSWORD);
  //循环等待连接上WiFi
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(1000);
  }
  //创建取样器对象
  audioSampler = new AudioSampler();
  //创建发送句柄
  TaskHandle_t transmitHandle;
  xTaskCreate(transmitTask, "transmitTask", 10240, audioSampler, 1,
              &transmitHandle);
  //启动采样
  audioSampler->start(i2sConfig, i2SPinConfig, 2048, transmitHandle);
}

void loop() {}
