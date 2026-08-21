#include <M5Stack.h>
#include "Config.h"
#include "StateManager.h"
#include "DisplayManager.h"
#include "LedBuzzerManager.h"
#include "NetworkManager.h"
#include "StickerSosManager.h"
#include "EspNowManager.h"

unsigned long lastConfigFetchTimer = 0;

void setup() {
  M5.begin();

  displayMgr.init();
  ledBuzzerMgr.init();

  // Wi-Fi接続・設定 (WiFiManager) & NTP
  networkMgr.init();

  // ESP-NOW初期化
  EspNowManager::init();

  // サーバー設定取得
  networkMgr.fetchStationConfig();

  StateManager::changeState(STATE_IDLE);
}

void loop() {
  M5.update();

  // ボタンB：設定・状態表示のトグル
  if (M5.BtnB.wasPressed()) {
    if (StateManager::currentState != STATE_SHOW_SETTING) {
      StateManager::changeState(STATE_SHOW_SETTING);
    } else {
      StateManager::changeState(STATE_IDLE);
    }
  }

  // ボタンC長押し：Wi-Fi設定リセット
  if (M5.BtnC.pressedFor(1000)) {
    networkMgr.resetSettings();
  }

  // 定期的な設定ポーリング
  if (millis() - lastConfigFetchTimer > CONFIG_FETCH_INTERVAL) {
    lastConfigFetchTimer = millis();
    networkMgr.fetchStationConfig();
  }

  // タイマー状態の自動復帰
  StateManager::checkStateTimeout(5000);

  delay(10);
}