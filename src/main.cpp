#include <M5Stack.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <vector>
#include <algorithm>

// ==========================================
// 1. 定数・設定値・データ構造
// ==========================================
#define LED_BAR_PIN 15
#define NUM_LED 10
const int RSSI_THRESHOLD = -60;

const char* SERVER_URL = "http://157.17.49.151";

// 接続候補のWiFiリスト
struct WifiCredential {
  const char* ssid;
  const char* password;
};

const WifiCredential KNOWN_AP_LIST[] = {
  {"icewave-G", "20240831A#z"},
  {"Home-WiFi",  "password1234"}
};
const size_t KNOWN_AP_COUNT = sizeof(KNOWN_AP_LIST) / sizeof(KNOWN_AP_LIST[0]);

enum SystemState {
  STATE_IDLE,             // 通常待機
  STATE_STICKER_DISPLAY,  // SPシール配布完了表示
  STATE_SOS_ALERT,        // 子機SOS(本物)
  STATE_DUMMY_SOS_ALERT,  // 子機SOS(ダミー)
  STATE_SHOW_SETTING      // 設定表示
};

struct CommunicationPacket {
  char device_id[16];   // 子機ID
  int type;             // 0: 通過検知, 1: SOS
  char stickerId[16];  // 配布シールID
};

struct DistributeLog {
  String device_id;
  String device_timestamp;
};

struct SosLog {
  String device_id;
  String device_timestamp;
};

// ==========================================
// 2. デバイス制御層 (Device Control Layer)
// ==========================================
class BuzzerManager {
public:
  void init() {}
  void alertSOS() {
    M5.Speaker.tone(1000, 500);
  }
};

class LedManager {
private:
  Adafruit_NeoPixel pixels;
public:
  LedManager() : pixels(NUM_LED, LED_BAR_PIN, NEO_GRB + NEO_KHZ800) {}

  void init() {
    pixels.begin();
    clear();
  }

  void alertSOS() {
    pixels.fill(pixels.Color(255, 0, 0), 0, NUM_LED);
    pixels.show();
  }

  void clear() {
    pixels.clear();
    pixels.show();
  }
};

class ButtonManager {
public:
  void update() {
    M5.update();
  }
  bool isSettingButtonPressed() {
    return M5.BtnB.wasPressed();
  }
  bool isTestButtonPressed() {
    return M5.BtnA.wasPressed();
  }
};

