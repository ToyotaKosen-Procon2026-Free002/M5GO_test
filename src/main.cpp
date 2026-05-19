#include <M5Stack.h>
#include <Adafruit_NeoPixel.h>
#include <NimBLEDevice.h>

// LEDバーのピン番号とLEDの数の定義
#define LED_BAR_PIN 15
#define NUM_LED 10

// BluetoothのサービスUUIDの定義（その他のBluetooth通信と区別するため）
#define SERVICE_UUID "42fbd1f2-b02c-1ba6-87f8-7d9ca4f3a343"

// RSSIの閾値（この値以上のRSSIを近いとみなす）
#define NEARBY_THRESHOLD -40

// NeoPixel（LED制御）のインスタンスを作成
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LED, LED_BAR_PIN, NEO_GRB + NEO_KHZ800);

// Bluetoothの広告とスキャンのインスタンス
NimBLEAdvertising *pAdvertising;
NimBLEScan *pScan;

// 画面ちらつき防止でSpriteを使用
TFT_eSprite sprite = TFT_eSprite(&M5.Lcd);


// Bluetoothのスキャンのコールバッククラス
class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *device) override {
    if (device->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) {
      int rssi = device->getRSSI();
      std::string addr = device->getAddress().toString();

      sprite.fillScreen(BLACK);
      sprite.setCursor(0, 0);
      sprite.printf("Device:\n %s\n", addr.c_str());
      sprite.printf("RSSI:\n %d dBm\n", rssi);
      sprite.pushSprite(0, 0);

      if (rssi > NEARBY_THRESHOLD) { // RSSI（信号強度）が閾値以上なら近いとみなす
        pixels.fill(pixels.Color(0, 255, 0)); // 緑色で点灯
        pixels.show();
      } else {
        pixels.fill(pixels.Color(0, 0, 0)); // 消灯
        pixels.show();
      }
    }
  }
};

void setup() {
  // 初期化処理
  M5.begin();

  // メモリ競合を防止するため、Classic Bluetoothのメモリを解放
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  // Bluetoothの初期化
  NimBLEDevice::init("ESP_NODE");

  // Bluetoothの広告の設定
  pAdvertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData;
  advData.setName("ESP_NODE");
  advData.addServiceUUID(SERVICE_UUID);
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();

  // Bluetoothのスキャンの設定
  pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(100);
  pScan->setScanCallbacks(new ScanCallbacks(), true);
  pScan->start(0, false, true);

  // スプライトの初期化
  sprite.setColorDepth(8);
  sprite.setTextSize(2);
  sprite.createSprite(M5.Lcd.width(), M5.Lcd.height());
}

void loop() {
}