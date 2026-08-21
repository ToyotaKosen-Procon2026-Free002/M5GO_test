#include "NetworkManager.h"
#include "StateManager.h"

NetworkManager networkMgr;

static void configModeCallback(WiFiManager* myWiFiManager) {
  StateManager::changeState(STATE_WIFI_CONFIG);
}

void NetworkManager::init() {
  spotName = "未登録スポット";
  distributeStickerId = "none";
  lastSyncTime = "未同期";

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char idBuf[16];
  snprintf(idBuf, sizeof(idBuf), "M5-%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);
  deviceId = String(idBuf);

  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  wm.setConfigPortalTimeout(180);

  String apName = "COCO-STATION-" + deviceId;
  wm.autoConnect(apName.c_str());

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
}

void NetworkManager::resetSettings() {
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

String NetworkManager::getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01 00:00:00";
  }
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

void NetworkManager::fetchStationConfig() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  if (http.begin(client, String(SERVER_URL) + "/api/stations/" + deviceId + "/config")) {
    int code = http.GET();
    if (code == 200) {
      String response = http.getString();
      int idxSticker = response.indexOf("\"distribute_sticker_id\":\"");
      if (idxSticker != -1) {
        int start = idxSticker + 25;
        int end = response.indexOf("\"", start);
        distributeStickerId = response.substring(start, end);
      }
      int idxSpot = response.indexOf("\"spot_name\":\"");
      if (idxSpot != -1) {
        int start = idxSpot + 13;
        int end = response.indexOf("\"", start);
        spotName = response.substring(start, end);
      }
      lastSyncTime = "OK (" + getTimestamp() + ")";
    }
    http.end();
  }
}

void NetworkManager::syncDistributeLogs(std::vector<DistributeLog>& logs) {
  if (logs.empty() || WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;
  if (http.begin(client, String(SERVER_URL) + "/api/stations/logs/distribute")) {
    http.addHeader("Content-Type", "application/json");

    String body = "{\"station_device_id\":\"" + deviceId + "\",\"logs\":[";
    for (size_t i = 0; i < logs.size(); i++) {
      body += "{\"device_id\":\"" + logs[i].child_device_id + "\",";
      body += "\"device_timestamp\":\"" + logs[i].device_timestamp + "\"}";
      if (i < logs.size() - 1) body += ",";
    }
    body += "]}";

    int code = http.POST(body);
    if (code == 200) {
      logs.clear();
    }
    http.end();
  }
}

bool NetworkManager::verifyAndSendSos(const String& childId, const String& timestamp) {
  bool isReal = true;
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    if (http.begin(client, String(SERVER_URL) + "/api/stations/logs/sos")) {
      http.addHeader("Content-Type", "application/json");

      String body = "{\"station_device_id\":\"" + deviceId + "\",";
      body += "\"child_device_id\":\"" + childId + "\",";
      body += "\"device_timestamp\":\"" + timestamp + "\"}";

      int code = http.POST(body);
      if (code == 200) {
        String res = http.getString();
        if (res.indexOf("\"is_real\":false") != -1 || res.indexOf("\"is_real\": false") != -1) {
          isReal = false;
        }
      }
      http.end();
    }
  }
  return isReal;
}