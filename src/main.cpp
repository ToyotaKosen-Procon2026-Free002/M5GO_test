#include <M5Stack.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <vector>
#include <algorithm>


// ハードウェア設定
// LEDバーのピン番号とLEDの数の定義
#define LED_BAR_PIN 15
#define NUM_LED 10

// ボタンピン番号の定義
#define BUTTON_A_PIN 39
#define BUTTON_B_PIN 38
#define BUTTON_C_PIN 37

// NeoPixel（LED制御）のインスタンスを作成
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LED, LED_BAR_PIN, NEO_GRB + NEO_KHZ800);
// 画面ちらつき防止でSpriteを使用
TFT_eSprite sprite = TFT_eSprite(&M5.Lcd);

// WiFi設定の定義
const char* wifi_ssid = "icewave-G";
const char* wifi_password = "20240831A#z";
// coco-seal.mydns.jp
const char* serverUrl = "http://157.17.49.151";

// 日本標準時（JST = UTC+9）設定
const long  gmtOffset_sec     = 9 * 3600;
const int   daylightOffset_sec = 0;
const char* ntpServer         = "ntp.nict.jp";

// 親機固有情報
String device_id             = ""; // 親機ID
String distribute_sticker_id = "none";  // 配布するシールの種類ID
String last_sync_time        = "未同期";   // 最終同期時刻

// 未送信ログの構造体
struct DistributeLog {
  String device_id; // すれ違った子機のID
  String device_timestamp; // 記録時刻
};

struct SosLog {
  String device_id; // SOSを発信した子機のID
  String device_timestamp; // 記録時刻
};

std::vector<DistributeLog> pending_distribute_logs; // クラウド未送信のシール配布ログ
std::vector<SosLog>        pending_sos_logs;        // クラウド未送信の緊急SOSログ
std::vector<String>        distributed_today_list;  // 当日配布済み子機ID（重複防止用）

// ESP-NOW パケット構造体
struct CommunicationPacket {
  char device_id[16];  // 子機ID
  int type;           // 0: 通過検知, 1: SOS
  char stickerId[16]; // 配布シールID
};

enum State {
  STATE_IDLE,             // 通常待機
  STATE_STICKER_DISPLAY,  // SPシール配布完了表示
  STATE_SOS_ALERT,        // 子機SOS(本物)
  STATE_DUMMY_SOS_ALERT,  // 子機SOS(ダミー)
  STATE_SHOW_SETTING      // 設定表示
};

// システム管理用変数
State currentState = STATE_IDLE;
unsigned long stateTimer = 0;
const int RSSI_THRESHOLD = -60;

// ==========================================
// 時刻取得関数
// ==========================================
String getDeviceTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01 00:00:00"; 
  }
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

// =============
// 画面表示処理
// =============
void updateDisplay() {
  sprite.fillScreen(BLACK);
  sprite.setCursor(0, 0);
  
  switch (currentState) {
    case STATE_IDLE:
      sprite.setTextColor(WHITE);
      sprite.println("=== Station Mode ==="); 
      sprite.printf("ID: %s\n\n", device_id.c_str());
      sprite.println("Distribute Sticker:");
      sprite.printf("ID: %s\n", distribute_sticker_id.c_str());
      break;

    case STATE_STICKER_DISPLAY:
      sprite.setTextColor(GREEN);
      sprite.println("=== Distributed ===");
      sprite.println();
      sprite.println("Sticker Sent!");
      sprite.printf("ID: %s\n", distribute_sticker_id.c_str());
      break;

    case STATE_SOS_ALERT:
      sprite.setTextColor(RED);
      sprite.println("!! SOS ALERT !!");
      sprite.println();
      sprite.println("SOS Triggered!");
      break;

    case STATE_DUMMY_SOS_ALERT:
      sprite.setTextColor(YELLOW);
      sprite.println("-- DUMMY SOS --");
      sprite.println();
      sprite.println("Test Signal Recv");
      break;

    case STATE_SHOW_SETTING:
      sprite.setTextColor(CYAN);
      sprite.println("=== Settings ===");
      sprite.printf("Station: %s\n", device_id.c_str());
      sprite.printf("Sticker: %s\n", distribute_sticker_id.c_str());
      sprite.printf("Pending Dist: %d\n", pending_distribute_logs.size());
      sprite.printf("Pending SOS : %d\n", pending_sos_logs.size());
      sprite.printf("Last Sync   : %s\n", last_sync_time.c_str());
      break;
  }
  
  sprite.pushSprite(0, 0);
}

