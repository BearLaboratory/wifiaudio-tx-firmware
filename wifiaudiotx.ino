//--------引入库-----------
#include <Arduino.h>
#include <WiFi.h>
#include "AudioSampler.h"
//--------定义常量---------
#define OPEN_DEBUG true //正式运行的时候请设置为false
#define SSID "BLabAudio"
#define PASSWORD "dengyi1991"
#define SERVER_IP "192.168.101.2"
#define SERVER_PORT 8888
#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  600        //睡眠时间10分钟，到时会醒来检查一次，还是没电会继续睡
#define MUTEPIN 12
#define TOUCH_THRESHOLD 40        //触摸灵敏度阈值，越大越灵敏
#define STATUSPIN 14
#define ADCPIN 34
//-------定义变量----------
volatile bool wifiConnected = false;
volatile bool mute = false;
volatile int statusLedState = LOW;
volatile unsigned long sinceLastTouch = 0;
volatile bool inited = false;
// i2s配置
i2s_config_t i2sConfig = { .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                           .sample_rate = 44100,
                           .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
                           .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
                           .communication_format =
                             i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
                           .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
                           .dma_buf_count = 4,
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
   静音中断函数
*/
void muteTouch() {
  if (millis() - sinceLastTouch < 1000) return;
  sinceLastTouch = millis();
  mute = !mute;
  if (mute) {
    digitalWrite(MUTEPIN, HIGH);
  } else {
    digitalWrite(MUTEPIN, LOW);
  }
}

/**
   发送缓冲数组到服务器方法
   @param param
*/
void transmitTask(void *param) {
  AudioSampler *audioSampler = (AudioSampler *)param;
  //socket连接服务器
  WiFiClient *wifiClient = new WiFiClient();
  //  while (!wifiClient->connect("192.168.43.121", 8888)) {
  while (!wifiClient->connect(SERVER_IP, SERVER_PORT)) {
    delay(100);
  }
  wifiClient->setNoDelay(true);
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
  unsigned long startTime;
  unsigned long endTime;
  while (true) {
    // 等待队列通知
    uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    if (ulNotificationValue > 0) {
      //wifi连接上同时未静音才发送数据
      if (wifiConnected && !mute) {
//        Serial.print("start-->");
//        startTime = millis();
//        Serial.print(startTime);
//        Serial.print("---->");
        int sendNum = wifiClient->write((uint8_t *)audioSampler->getTransmitBuffer(), audioSampler->getTransmitPackageSize());
//        Serial.print("end-->");
//        endTime = millis();
//        Serial.print(endTime);
//        Serial.print("---->");
//        Serial.print("total--->");
//        Serial.println(endTime - startTime);
      } else {
        //未连接时情况tcp缓存
        wifiClient->flush();

      }
    }
  }
}
void wifiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_CONNECTED:
      wifiConnected = true;
      //led亮起
      digitalWrite(STATUSPIN, HIGH);
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      //回调会多次执行，所以要判断一下
      if (inited) {
        wifiConnected = false;
        digitalWrite(STATUSPIN, LOW);
        ESP.restart();
      }
      break;
    default: break;
  }
}
void setup() {
  //状态led初始化
  pinMode(MUTEPIN, OUTPUT);
  pinMode(STATUSPIN, OUTPUT);
  digitalWrite(STATUSPIN, LOW);
  digitalWrite(MUTEPIN, LOW);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //读取电压，电池电压低于3.7v就直接睡眠，ADC值大概算了一下在2300未精确测量，睡10分钟然后醒来检测一次
  if (analogRead(ADCPIN) <= 2300) {
    esp_deep_sleep_start();
  }
  //是否开启debug
  if (OPEN_DEBUG) {
    Serial.begin(115200);
  }
  //初始化并连接WiFi
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(wifiEvent);
  WiFi.begin(SSID, PASSWORD);
  //循环等待连接上WiFi
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(500);
    if (statusLedState == LOW) {
      statusLedState = HIGH;
    } else {
      statusLedState = LOW;
    }
    digitalWrite(STATUSPIN, statusLedState);
    Serial.println(statusLedState);
  }
  //创建取样器对象
  audioSampler = new AudioSampler();
  //创建发送句柄
  TaskHandle_t transmitHandle;
  xTaskCreate(transmitTask, "transmitTask", 10240, audioSampler, 1,
              &transmitHandle);
  //启动采样
  audioSampler->start(i2sConfig, i2SPinConfig, 2048, transmitHandle);
  //触摸T0对应的是gpio4
  touchAttachInterrupt(T0, muteTouch, TOUCH_THRESHOLD);
  //初始化完将初始化状态置为true
  inited = true;
}

void loop() {}
