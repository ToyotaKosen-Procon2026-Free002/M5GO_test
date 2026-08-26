#include "StickerSosManager.h"
#include "BleManager.h"
#include "EspNowManager.h"
#include "StateManager.h"
#include "LedBuzzerManager.h"

StickerSosManager stickerSosMgr;

StickerSosManager::StickerSosManager() {}

void StickerSosManager::handlePacket(const CommunicationPacket& packet, int rssi) {
  String childId = String(packet.device_id);
  String timestamp = bleMgr.getTimestamp();

  if (packet.type == 1) { // SOS受信
    pendingSosLogs.push_back({childId, timestamp});
    ledBuzzerMgr.showRealSos();
    StateManager::changeState(STATE_SOS_ALERT);
  } 
  else if (packet.type == 0) { // 通過・シール要求
    pendingDistributeLogs.push_back({childId, timestamp});

    if (rssi >= RSSI_THRESHOLD) {
      auto it = std::find(distributedTodayList.begin(), distributedTodayList.end(), childId);
      if (it == distributedTodayList.end()) {
        distributedTodayList.push_back(childId);
        EspNowManager::sendSticker(bleMgr.deviceId, bleMgr.distributeStickerId);
        StateManager::changeState(STATE_STICKER_DISPLAY);
      }
    }
  }
}

void StickerSosManager::flushLogsToBle() {
  String json = "{\"station_id\":\"" + bleMgr.deviceId + "\",";
  json += "\"encounter_logs\":[";
  for (size_t i = 0; i < pendingDistributeLogs.size(); i++) {
    json += "{\"device_id_2\":\"" + pendingDistributeLogs[i].device_id_2 + "\",\"device_timestamp\":\"" + pendingDistributeLogs[i].device_timestamp + "\"}";
    if (i < pendingDistributeLogs.size() - 1) json += ",";
  }
  json += "],\"sos_logs\":[";
  for (size_t i = 0; i < pendingSosLogs.size(); i++) {
    json += "{\"child_id\":\"" + pendingSosLogs[i].child_id + "\",\"device_timestamp\":\"" + pendingSosLogs[i].device_timestamp + "\"}";
    if (i < pendingSosLogs.size() - 1) json += ",";
  }
  json += "]}";

  bleMgr.sendLogsToApp(json);
  pendingDistributeLogs.clear();
  pendingSosLogs.clear();
}