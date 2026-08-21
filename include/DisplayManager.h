#pragma once
#include <M5Stack.h>
#include "Config.h"

class DisplayManager {
private:
  TFT_eSprite sprite;

public:
  DisplayManager();
  void init();
  void update();
};

extern DisplayManager displayMgr;