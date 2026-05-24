#include <M5Stack.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

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
const char* ssid = "";
const char* password = "";

bool sosSent = false;

void sendSOS() {
  WiFiClientSecure client;
  HTTPClient https;

  client.setInsecure();

  pixels.fill(pixels.Color(255, 0, 0), 0, NUM_LED);
  pixels.show();

  bool ok = https.begin(client, "https://192.168.10.10/sos");
  https.addHeader("Content-Type", "application/json");

  String mac = WiFi.macAddress();
  String body = "{\"mac\":\"" + mac + "\"}";
  
  int code = https.POST(body);
  String response = https.getString();
  
  sprite.fillScreen(BLACK);
  sprite.setCursor(0, 0);
  sprite.println(code);
  sprite.println(ok);
  sprite.println(response);
  sprite.pushSprite(0, 0);

  https.end();
}

void setup() {
  // 初期化処理
  M5.begin();

  // スプライトの初期化
  sprite.setColorDepth(8);
  sprite.setTextSize(2);
  sprite.createSprite(M5.Lcd.width(), M5.Lcd.height());

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  sprite.println("Activated");
  sprite.pushSprite(0, 0);

  // LED初期化
  pixels.fill(pixels.Color(0, 0, 0, 0), 0, NUM_LED);
  pixels.show();
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed() && !sosSent) {
    sendSOS();
    sosSent = true;
  }
}