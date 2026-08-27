#include <M5Unified.h>
#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <SPI.h>
#include <set>
#include <string>

// ======================= 設定・定数 =======================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASS     = "YOUR_WIFI_PASS";
const char* SERVER_LOG_URL = "http://192.168.1.100:8000/api/log";      // UbuntuサーバーAPI
const char* SERVER_SOS_URL = "http://192.168.1.100:8000/api/sos";

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// パケットタイプ定義
enum PacketType : uint8_t {
    PKT_DISCOVERY    = 0x01,  // 子機検知
    PKT_STICKER_OFFER = 0x02,  // シール配布
    PKT_SOS_ALERT    = 0xFF   // SOS
};

// ESP-NOW パケット構造体
typedef struct {
    uint8_t packetType;       // PacketType
    char senderId[16];        // 送信元デバイスID (MACや独自ID)
    uint16_t stickerId;       // レアシールID
    uint8_t dummyFlag;        // 0: 本物, 1: ダミー
} __attribute__((packed)) MessagePacket;

// 親機の状態
enum State {
    STATE_IDLE,
    STATE_STICKER_DISPLAY,
    STATE_SOS_ALERT,
    STATE_SHOW_SETTING
};

// ======================= グローバル変数 =======================
State currentState = STATE_IDLE;
uint32_t stateTimer = 0;

// 親機設定情報（BLE/アプリから設定可能）
String parentName = "三角公園前 親機";
uint16_t currentSpecialStickerId = 101; // 配布中レアシールID

// 二重配布防止キャッシュ（当日に配布した子機のMAC/ID一覧）
std::set<String> distributedList;

// BLE通信用
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool bleConnected = false;

// ======================= 画面・LED・ブザー制御 =======================
void updateDisplay() {
    M5.Display.clear();
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 10);

    switch (currentState) {
        case STATE_IDLE:
            M5.Display.setTextColor(WHITE, BLACK);
            M5.Display.printf("=== %s ===\n\n", parentName.c_str());
            M5.Display.printf("Now Distributing:\n");
            M5.Display.setTextColor(YELLOW, BLACK);
            M5.Display.printf(" [Rare Sticker #%03d]\n\n", currentSpecialStickerId);
            M5.Display.setTextColor(LIGHTGREY, BLACK);
            M5.Display.printf("BLE Status: %s\n", bleConnected ? "Connected" : "Advertising");
            break;

        case STATE_STICKER_DISPLAY:
            M5.Display.setTextColor(GREEN, BLACK);
            M5.Display.printf("\n  >> STICKER SENT! <<\n\n");
            M5.Display.setTextColor(WHITE, BLACK);
            M5.Display.printf("Rare Sticker #%03d\nGiven to child device!", currentSpecialStickerId);
            break;

        case STATE_SOS_ALERT:
            M5.Display.fillScreen(RED);
            M5.Display.setTextColor(WHITE, RED);
            M5.Display.setTextSize(3);
            M5.Display.setCursor(20, 40);
            M5.Display.printf("!! SOS ALERT !!\n\n");
            M5.Display.setTextSize(2);
            M5.Display.printf("Child pressed SOS!\nRelaying to server...");
            break;

        case STATE_SHOW_SETTING:
            M5.Display.setTextColor(CYAN, BLACK);
            M5.Display.printf("=== Device Settings ===\n");
            M5.Display.setTextColor(WHITE, BLACK);
            M5.Display.printf("Name: %s\n", parentName.c_str());
            M5.Display.printf("Sticker ID: %d\n", currentSpecialStickerId);
            M5.Display.printf("Today Count: %d\n", distributedList.size());
            M5.Display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
            break;
    }
}

// ======================= サーバー通信 (Wi-Fi経由) =======================
void sendLogToServer(String childId, uint16_t stickerId) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(SERVER_LOG_URL);
        http.addHeader("Content-Type", "application/json");
        String payload = "{\"parent_name\":\"" + parentName + "\",\"child_id\":\"" + childId + "\",\"sticker_id\":" + String(stickerId) + "}";
        int httpCode = http.POST(payload);
        http.end();
    }
}