// ==========================================
// 3. 通信制御層 (Network Control Layer)
// ==========================================
class WifiManager {
private:
  unsigned long lastReconnectAttempt = 0;
  const unsigned long RECONNECT_INTERVAL = 30000;
  String connectedSSID = "未接続";

public:
  bool autoConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 10);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.println("Scanning WiFi...");

    int n = WiFi.scanNetworks();
    M5.Lcd.printf("Found %d APs:\n", n);

    // 検出されたSSIDを画面とシリアルに一覧表示（確認用）
    for (int i = 0; i < n && i < 6; ++i) {
      M5.Lcd.printf("- %s (%d)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      Serial.printf("[Scan] %s (RSSI: %d)\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
    delay(1500); // 画面で確認するための短い待機

    // 1. スキャン結果と登録リストを照合
    for (int i = 0; i < n; ++i) {
      String scannedSSID = WiFi.SSID(i);
      scannedSSID.trim(); // 前後の空白を除去

      for (size_t j = 0; j < KNOWN_AP_COUNT; ++j) {
        String targetSSID = String(KNOWN_AP_LIST[j].ssid);
        targetSSID.trim();

        if (scannedSSID.equalsIgnoreCase(targetSSID)) { // 大文字小文字を区別せず比較
          M5.Lcd.setTextColor(YELLOW, BLACK);
          M5.Lcd.printf("\nConnecting: %s\n", KNOWN_AP_LIST[j].ssid);
          WiFi.begin(KNOWN_AP_LIST[j].ssid, KNOWN_AP_LIST[j].password);

          int retry = 0;
          while (WiFi.status() != WL_CONNECTED && retry < 20) {
            delay(500);
            M5.Lcd.print(".");
            retry++;
          }

          if (WiFi.status() == WL_CONNECTED) {
            connectedSSID = KNOWN_AP_LIST[j].ssid;
            configTime(9 * 3600, 0, "ntp.nict.jp");
            M5.Lcd.setTextColor(GREEN, BLACK);
            M5.Lcd.println("\nWiFi OK!");
            delay(800);
            return true;
          }
        }
      }
    }

    // 2. マッチしなかった場合のフォールバック（デフォルトAPへ直接接続試行）
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("\nNo match! Fallback to 1st AP...");
    M5.Lcd.printf("Trying: %s\n", KNOWN_AP_LIST[0].ssid);
    
    WiFi.begin(KNOWN_AP_LIST[0].ssid, KNOWN_AP_LIST[0].password);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
      delay(500);
      M5.Lcd.print(".");
      retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      connectedSSID = KNOWN_AP_LIST[0].ssid;
      configTime(9 * 3600, 0, "ntp.nict.jp");
      M5.Lcd.setTextColor(GREEN, BLACK);
      M5.Lcd.println("\nWiFi OK (Fallback)!");
      delay(800);
      return true;
    }

    M5.Lcd.println("\nConnect Failed.");
    delay(1000);
    return false;
  }
  
  void maintainConnection() {
    if (WiFi.status() != WL_CONNECTED) {
      connectedSSID = "切断中";
      unsigned long currentMillis = millis();
      if (currentMillis - lastReconnectAttempt > RECONNECT_INTERVAL) {
        lastReconnectAttempt = currentMillis;
        autoConnect();
      }
    }
  }

  bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
  }

  String getConnectedSSID() const {
    return connectedSSID;
  }

  String getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      return "1970-01-01 00:00:00";
    }
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
  }

  bool sendDistributeLogs(const String& stationId, const std::vector<DistributeLog>& logs) {
    if (!isConnected() || logs.empty()) return false;
    WiFiClient client;
    HTTPClient http;
    if (http.begin(client, String(SERVER_URL) + "/api/stations/logs/distribute")) {
      http.addHeader("Content-Type", "application/json");
      String body = "{\"station_device_id\":\"" + stationId + "\",\"logs\":[";
      for (size_t i = 0; i < logs.size(); i++) {
        body += "{\"device_id\":\"" + logs[i].device_id + "\",\"device_timestamp\":\"" + logs[i].device_timestamp + "\"}";
        if (i < logs.size() - 1) body += ",";
      }
      body += "]}";
      int code = http.POST(body);
      http.end();
      return (code == 200);
    }
    return false;
  }

  bool sendSosLogs(const String& stationId, const std::vector<SosLog>& logs) {
    if (!isConnected() || logs.empty()) return false;
    WiFiClient client;
    HTTPClient http;
    if (http.begin(client, String(SERVER_URL) + "/api/stations/logs/sos")) {
      http.addHeader("Content-Type", "application/json");
      String body = "{\"station_device_id\":\"" + stationId + "\",\"logs\":[";
      for (size_t i = 0; i < logs.size(); i++) {
        body += "{\"device_id\":\"" + logs[i].device_id + "\",\"device_timestamp\":\"" + logs[i].device_timestamp + "\"}";
        if (i < logs.size() - 1) body += ",";
      }
      body += "]}";
      int code = http.POST(body);
      http.end();
      return (code == 200);
    }
    return false;
  }

  bool fetchConfig(const String& stationId, String& outStickerId) {
    if (!isConnected()) return false;
    WiFiClient client;
    HTTPClient http;
    if (http.begin(client, String(SERVER_URL) + "/api/stations/" + stationId + "/config")) {
      int code = http.GET();
      if (code == 200) {
        String res = http.getString();
        int idx = res.indexOf("\"distribute_sticker_id\":\"");
        if (idx != -1) {
          int start = idx + 25;
          int end = res.indexOf("\"", start);
          outStickerId = res.substring(start, end);
        }
        http.end();
        return true;
      }
      http.end();
    }
    return false;
  }
};

