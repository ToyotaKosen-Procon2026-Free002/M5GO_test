#include "DisplayManager.h"
#include "StateManager.h"
#include "BleManager.h"
#include "StickerSosManager.h"

DisplayManager displayMgr;

DisplayManager::DisplayManager() : sprite(nullptr) {}

bool DisplayManager::init() {
  M5.Lcd.setBrightness(100);
  M5.Lcd.fillScreen(BLACK);

  if (sprite == nullptr) {
    sprite = new TFT_eSprite(&M5.Lcd);
    sprite->setColorDepth(8);
    sprite->createSprite(320, 240);
    sprite->setTextSize(2);
  }
  return true;
}

void DisplayManager::update() {
  if (sprite == nullptr) return;

  sprite->fillScreen(BLACK);
  sprite->setCursor(10, 10);

  String devId = bleMgr.deviceId.isEmpty() ? "M5-INIT" : bleMgr.deviceId;
  String spot = bleMgr.spotName.isEmpty() ? "未設定" : bleMgr.spotName;
  String sticker = bleMgr.distributeStickerId.isEmpty() ? "none" : bleMgr.distributeStickerId;

  switch (StateManager::currentState) {
    case STATE_BLE_CONNECTED:
      sprite->setTextColor(CYAN);
      sprite->println("=== iPad Connected ===");
      sprite->println("\nSyncing with App...");
      sprite->println("Updating Settings");
      break;

    case STATE_IDLE:
      sprite->setTextColor(WHITE);
      sprite->println("=== Station Mode ===");
      sprite->printf("Spot: %s\n", spot.c_str());
      sprite->printf("ID  : %s\n\n", devId.c_str());
      sprite->setTextColor(YELLOW);
      sprite->println("[Distribute Sticker]");
      sprite->println(sticker);
      break;

    case STATE_STICKER_DISPLAY:
      sprite->setTextColor(GREEN);
      sprite->println("=== Distributed ===");
      sprite->println("\nSticker Sent!");
      sprite->printf("ID: %s\n", sticker.c_str());
      break;

    case STATE_SOS_ALERT:
      sprite->setTextColor(RED);
      sprite->println("!! SOS ALERT !!");
      sprite->println("\nSOS Signal Recv!");
      break;

    case STATE_DUMMY_SOS_ALERT:
      sprite->setTextColor(ORANGE);
      sprite->println("-- DUMMY SOS --");
      sprite->println("\nTest Signal Recv");
      break;

    case STATE_SHOW_SETTING:
      sprite->setTextColor(CYAN);
      sprite->println("=== Settings (BLE) ===");
      sprite->printf("ID     : %s\n", devId.c_str());
      sprite->printf("Spot   : %s\n", spot.c_str());
      sprite->printf("Sticker: %s\n", sticker.c_str());
      sprite->printf("Logs   : D:%d / S:%d\n", 
                    (int)stickerSosMgr.pendingDistributeLogs.size(), 
                    (int)stickerSosMgr.pendingSosLogs.size());
      sprite->printf("Sync   : %s\n", bleMgr.lastSyncTime.c_str());
      break;
  }
  
  sprite->pushSprite(0, 0);
}