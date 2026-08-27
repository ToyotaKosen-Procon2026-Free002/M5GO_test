#include "StickerSosManager.h"
#include "BleManager.h"
#include "EspNowManager.h"
#include "StateManager.h"
#include "LedBuzzerManager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#define API_BASE_URL "https://coco-seal.mydns.jp/api"

StickerSosManager stickerSosMgr;

StickerSosManager::StickerSosManager() {}

// 通過ログをクラウドへ送信
static void sendPassageLogToApi(const String& stationId, const String& childId, const String& stickerId) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(API_BASE_URL) + "/passage_logs";
  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<256> doc;
    doc["parent_id"] = stationId;
    doc["child_id"] = childId;
    doc["sticker_id"] = stickerId;

    String body;
    serializeJson(doc, body);
    http.POST(body);
    http.end();
  }
}

// SOS信号の真偽をサーバーへ検証依頼
static bool verifySosWithApi(const String& stationId, const String& childId) {
  if (WiFi.status() != WL_CONNECTED) return true; // オフライン時は安全のため本物扱い

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = String(API_BASE_URL) + "/sos/verify";
  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<256> doc;
    doc["parent_id"] = stationId;
    doc["child_id"] = childId;

    String body;
    serializeJson(doc, body);
    int httpCode = http.POST(body);
    if (httpCode == 200) {
      String res = http.getString();
      StaticJsonDocument<256> resDoc;
      deserializeJson(resDoc, res);
      http.end();
      return resDoc["is_real"] | true;
    }
    http.end();
  }
  return true;
}

// SOS共通処理 (ESP-NOW / LoRa)
void StickerSosManager::handleSos(const String& childId, const String& source) {
  String timestamp = bleMgr.getTimestamp();
  pendingSosLogs.push_back({childId, timestamp});

  bool isReal = verifySosWithApi(bleMgr.deviceId, childId);
  if (isReal) {
    ledBuzzerMgr.showRealSos();
    StateManager::changeState(STATE_SOS_ALERT);
  } else {
    ledBuzzerMgr.showDummySos();
    StateManager::changeState(STATE_DUMMY_SOS_ALERT);
  }
}

void StickerSosManager::handlePacket(const CommunicationPacket& packet, int rssi) {
  String childId = String(packet.device_id);
  String timestamp = bleMgr.getTimestamp();

  if (packet.type == 1) { // SOSパケット受信
    handleSos(childId, "ESP-NOW");
  } 
  else if (packet.type == 0) { // 通過・シール要求
    pendingDistributeLogs.push_back({childId, timestamp});

    if (rssi >= RSSI_THRESHOLD) {
      // 1日1回配布制限チェック
      auto it = std::find(distributedTodayList.begin(), distributedTodayList.end(), childId);
      if (it == distributedTodayList.end()) {
        distributedTodayList.push_back(childId);
        
        // シール配布パケット送信
        EspNowManager::sendSticker(bleMgr.deviceId, bleMgr.distributeStickerId);
        
        // サーバーへログ送信
        sendPassageLogToApi(bleMgr.deviceId, childId, bleMgr.distributeStickerId);

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