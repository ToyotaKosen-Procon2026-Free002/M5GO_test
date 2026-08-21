#pragma once
#include <Adafruit_NeoPixel.h>
#include "Config.h"

class LedBuzzerManager {
private:
  Adafruit_NeoPixel pixels;

public:
  LedBuzzerManager();
  void init();
  void showIdle();
  void showRealSos();
  void showDummySos();
  void clear();
};

extern LedBuzzerManager ledBuzzerMgr;