void sendSosToServer(String childId) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(SERVER_SOS_URL);
        http.addHeader("Content-Type", "application/json");
        String payload = "{\"parent_name\":\"" + parentName + "\",\"child_id\":\"" + childId + "\",\"status\":\"EMERGENCY\"}";
        int httpCode = http.POST(payload);
        http.end();
    }
}

// ======================= BLE (アプリ設定通信) =======================
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleConnected = true;
    };
    void onDisconnect(BLEServer* pServer) {
        bleConnected = false;
        BLEDevice::startAdvertising();
    }
};

class SettingCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            // STICKER_ID または NAME
            String cmd = String(value.c_str());
            if (cmd.startsWith("STICKER_ID:")) {
                currentSpecialStickerId = cmd.substring(11).toInt();
            } else if (cmd.startsWith("NAME:")) {
                parentName = cmd.substring(5);
            }
            currentState = STATE_IDLE;
            updateDisplay();
        }
    }
};

void setupBLE() {
    BLEDevice::init("CocoSeal-Parent");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ   |
                        BLECharacteristic::PROPERTY_WRITE  |
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
    pCharacteristic->setCallbacks(new SettingCallbacks());
    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
}

// ======================= ESP-NOW (子機との通信) =======================
void onEspNowReceive(const uint8_t *mac_addr, const uint8_t *data, int len) {
    if (len < sizeof(MessagePacket)) return;

    MessagePacket pkt;
    memcpy(&pkt, data, sizeof(MessagePacket));
    String childId = String(pkt.senderId);

    // 1. SOS受信時
    if (pkt.packetType == PKT_SOS_ALERT) {
        currentState = STATE_SOS_ALERT;
        stateTimer = millis();
        M5.Speaker.tone(2000, 1000); // 警告音
        sendSosToServer(childId);
        updateDisplay();
        return;
    }

    // 2. 子機検知・すれ違いシール配布
    if (pkt.packetType == PKT_DISCOVERY) {
        // 重複配布チェック (1日1回)
        if (distributedList.find(childId) == distributedList.end()) {
            distributedList.insert(childId);

            // シール配布パケット作成 & 送信
            MessagePacket reply;
            reply.packetType = PKT_STICKER_OFFER;
            strncpy(reply.senderId, "PARENT_01", sizeof(reply.senderId));
            reply.stickerId = currentSpecialStickerId;
            reply.dummyFlag = 0;

            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, mac_addr, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            if (!esp_now_is_peer_exist(mac_addr)) {
                esp_now_add_peer(&peerInfo);
            }
            esp_now_send(mac_addr, (uint8_t*)&reply, sizeof(reply));

            // 通過ログ送信
            sendLogToServer(childId, currentSpecialStickerId);

            // 画面表示更新
            currentState = STATE_STICKER_DISPLAY;
            stateTimer = millis();
            updateDisplay();
        }
    }
}

// ======================= 初期設定 & ループ =======================
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);

    // Wi-Fi 接続
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // ESP-NOW 初期化
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(onEspNowReceive);
    }

    // BLE 初期化
    setupBLE();

    // 画面初期表示
    currentState = STATE_IDLE;
    updateDisplay();
}

void loop() {
    M5.update();

    // 中央ボタン(BtnB)押下で設定・ステータス確認画面切り替え
    if (M5.BtnB.wasPressed()) {
        if (currentState == STATE_SHOW_SETTING) {
            currentState = STATE_IDLE;
        } else {
            currentState = STATE_SHOW_SETTING;
        }
        updateDisplay();
    }

    // STICKER_DISPLAY から約10秒で IDLE に戻る
    if (currentState == STATE_STICKER_DISPLAY && millis() - stateTimer > 10000) {
        currentState = STATE_IDLE;
        updateDisplay();
    }

    // SOS_ALERT から約2分で通常画面に戻る
    if (currentState == STATE_SOS_ALERT && millis() - stateTimer > 120000) {
        currentState = STATE_IDLE;
        updateDisplay();
    }

    delay(20);
}