typedef void (*PacketCallback)(const CommunicationPacket& packet);

class EspNowManager {
private:
  static PacketCallback onRecvCallback;
  static void internalRecvCb(const uint8_t *mac, const uint8_t *data, int len) {
    if (len == sizeof(CommunicationPacket) && onRecvCallback) {
      CommunicationPacket packet;
      memcpy(&packet, data, sizeof(packet));
      onRecvCallback(packet);
    }
  }

public:
  void init(PacketCallback cb) {
    onRecvCallback = cb;
    if (esp_now_init() == ESP_OK) {
      esp_now_register_recv_cb(internalRecvCb);
    }
  }
};
PacketCallback EspNowManager::onRecvCallback = nullptr;

// ==========================================
// 4. アプリケーション層 (Application Layer)
// ==========================================
class StateManager {
private:
  SystemState currentState = STATE_IDLE;
  unsigned long stateTimer = 0;

public:
  SystemState getState() const { return currentState; }

  void setState(SystemState newState) {
    currentState = newState;
    stateTimer = millis();
  }

  void toggleSetting() {
    currentState = (currentState == STATE_SHOW_SETTING) ? STATE_IDLE : STATE_SHOW_SETTING;
  }

  bool checkAutoReturnToIdle(unsigned long timeoutMs = 5000) {
    if (currentState == STATE_STICKER_DISPLAY ||
        currentState == STATE_SOS_ALERT ||
        currentState == STATE_DUMMY_SOS_ALERT) {
      if (millis() - stateTimer > timeoutMs) {
        currentState = STATE_IDLE;
        return true;
      }
    }
    return false;
  }
};

class DisplayManager {
public:
  void init() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
  }

  // メモリ不足を防ぐため直接 LCD に描画
  void render(SystemState state, const String& stationId, const String& stickerId,
              int pendingDist, int pendingSos, const String& lastSync, const String& wifiSSID) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 10);

    switch (state) {
      case STATE_IDLE:
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.println("=== Station Mode ===");
        M5.Lcd.printf("ID: %s\n", stationId.c_str());
        M5.Lcd.printf("WiFi: %s\n\n", wifiSSID.c_str());
        M5.Lcd.println("Distribute Sticker:");
        M5.Lcd.printf("ID: %s\n", stickerId.c_str());
        break;

      case STATE_STICKER_DISPLAY:
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.println("=== Distributed ===");
        M5.Lcd.println();
        M5.Lcd.println("Sticker Sent!");
        M5.Lcd.printf("ID: %s\n", stickerId.c_str());
        break;

      case STATE_SOS_ALERT:
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.println("!! SOS ALERT !!");
        M5.Lcd.println();
        M5.Lcd.println("SOS Triggered!");
        break;

      case STATE_DUMMY_SOS_ALERT:
        M5.Lcd.setTextColor(YELLOW, BLACK);
        M5.Lcd.println("-- DUMMY SOS --");
        M5.Lcd.println();
        M5.Lcd.println("Test Signal Recv");
        break;

      case STATE_SHOW_SETTING:
        M5.Lcd.setTextColor(CYAN, BLACK);
        M5.Lcd.println("=== Settings ===");
        M5.Lcd.printf("Station: %s\n", stationId.c_str());
        M5.Lcd.printf("WiFi   : %s\n", wifiSSID.c_str());
        M5.Lcd.printf("Sticker: %s\n", stickerId.c_str());
        M5.Lcd.printf("Pending Dist: %d\n", pendingDist);
        M5.Lcd.printf("Pending SOS : %d\n", pendingSos);
        M5.Lcd.printf("Last Sync   : %s\n", lastSync.c_str());
        break;
    }
  }
};

class SettingManager {
public:
  String deviceId = "STATION_001";
  String distributeStickerId = "none";
  String lastSyncTime = "未同期";