// =================
// サーバー通信機能
// =================
// ① 配布ログの送信
void syncDistributeLogs() {
  if (pending_distribute_logs.empty() || WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  if (http.begin(client, String(serverUrl) + "/api/stations/logs/distribute")) {
    http.addHeader("Content-Type", "application/json");

    // JSON
    String body = "{\"station_device_id\":\"" + device_id + "\",\"logs\":[";
    for (size_t i = 0; i < pending_distribute_logs.size(); i++) {
      body += "{\"device_id\":\"" + pending_distribute_logs[i].device_id + "\",";
      body += "\"device_timestamp\":\"" + pending_distribute_logs[i].device_timestamp + "\"}";
      if (i < pending_distribute_logs.size() - 1) body += ",";
    }
    body += "]}";

    int httpCode = http.POST(body);
    if (httpCode == 200) {
      pending_distribute_logs.clear(); // 送信成功したらキューをクリア
      last_sync_time = "OK (" + getDeviceTimestamp() + ")";
    }
    http.end();
  }
}

// ② SOSログの送信
void syncSosLogs(String child_device_id) {
  // SOSログを未送信キューに追加
  pending_sos_logs.push_back({child_device_id, getDeviceTimestamp()});

  // LED・ブザー警告
  pixels.fill(pixels.Color(255, 0, 0), 0, NUM_LED);
  pixels.show();
  currentState = STATE_SOS_ALERT;
  M5.Speaker.tone(1000, 500);

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    if (http.begin(client, String(serverUrl) + "/api/stations/logs/sos")) {
      http.addHeader("Content-Type", "application/json");

      String body = "{\"station_device_id\":\"" + device_id + "\",\"logs\":[";
      for (size_t i = 0; i < pending_sos_logs.size(); i++) {
        body += "{\"device_id\":\"" + pending_sos_logs[i].device_id + "\",";
        body += "\"device_timestamp\":\"" + pending_sos_logs[i].device_timestamp + "\"}";
        if (i < pending_sos_logs.size() - 1) body += ",";
      }
      body += "]}";

      int httpCode = http.POST(body);
      if (httpCode == 200) {
        pending_sos_logs.clear();
      }
      http.end();
    }
  }

  stateTimer = millis();
  updateDisplay();
}

// ③ 親機設定の同期
void fetchStationConfig() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient client;
  HTTPClient http;

  if (http.begin(client, String(serverUrl) + "/api/stations/" + device_id + "/config")) {
    int code = http.GET();
    if (code == 200) {
      String response = http.getString();
      // レスポンスから distribute_sticker_id を抽出
      int idx = response.indexOf("\"distribute_sticker_id\":\"");
      if (idx != -1) {
        int start = idx + 25;
        int end = response.indexOf("\"", start);
        distribute_sticker_id = response.substring(start, end);
      }
      last_sync_time = "Config OK";
    }
    http.end();
  }
}

// ==================
// すれ違い＆パケット受信
// ==================
void checkAndDistributeSticker(String child_device_id, int rssi) {
  // 配布ログをキューに追加
  pending_distribute_logs.push_back({child_device_id, getDeviceTimestamp()});
  syncDistributeLogs();

  if (rssi >= RSSI_THRESHOLD) {
    auto it = std::find(distributed_today_list.begin(), distributed_today_list.end(), child_device_id);
    if (it == distributed_today_list.end()) {
      distributed_today_list.push_back(child_device_id);

      // TODO: ESP-NOWで子機へ distribute_sticker_id を送信

      currentState = STATE_STICKER_DISPLAY;
      stateTimer = millis();
      updateDisplay();
    }
  }
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  CommunicationPacket packet;
  if (len == sizeof(packet)) {
    memcpy(&packet, incomingData, sizeof(packet));
    String child_device_id = String(packet.device_id);

    if (packet.type == 1) {
      syncSosLogs(child_device_id);
    } else if (packet.type == 0) {
      checkAndDistributeSticker(child_device_id, -50);
    }
  }
}

// ================
// setup & loop
// ================
void setup() {
  M5.begin();

  sprite.setColorDepth(8);
  sprite.setTextSize(2);
  sprite.createSprite(M5.Lcd.width(), M5.Lcd.height());

  pixels.begin();
  pixels.clear();
  pixels.show();

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
  }

  fetchStationConfig();
  currentState = STATE_IDLE;
  updateDisplay();
}

void loop() {
  M5.update();

  if (M5.BtnB.wasPressed()) {
    currentState = (currentState != STATE_SHOW_SETTING) ? STATE_SHOW_SETTING : STATE_IDLE;
    updateDisplay();
  }

  // ダミーSOS発信（テスト用）
  //if (M5.BtnA.wasPressed()) {
  //  syncSosLogs("ESP-TEST-0001");
  //}
 
  // 5秒経過で通常待機画面へ復帰
  if (currentState == STATE_STICKER_DISPLAY || 
      currentState == STATE_SOS_ALERT || 
      currentState == STATE_DUMMY_SOS_ALERT) {
    
    if (millis() - stateTimer > 5000) {
      pixels.clear();
      pixels.show();
      currentState = STATE_IDLE;
      updateDisplay();
    }
  }

  delay(10);
}