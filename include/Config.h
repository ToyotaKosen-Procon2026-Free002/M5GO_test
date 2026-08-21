#pragma once
#include <Arduino.h>

#define LED_BAR_PIN 15
#define NUM_LED 10
#define RSSI_THRESHOLD -60
#define CONFIG_FETCH_INTERVAL 30000 // 30秒

const char SERVER_URL[] = "http://157.17.49.151";
const char NTP_SERVER[] = "ntp.nict.jp";
const long GMT_OFFSET_SEC = 9 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

enum State {
  STATE_IDLE,
  STATE_STICKER_DISPLAY,
  STATE_SOS_ALERT,
  STATE_DUMMY_SOS_ALERT,
  STATE_SHOW_SETTING,
  STATE_WIFI_CONFIG
};

struct CommunicationPacket {
  char device_id[16];
  int type; // 0: 通過/シール要求, 1: SOS
  char stickerId[16];
};

struct DistributeLog {
  String child_device_id;
  String device_timestamp;
};

struct SosLog {
  String child_device_id;
  String device_timestamp;
};