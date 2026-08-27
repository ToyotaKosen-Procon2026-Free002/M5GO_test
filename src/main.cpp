#include <M5Stack.h>
#include "DisplayManager.h"
#include "StateManager.h"
#include "BleManager.h"
#include "EspNowManager.h"
#include "LedBuzzerManager.h"
#include "StickerSosManager.h"

/*
// LoRaモジュール接続ピン
#define LORA_RX_PIN 16
#define LORA_TX_PIN 17
*/

void setup() {
  M5.begin();
  M5.Power.begin();
  Serial.begin(115200);
  //Serial2.begin(9600, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);

  // マネージャー初期化
  displayMgr.init();
  ledBuzzerMgr.init();

  // BLE & ESP-NOW 開始
  bleMgr.init();
  EspNowManager::init();

  StateManager::changeState(STATE_IDLE);
}

void loop() {
  M5.update();

  // 1. 中央ボタン(BtnB)押下でSHOW_SETTINGと通常画面を切り替え
  if (M5.BtnB.wasPressed()) {
    if (StateManager::currentState == STATE_SHOW_SETTING) {
      StateManager::changeState(STATE_IDLE);
    } else {
      StateManager::changeState(STATE_SHOW_SETTING);
    }
  }

  /*
  // 2. LoRa経由の遠距離SOS信号受信
  if (Serial2.available() > 0) {
    String loraMsg = Serial2.readStringUntil('\n');
    loraMsg.trim();
    if (loraMsg.startsWith("SOS:")) {
      String childId = loraMsg.substring(4);
      stickerSosMgr.handleSos(childId, "LoRa");
    }
  }
  */

  // 3. 画面タイムアウト監視 (配布表示・SOS表示から10秒でIDLEに戻る)
  StateManager::checkStateTimeout(10000);

  delay(20);
}