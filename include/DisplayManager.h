#pragma once
#include <M5Stack.h>
#include "Config.h"

class DisplayManager {
private:
  TFT_eSprite* sprite = nullptr;

public:
  DisplayManager();
  bool init();
  void update();
};

extern DisplayManager displayMgr;