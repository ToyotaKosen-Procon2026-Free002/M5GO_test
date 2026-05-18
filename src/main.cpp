#include <M5Stack.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_now.h>

// ボタンのピン番号の定義
#define BUTTON_A_PIN 39
#define BUTTON_B_PIN 38
#define BUTTON_C_PIN 37

// LEDバーのピン番号とLEDの数の定義
#define LED_BAR_PIN 15
#define NUM_LED 10

// NeoPixel（LED制御）のインスタンスを作成
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LED, LED_BAR_PIN, NEO_GRB + NEO_KHZ800);

// 送信先のMACアドレス、ブロードキャストに設定
uint8_t address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

bool isButtonPressing = false;

// データを送った際のコールバック関数（ブロードキャストでは常に成功とみなされるらしい）
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.clear();
  M5.Lcd.printf("Last Packet Send Status:\n %s\n", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  M5.Lcd.printf("Sent to:\n %02X:%02X:%02X:%02X:%02X:%02X\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

// データを受け取った際のコールバック関数
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.clear();
  M5.Lcd.printf("Received data from:\n %02X:%02X:%02X:%02X:%02X:%02X\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  M5.Lcd.printf("Data:\n %.*s\n", data_len, data);
}

// データを送る関数
void sendData(const char *message) {
  esp_now_send(address, (uint8_t*)message, strlen(message));
}

void setup() {
  // 初期化処理
  M5.begin();
  M5.Lcd.setTextSize(2);

  pixels.begin();

  // ESP-NOWの初期化
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // 通信の情報を設定
  memcpy(peerInfo.peer_addr, address, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // 送信と受信のコールバック関数を登録
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  M5.update();

  if (digitalRead(BUTTON_A_PIN) == LOW && !isButtonPressing) { // ボタンAが押されたとき「Button A Pressed」というメッセージを送り、LEDバーを赤色に光らせる
    sendData("Button A Pressed");
    pixels.fill(pixels.Color(255, 0, 0), 0, NUM_LED);
    pixels.show();
    isButtonPressing = true;
  } else if (digitalRead(BUTTON_B_PIN) == LOW && !isButtonPressing) { // ボタンBが押されたとき「Button B Pressed」というメッセージを送り、LEDバーを緑色に光らせる
    sendData("Button B Pressed");
    pixels.fill(pixels.Color(0, 255, 0), 0, NUM_LED);
    pixels.show();
    isButtonPressing = true;
  } else if (digitalRead(BUTTON_C_PIN) == LOW && !isButtonPressing) { // ボタンCが押されたとき「Button C Pressed」というメッセージを送り、LEDバーを青色に光らせる
    sendData("Button C Pressed");
    pixels.fill(pixels.Color(0, 0, 255), 0, NUM_LED);
    pixels.show();
    isButtonPressing = true;
  } else if (digitalRead(BUTTON_A_PIN) == HIGH && digitalRead(BUTTON_B_PIN) == HIGH && digitalRead(BUTTON_C_PIN) == HIGH) { // どのボタンも押されていなければLEDバーを消灯する
    isButtonPressing = false;
    pixels.fill(pixels.Color(0, 0, 0), 0, NUM_LED);
    pixels.show();
  }
}