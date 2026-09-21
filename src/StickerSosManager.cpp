#include "StickerSosManager.h"
#include "BleManager.h"
#include "EspNowManager.h"
#include "StateManager.h"
#include "LedBuzzerManager.h"

StickerSosManager stickerSosMgr;

StickerSosManager::StickerSosManager() {}

void StickerSosManager::handleSos(const String& childId, const String& source) {
  String timestamp = bleMgr.getTimestamp();
  pendingSosLogs.push_back({childId, timestamp});

  ledBuzzerMgr.showRealSos();
  StateManager::changeState(STATE_SOS_ALERT);
}

void StickerSosManager::handlePacket(const CommunicationPacket& packet, int rssi) {
  String senderId = String(packet.device_id);
  String timestamp = bleMgr.getTimestamp();

  // ====================================================
  // 親機判定（相手が親機ならシール配布・すれ違いログをスキップ）
  // ====================================================
  if (packet.isGateway) {
    Serial.printf("[ESP-NOW] Another Gateway detected: %s (Ignored)\n", senderId.c_str());
    return;
  }

  // 以降は子機（isGateway == false）からのパケットのみ処理
  if (packet.type == 1) { // SOSパケット
    handleSos(senderId, "ESP-NOW");
  } 
  else if (packet.type == 0) { // 通過・シール要求
    if (rssi >= RSSI_THRESHOLD) {
      auto it = std::find(distributedTodayList.begin(), distributedTodayList.end(), senderId);
      if (it == distributedTodayList.end()) {
        distributedTodayList.push_back(senderId);
        pendingDistributeLogs.push_back({senderId, timestamp});

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