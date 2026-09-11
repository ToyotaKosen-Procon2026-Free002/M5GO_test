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
  String childId = String(packet.device_id);
  String timestamp = bleMgr.getTimestamp();

  if (packet.type == 1) { // SOSパケット
    handleSos(childId, "ESP-NOW");
  } 
  else if (packet.type == 0) { // 通過・シール要求
    if (rssi >= RSSI_THRESHOLD) {
      // 1日1回配布制限チェック
      auto it = std::find(distributedTodayList.begin(), distributedTodayList.end(), childId);
      if (it == distributedTodayList.end()) {
        // 初回配布時のみ配布リストと未送信ログに登録（二重カウント防止）
        distributedTodayList.push_back(childId);
        pendingDistributeLogs.push_back({childId, timestamp});

        // 子機へシールを返信
        EspNowManager::sendSticker(bleMgr.deviceId, bleMgr.distributeStickerId);

        // 画面を配布完了に切り替え
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