#include "StickerSosManager.h"
#include "NetworkManager.h"
#include "EspNowManager.h"
#include "StateManager.h"
#include "LedBuzzerManager.h"

StickerSosManager stickerSosMgr;

StickerSosManager::StickerSosManager() : lastCheckedDay(-1) {}

void StickerSosManager::checkDailyReset() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    if (lastCheckedDay != -1 && lastCheckedDay != timeinfo.tm_mday) {
      distributedTodayList.clear();
    }
    lastCheckedDay = timeinfo.tm_mday;
  }
}

void StickerSosManager::handlePacket(const CommunicationPacket& packet, int rssi) {
  String childId = String(packet.device_id);
  String timestamp = networkMgr.getTimestamp();

  if (packet.type == 1) { // SOS
    pendingSosLogs.push_back({childId, timestamp});
    bool isReal = networkMgr.verifyAndSendSos(childId, timestamp);
    
    if (isReal) {
      ledBuzzerMgr.showRealSos();
      StateManager::changeState(STATE_SOS_ALERT);
    } else {
      ledBuzzerMgr.showDummySos();
      StateManager::changeState(STATE_DUMMY_SOS_ALERT);
    }
  } 
  else if (packet.type == 0) { // 通過 & シール要求
    checkDailyReset();
    pendingDistributeLogs.push_back({childId, timestamp});
    networkMgr.syncDistributeLogs(pendingDistributeLogs);

    if (rssi >= RSSI_THRESHOLD) {
      auto it = std::find(distributedTodayList.begin(), distributedTodayList.end(), childId);
      if (it == distributedTodayList.end()) {
        distributedTodayList.push_back(childId);
        EspNowManager::sendSticker(networkMgr.deviceId, networkMgr.distributeStickerId);
        StateManager::changeState(STATE_STICKER_DISPLAY);
      }
    }
  }
}