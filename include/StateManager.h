#pragma once
#include "Config.h"

class StateManager {
public:
  static State currentState;
  static unsigned long stateTimer;

  static void changeState(State newState);
  static void checkStateTimeout(unsigned long timeoutMs = 5000);
};