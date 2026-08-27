#include <M5Stack.h>
#include <WiFi.h>
#include "DisplayManager.h"
#include "StateManager.h"
#include "BleManager.h"
#include "EspNowManager.h"
#include "LedBuzzerManager.h"
#include "StickerSosManager.h"

#define LORA_RX_PIN 16
#define LORA_TX_PIN 17

void setup() {
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);

  // 1. ハードウェアマネージャーの初期化
  displayMgr.init();
  ledBuzzerMgr.init();

  // 2. ESP-NOWを動作させるためにWi-FiドライバをSTAモードで有効化
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // 3. ESP-NOW & BLE 初期化
  EspNowManager::init();
  bleMgr.init();

  // 4. 初期画面表示
  StateManager::changeState(STATE_IDLE);
}

void loop() {
  M5.update();

  // 中央ボタン(BtnB)で設定画面と通常画面のトグル
  if (M5.BtnB.wasPressed()) {
    if (StateManager::currentState == STATE_SHOW_SETTING) {
      StateManager::changeState(STATE_IDLE);
    } else {
      StateManager::changeState(STATE_SHOW_SETTING);
    }
  }

  // LoRa経由の遠距離SOS信号受信
  if (Serial2.available() > 0) {
    String loraMsg = Serial2.readStringUntil('\n');
    loraMsg.trim();
    if (loraMsg.startsWith("SOS:")) {
      String childId = loraMsg.substring(4);
      stickerSosMgr.handleSos(childId, "LoRa");
    }
  }

  // 状態タイムアウト監視 (10秒でIDLE復帰)
  StateManager::checkStateTimeout(10000);

  delay(20);
}