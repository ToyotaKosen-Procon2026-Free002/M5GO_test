#include "DisplayManager.h"
#include "StateManager.h"
#include "NetworkManager.h"
#include "StickerSosManager.h"

DisplayManager displayMgr;

DisplayManager::DisplayManager() : sprite(&M5.Lcd) {}

void DisplayManager::init() {
  sprite.setColorDepth(8);
  sprite.setTextSize(2);
  sprite.createSprite(M5.Lcd.width(), M5.Lcd.height());
}

void DisplayManager::update() {
  sprite.fillScreen(BLACK);
  sprite.setCursor(0, 0);

  switch (StateManager::currentState) {
    case STATE_WIFI_CONFIG:
      sprite.setTextColor(YELLOW);
      sprite.println("=== WiFi Config Mode ===");
      sprite.println("Connect AP:");
      sprite.printf("SSID: COCO-%s\n\n", networkMgr.deviceId.c_str());
      sprite.setTextColor(WHITE);
      sprite.println("Open IP: 192.168.4.1");
      break;

    case STATE_IDLE:
      sprite.setTextColor(WHITE);
      sprite.println("=== Station Mode ===");
      sprite.printf("Spot: %s\n", networkMgr.spotName.c_str());
      sprite.printf("ID  : %s\n\n", networkMgr.deviceId.c_str());
      sprite.setTextColor(YELLOW);
      sprite.println("[Distribute Sticker]");
      sprite.println(networkMgr.distributeStickerId);
      break;

    case STATE_STICKER_DISPLAY:
      sprite.setTextColor(GREEN);
      sprite.println("=== Distributed ===");
      sprite.println("\nSticker Sent!");
      sprite.printf("ID: %s\n", networkMgr.distributeStickerId.c_str());
      break;

    case STATE_SOS_ALERT:
      sprite.setTextColor(RED);
      sprite.println("!! SOS ALERT !!");
      sprite.println("\nREAL SOS Triggered!");
      sprite.println("Notified to Server");
      break;

    case STATE_DUMMY_SOS_ALERT:
      sprite.setTextColor(ORANGE);
      sprite.println("-- DUMMY SOS --");
      sprite.println("\nTest Signal Recv");
      break;

    case STATE_SHOW_SETTING:
      sprite.setTextColor(CYAN);
      sprite.println("=== Settings ===");
      sprite.printf("ID     : %s\n", networkMgr.deviceId.c_str());
      sprite.printf("Spot   : %s\n", networkMgr.spotName.c_str());
      sprite.printf("WiFi   : %s\n", WiFi.SSID().c_str());
      sprite.printf("Sticker: %s\n", networkMgr.distributeStickerId.c_str());
      sprite.printf("Pending: D:%d / S:%d\n", 
                    stickerSosMgr.pendingDistributeLogs.size(), 
                    stickerSosMgr.pendingSosLogs.size());
      sprite.printf("Sync   : %s\n", networkMgr.lastSyncTime.c_str());
      break;
  }
  sprite.pushSprite(0, 0);
}