  void syncConfig(WifiManager& wifi) {
    String newStickerId;
    if (wifi.fetchConfig(deviceId, newStickerId)) {
      distributeStickerId = newStickerId;
      lastSyncTime = "Config OK (" + wifi.getTimestamp() + ")";
    }
  }
};

class StickerManager {
private:
  std::vector<DistributeLog> pendingLogs;
  std::vector<String> distributedTodayList;

public:
  int getPendingCount() const { return pendingLogs.size(); }

  void handlePassDetection(const String& childId, int rssi, WifiManager& wifi,
                           SettingManager& settings, StateManager& stateMgr) {
    pendingLogs.push_back({childId, wifi.getTimestamp()});

    if (wifi.sendDistributeLogs(settings.deviceId, pendingLogs)) {
      pendingLogs.clear();
      settings.lastSyncTime = "OK (" + wifi.getTimestamp() + ")";
    }

    if (rssi >= RSSI_THRESHOLD) {
      auto it = std::find(distributedTodayList.begin(), distributedTodayList.end(), childId);
      if (it == distributedTodayList.end()) {
        distributedTodayList.push_back(childId);
        // TODO: ESP-NOWで子機へ distributeStickerId を送信
        stateMgr.setState(STATE_STICKER_DISPLAY);
      }
    }
  }
};

class SosManager {
private:
  std::vector<SosLog> pendingSosLogs;

public:
  int getPendingCount() const { return pendingSosLogs.size(); }

  void triggerSos(const String& childId, WifiManager& wifi, SettingManager& settings,
                  StateManager& stateMgr, LedManager& led, BuzzerManager& buzzer) {
    pendingSosLogs.push_back({childId, wifi.getTimestamp()});

    led.alertSOS();
    buzzer.alertSOS();
    stateMgr.setState(STATE_SOS_ALERT);

    if (wifi.sendSosLogs(settings.deviceId, pendingSosLogs)) {
      pendingSosLogs.clear();
    }
  }
};

// ==========================================
// 5. グローバルインスタンス & メインルーチン
// ==========================================
ButtonManager   btnMgr;
BuzzerManager   buzzerMgr;
LedManager      ledMgr;
DisplayManager  dispMgr;
WifiManager     wifiMgr;
EspNowManager   espNowMgr;
StateManager    stateMgr;
SettingManager  settingMgr;
StickerManager  stickerMgr;
SosManager      sosMgr;

void refreshScreen() {
  dispMgr.render(stateMgr.getState(), settingMgr.deviceId, settingMgr.distributeStickerId,
                 stickerMgr.getPendingCount(), sosMgr.getPendingCount(), settingMgr.lastSyncTime,
                 wifiMgr.getConnectedSSID());
}

void onPacketReceived(const CommunicationPacket& packet) {
  String childId = String(packet.device_id);
  if (packet.type == 1) {
    sosMgr.triggerSos(childId, wifiMgr, settingMgr, stateMgr, ledMgr, buzzerMgr);
  } else if (packet.type == 0) {
    stickerMgr.handlePassDetection(childId, -50, wifiMgr, settingMgr, stateMgr);
  }
  refreshScreen();
}

void setup() {
  // LCD, SD, Serial, I2C を有効化
  M5.begin(true, false, true, true);

  dispMgr.init();
  ledMgr.init();
  buzzerMgr.init();

  M5.Lcd.println("Starting M5Station...");

  // 周辺WiFi自動スキャン・接続
  wifiMgr.autoConnect();
  espNowMgr.init(onPacketReceived);

  // サーバー設定の取得
  settingMgr.syncConfig(wifiMgr);
  stateMgr.setState(STATE_IDLE);

  refreshScreen();
}

void loop() {
  btnMgr.update();

  wifiMgr.maintainConnection();

  // Bボタン：設定画面の表示切替
  if (btnMgr.isSettingButtonPressed()) {
    stateMgr.toggleSetting();
    refreshScreen();
  }

  // 5秒経過で通常待機画面へ自動復帰
  if (stateMgr.checkAutoReturnToIdle(5000)) {
    ledMgr.clear();
    refreshScreen();
  }

  delay(10);
}