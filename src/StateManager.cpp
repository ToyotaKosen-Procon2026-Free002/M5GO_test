#include "StateManager.h"
#include "DisplayManager.h"
#include "LedBuzzerManager.h"

State StateManager::currentState = STATE_IDLE;
unsigned long StateManager::stateTimer = 0;

void StateManager::changeState(State newState) {
  currentState = newState;
  stateTimer = millis();
  displayMgr.update();
}

void StateManager::checkStateTimeout(unsigned long timeoutMs) {
  if (currentState == STATE_STICKER_DISPLAY || 
      currentState == STATE_SOS_ALERT || 
      currentState == STATE_DUMMY_SOS_ALERT) {
    if (millis() - stateTimer > timeoutMs) {
      ledBuzzerMgr.clear();
      changeState(STATE_IDLE);
    }
  }
}