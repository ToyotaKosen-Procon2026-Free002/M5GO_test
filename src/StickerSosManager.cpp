#include "StickerSosManager.h"
#include "BleManager.h"
#include "EspNowManager.h"
#include "StateManager.h"
#include "LedBuzzerManager.h"

StickerSosManager stickerSosMgr;

StickerSosManager::StickerSosManager() {}

// SOS共通ハンドラ (ESP-NOW / LoRa共通)
void StickerSosManager::handleSos(const String& childId, const String& source) {
  String timestamp = bleMgr.getTimestamp();
  
  // SOSログを溜める
  pendingSosLogs.push_back({childId, timestamp});

  // 赤色LED・警告ブザー・画面表示
  ledBuzzerMgr.showRealSos();
  StateManager::changeState(STATE_SOS_ALERT);
}

void StickerSosManager::handlePacket(const CommunicationPacket& packet, int rssi) {
  String childId = String(packet.device_id);
  String timestamp = bleMgr.getTimestamp();

  // 1. ESP-NOW経由でSOSを受信した場合
  if (packet.type == 1) {
    handleSos(childId, "ESP-NOW");
  } 
  // 2. 通過・シール要求パケットを受信した場合
  else if (packet.type == 0) {
    // 通過ログを溜める
    pendingDistributeLogs.push_back({childId, timestamp});

    // 電波強度が一定以上の場合のみシールを配布
    if (rssi >= RSSI_THRESHOLD) {
      // 1日1回重複チェック
      auto it = std::find(distributedTodayList.begin(), distributedTodayList.end(), childId);
      if (it == distributedTodayList.end()) {
        distributedTodayList.push_back(childId);
        
        // 子機へシールをESP-NOWで送信
        EspNowManager::sendSticker(bleMgr.deviceId, bleMgr.distributeStickerId);
        
        // 画面を「配布完了」に切り替え
        StateManager::changeState(STATE_STICKER_DISPLAY);
      }
    }
  }
}

// アプリから「GET_LOGS」が送られてきたときにBLEでログを一括送信
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
  
  // 送信完了後にログキャッシュをクリア
  pendingDistributeLogs.clear();
  pendingSosLogs.clear();
}