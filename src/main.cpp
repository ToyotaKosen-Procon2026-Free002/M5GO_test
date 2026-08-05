#include <M5Stack.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <vector>
#include <algorithm>

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
const char* ssid = "icewave-G";
const char* password = "20240831A#z";
const char* serverUrl = "https://192.168.10.10"; // UbuntuサーバーURL

bool sosSent = false;

// 近距離判定の電波強度閾値 (dBm)
const int RSSI_THRESHOLD = -60;

struct CommunicationPacket {
  char senderId[16];  // 子機ID
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
String currentSticker = "RareSticker_A"; // 現在のシール設定
String lastSyncTime   = "未同期";         // 最終同期時刻
std::vector<String> distributedList;    // 当日配布した子機ID（二重配布防止）
unsigned long stateTimer = 0;           // 画面切替タイマー

// =============
// 画面表示処理
// =============
void updateDisplay() {
  sprite.fillScreen(BLACK);
  sprite.setCursor(0, 0);
  
  switch (currentState) {
    case STATE_IDLE:
      sprite.setTextColor(WHITE);
      sprite.println("=== Normal Mode ===");
      sprite.println();
      sprite.println("Sticker:");
      sprite.println(currentSticker);
      break;

    case STATE_STICKER_DISPLAY:
      sprite.setTextColor(GREEN);
      sprite.println("=== Distributed ===");
      sprite.println();
      sprite.println("Sticker Sent!");
      break;

    case STATE_SOS_ALERT:
      sprite.setTextColor(RED);
      sprite.println("!! SOS ALERT !!");
      sprite.println();
      sprite.println("Distance: ~?m");
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
      sprite.println();
      sprite.print("Sync: "); sprite.println(lastSyncTime);
      sprite.print("Item: "); sprite.println(currentSticker);
      break;
  }
  
  sprite.pushSprite(0, 0);
}

// =================
// サーバー通信機能
// =================
// ① 通過ログの送信 (Ubuntuへ)
void sendPassLog(String childId) {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  HTTPClient https;
  client.setInsecure();

  if (https.begin(client, String(serverUrl) + "/api/pass_log")) {
    https.addHeader("Content-Type", "application/json");
    String body = "{\"childId\":\"" + childId + "\",\"parentId\":\"PARENT_01\"}";
    https.POST(body);
    https.end();
  }
}

// ② SOSの検証 (サーバーへ問い合わせ)
void processSosSignal(String childId) {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  HTTPClient https;
  client.setInsecure();

  // LEDを赤く光らせる
  pixels.fill(pixels.Color(255, 0, 0), 0, NUM_LED);
  pixels.show();

  if (https.begin(client, String(serverUrl) + "/api/verify_sos?id=" + childId)) {
    int code = https.GET();
    String response = https.getString();

    // 本物・ダミーの検証判定
    if (code == 200 && response.indexOf("REAL") != -1) {
      currentState = STATE_SOS_ALERT;
      M5.Speaker.tone(1000, 500); // 警告ブザーを鳴らす
    } else {
      currentState = STATE_DUMMY_SOS_ALERT;
    }
    https.end();
  } else {
    // サーバーへ繋がらない場合は安全のため本物警告扱い
    currentState = STATE_SOS_ALERT;
    M5.Speaker.tone(1000, 500);
  }
  
  stateTimer = millis();
  updateDisplay();
}

// ③ サーバーからの設定同期
void syncSettingsWithServer() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  HTTPClient https;
  client.setInsecure();

  if (https.begin(client, String(serverUrl) + "/api/get_setting")) {
    int code = https.GET();
    if (code == 200) {
      String newSticker = https.getString();
      if (newSticker.length() > 0) {
        currentSticker = newSticker;
      }
      lastSyncTime = "12:00"; // 簡易同期時刻表記
    }
    https.end();
  }
}

// ==================
// 通信 & ロジック
// ==================
// 子機検知＆シール配布チェック
void checkAndSendSticker(String childId, int rssi) {
  sendPassLog(childId); // 通過ログ作成＆送信

  // 近距離判定
  if (rssi >= RSSI_THRESHOLD) {
    // 重複配布チェック (二重配布防止)
    auto it = std::find(distributedList.begin(), distributedList.end(), childId);
    if (it == distributedList.end()) {
      distributedList.push_back(childId); // 配布済みリストに追加

      // TODO: ESP-NOWで子機へcurrentStickerを送信する処理

      currentState = STATE_STICKER_DISPLAY;
      stateTimer = millis();
      updateDisplay();
    }
  }
}

// ESP-NOW パケット受信コールバック
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  CommunicationPacket packet;
  memcpy(&packet, incomingData, sizeof(packet));
  String childId = String(packet.senderId);

  if (packet.type == 1) {
    processSosSignal(childId); // SOS受信
  } else if (packet.type == 0) {
    checkAndSendSticker(childId, -50); // 通過検知 (ダミーRSSI: -50)
  }
}

// ================
// setup & loop
// ================
void setup() {
  // 初期化処理
  M5.begin();

  // スプライトの初期化
  sprite.setColorDepth(8);
  sprite.setTextSize(2);
  sprite.createSprite(M5.Lcd.width(), M5.Lcd.height());

  // Wi-Fi接続
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // ESP-NOW の初期化
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
  }

  // LED初期化
  pixels.fill(pixels.Color(0, 0, 0, 0), 0, NUM_LED);
  pixels.show();

  // 初回設定同期 ＆ 画面描画
  syncSettingsWithServer();
  currentState = STATE_IDLE;
  updateDisplay();
}

void loop() {
  M5.update();

  // --- Button Manager ---
  // 真ん中のボタン（BtnB）を押したとき設定表示
  if (M5.BtnB.wasPressed()) {
    if (currentState != STATE_SHOW_SETTING) {
      currentState = STATE_SHOW_SETTING;
    } else {
      currentState = STATE_IDLE;
    }
    updateDisplay();
  }

  // Aボタンを押したとき（テスト用SOS自発送信）
  if (M5.BtnA.wasPressed()) {
    processSosSignal("SELF_TEST");
  }

  // --- 表示時間タイマー管理 ---
  // SOS警告やシール配布完了の画面は5秒経過で通常画面に戻し、LEDを消灯する
  if (currentState == STATE_STICKER_DISPLAY || 
      currentState == STATE_SOS_ALERT || 
      currentState == STATE_DUMMY_SOS_ALERT) {
    
    if (millis() - stateTimer > 5000) {
      pixels.fill(pixels.Color(0, 0, 0, 0), 0, NUM_LED); // LED消灯
      pixels.show();
      currentState = STATE_IDLE;
      updateDisplay();
    }
  }

  delay(10);
}