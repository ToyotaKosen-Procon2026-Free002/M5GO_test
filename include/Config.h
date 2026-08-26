#pragma once
#include <Arduino.h>

// ハードウェア設定
#define LED_BAR_PIN 15
#define NUM_LED 10
#define RSSI_THRESHOLD -60

// 親機 ⇔ アプリ間 BLE GATT UUID
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_CONFIG_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_LOG_UUID          "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"
#define CHAR_STATUS_UUID       "d29ae63e-b7d3-4874-a690-3432b85e05a5"

enum State {
  STATE_IDLE,
  STATE_STICKER_DISPLAY,
  STATE_SOS_ALERT,
  STATE_DUMMY_SOS_ALERT,
  STATE_SHOW_SETTING,
  STATE_BLE_CONNECTED
};

// 子機 ⇔ 親機 ESP-NOW パケット
struct CommunicationPacket {
  char device_id[16];
  int type; // 0: 通過/シール要求, 1: SOS
  char stickerId[16];
};

struct DistributeLog {
  String device_id_2;      // すれ違った子機ID
  String device_timestamp; // 記録時刻
};

struct SosLog {
  String child_id;         // SOS発信子機ID
  String device_timestamp; // 記録時刻
};