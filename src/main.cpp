#include <M5Stack.h>
#include <WiFi.h>
#include "Config.h"
#include "StateManager.h"
#include "DisplayManager.h"
#include "LedBuzzerManager.h"
#include "BleManager.h"
#include "StickerSosManager.h"
#include "EspNowManager.h"

void setup() {
  // 1. ハード初期化（画面・シリアルのみ）
  M5.begin(true, false, true, false);

  // 2. 画面・LED初期化
  displayMgr.init();
  ledBuzzerMgr.init();

  // 3. Wi-Fi STAモード（ESP-NOW & MAC取得用）
  WiFi.mode(WIFI_STA);
  delay(100);

  // 4. BLE初期化（deviceId確定）
  bleMgr.init();

  // 5. ESP-NOW初期化
  EspNowManager::init();

  // 6. 初回描画
  StateManager::changeState(STATE_IDLE);
}

void loop() {
  M5.update();

  // ボタンB：設定画面と通常画面の切り替え
  if (M5.BtnB.wasPressed()) {
    if (StateManager::currentState != STATE_SHOW_SETTING) {
      StateManager::changeState(STATE_SHOW_SETTING);
    } else {
      StateManager::changeState(STATE_IDLE);
    }
  }

  // アラート／配布画面の5秒自動復帰
  StateManager::checkStateTimeout(5000);

  delay(10);
}