#include <M5Stack.h>
#include <WiFi.h>
#include <esp_now.h>
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>
#include <vector>
#include <algorithm>

// ハードウェアピン
#define LED_BAR_PIN 15
#define NUM_LED 10
#define RSSI_THRESHOLD -60

// BLE UUID
#define SERVICE_UUID     "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_CONFIG_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_LOG_UUID    "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"
#define CHAR_STATUS_UUID "d29ae63e-b7d3-4874-a690-3432b85e05a5"

enum State {
  STATE_IDLE,
  STATE_STICKER_DISPLAY,
  STATE_SOS_ALERT,
  STATE_SHOW_SETTING,
  STATE_BLE_CONNECTED
};

State currentState = STATE_IDLE;
unsigned long stateTimer = 0;
String deviceId = "M5-INIT";
String distributeStickerId = "st_110";
String spotName = "Unregistered";
String lastSyncTime = "None";

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LED, LED_BAR_PIN, NEO_GRB + NEO_KHZ800);

NimBLECharacteristic* pLogChar = nullptr;
NimBLECharacteristic* pStatusChar = nullptr;
bool deviceConnected = false;

struct CommunicationPacket {
  char device_id[16];
  int type; // 0: 通過/シール, 1: SOS
  char stickerId[16];
};

struct DistributeLog {
  String device_id_2;
  String device_timestamp;
};

struct SosLog {
  String child_id;
  String device_timestamp;
};

std::vector<DistributeLog> pendingDistributeLogs;
std::vector<SosLog>        pendingSosLogs;
std::vector<String>        distributedTodayList;

String getTimestamp() {
  unsigned long sec = millis() / 1000;
  char buf[20];
  snprintf(buf, sizeof(buf), "+%lu sec", sec);
  return String(buf);
}

// 液晶への直接描画
void updateDisplay() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.setTextSize(2);

  switch (currentState) {
    case STATE_BLE_CONNECTED:
      M5.Lcd.setTextColor(CYAN, BLACK);
      M5.Lcd.println("=== iPad Connected ===");
      M5.Lcd.println("\nSyncing with App...");
      break;

    case STATE_IDLE:
      M5.Lcd.setTextColor(WHITE, BLACK);
      M5.Lcd.println("=== Station Mode ===");
      M5.Lcd.printf("Spot: %s\n", spotName.c_str());
      M5.Lcd.printf("ID  : %s\n\n", deviceId.c_str());
      M5.Lcd.setTextColor(YELLOW, BLACK);
      M5.Lcd.println("[Distribute Sticker]");
      M5.Lcd.println(distributeStickerId);
      break;

    case STATE_STICKER_DISPLAY:
      M5.Lcd.setTextColor(GREEN, BLACK);
      M5.Lcd.println("=== Distributed ===");
      M5.Lcd.println("\nSticker Sent!");
      M5.Lcd.printf("ID: %s\n", distributeStickerId.c_str());
      break;

    case STATE_SOS_ALERT:
      M5.Lcd.setTextColor(RED, BLACK);
      M5.Lcd.println("!! SOS ALERT !!");
      M5.Lcd.println("\nSOS Signal Recv!");
      break;

    case STATE_SHOW_SETTING:
      M5.Lcd.setTextColor(CYAN, BLACK);
      M5.Lcd.println("=== Settings ===");
      M5.Lcd.printf("ID     : %s\n", deviceId.c_str());
      M5.Lcd.printf("Spot   : %s\n", spotName.c_str());
      M5.Lcd.printf("Sticker: %s\n", distributeStickerId.c_str());
      M5.Lcd.printf("Logs   : D:%d / S:%d\n", (int)pendingDistributeLogs.size(), (int)pendingSosLogs.size());
      M5.Lcd.printf("Sync   : %s\n", lastSyncTime.c_str());
      break;
  }
}

void flushLogsToBle() {
  if (!pLogChar || !deviceConnected) return;

  String json = "{\"station_id\":\"" + deviceId + "\",";
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

  pLogChar->setValue((uint8_t*)json.c_str(), json.length());
  pLogChar->notify();

  pendingDistributeLogs.clear();
  pendingSosLogs.clear();
}

