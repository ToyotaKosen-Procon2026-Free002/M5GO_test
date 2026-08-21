#pragma once
#include <esp_now.h>
#include "Config.h"

class EspNowManager {
public:
  static void init();
  static void sendSticker(const String& stationId, const String& stickerId);
  static void onDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len);
};