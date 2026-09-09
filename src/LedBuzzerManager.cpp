#include "LedBuzzerManager.h"
#include <M5Stack.h>

LedBuzzerManager ledBuzzerMgr;

LedBuzzerManager::LedBuzzerManager() {}

void LedBuzzerManager::init() {
  pixels.updateType(NEO_GRB + NEO_KHZ800);
  pixels.updateLength(NUM_LED);
  pixels.setPin(LED_BAR_PIN);
  pixels.begin();
  pixels.clear();
  pixels.show();
}

void LedBuzzerManager::showIdle() {
  pixels.clear();
  pixels.show();
}

void LedBuzzerManager::showRealSos() {
  pixels.fill(pixels.Color(255, 0, 0), 0, NUM_LED);
  pixels.show();
  M5.Speaker.tone(1000, 1000);
}

void LedBuzzerManager::showDummySos() {
  pixels.fill(pixels.Color(255, 140, 0), 0, NUM_LED);
  pixels.show();
}

void LedBuzzerManager::clear() {
  pixels.clear();
  pixels.show();
}