// BLEコールバック
class ServerCallbacks: public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    currentState = STATE_BLE_CONNECTED;
    updateDisplay();
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    currentState = STATE_IDLE;
    updateDisplay();
    NimBLEDevice::startAdvertising();
  }
};

class CharCallbacks: public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    std::string val = pCharacteristic->getValue();
    if (val.length() > 0) {
      String payload = String(val.c_str());
      if (payload.startsWith("STICKER:")) {
        distributeStickerId = payload.substring(8);
      } else if (payload.startsWith("SPOT:")) {
        spotName = payload.substring(5);
      } else if (payload == "GET_LOGS") {
        flushLogsToBle();
      }
      lastSyncTime = "App Synced";
      currentState = STATE_IDLE;
      updateDisplay();
    }
  }
};

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  CommunicationPacket packet;
  if (len == sizeof(packet)) {
    memcpy(&packet, incomingData, sizeof(packet));
    String childId = String(packet.device_id);
    String timestamp = getTimestamp();

    if (packet.type == 1) { // SOS
      pendingSosLogs.push_back({childId, timestamp});
      currentState = STATE_SOS_ALERT;
      stateTimer = millis();

      pixels.fill(pixels.Color(255, 0, 0), 0, NUM_LED);
      pixels.show();
      updateDisplay();
    } else if (packet.type == 0) { // シール
      pendingDistributeLogs.push_back({childId, timestamp});

      auto it = std::find(distributedTodayList.begin(), distributedTodayList.end(), childId);
      if (it == distributedTodayList.end()) {
        distributedTodayList.push_back(childId);

        CommunicationPacket reply;
        strncpy(reply.device_id, deviceId.c_str(), sizeof(reply.device_id) - 1);
        reply.type = 0;
        strncpy(reply.stickerId, distributeStickerId.c_str(), sizeof(reply.stickerId) - 1);

        uint8_t bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, bcast, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        if (!esp_now_is_peer_exist(bcast)) {
          esp_now_add_peer(&peerInfo);
        }
        esp_now_send(bcast, (uint8_t*)&reply, sizeof(reply));

        currentState = STATE_STICKER_DISPLAY;
        stateTimer = millis();
        updateDisplay();
      }
    }
  }
}

void setup() {
  // ① M5Stackハード初期化
  M5.begin(true, false, true, false);
  M5.Lcd.setBrightness(100);

  // ② LED初期化
  pixels.begin();
  pixels.clear();
  pixels.show();

  // ③ WiFi STA & ID自動生成
  WiFi.mode(WIFI_STA);
  delay(50);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char idBuf[16];
  snprintf(idBuf, sizeof(idBuf), "M5-%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);
  deviceId = String(idBuf);

  // ④ ESP-NOW初期化
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
  }

  // ⑤ 初回画面表示
  currentState = STATE_IDLE;
  updateDisplay();

  // ⑥ BLE初期化
  String bleDeviceName = "COCO-" + deviceId;
  NimBLEDevice::init(bleDeviceName.c_str());

  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  NimBLECharacteristic* pConfigChar = pService->createCharacteristic(
    CHAR_CONFIG_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
  );
  pConfigChar->setCallbacks(new CharCallbacks());

  pLogChar = pService->createCharacteristic(
    CHAR_LOG_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  pStatusChar = pService->createCharacteristic(
    CHAR_STATUS_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  NimBLEDevice::startAdvertising();
}

void loop() {
  M5.update();

  // ボタンB：設定・蓄積ログ画面切り替え
  if (M5.BtnB.wasPressed()) {
    currentState = (currentState != STATE_SHOW_SETTING) ? STATE_SHOW_SETTING : STATE_IDLE;
    updateDisplay();
  }

  // 自動復帰（5秒）
  if (currentState == STATE_STICKER_DISPLAY || currentState == STATE_SOS_ALERT) {
    if (millis() - stateTimer > 5000) {
      pixels.clear();
      pixels.show();
      currentState = STATE_IDLE;
      updateDisplay();
    }
  }

  delay(10);
}