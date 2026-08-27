#include <WiFi.h>
#include "BleManager.h"
#include "StateManager.h"
#include "StickerSosManager.h"

BleManager bleMgr;

void BleManager::init() {
  spotName = "Unregistered";
  distributeStickerId = "st_110";
  lastSyncTime = "None";

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char idBuf[16];
  snprintf(idBuf, sizeof(idBuf), "M5-%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);
  deviceId = String(idBuf);

  String bleDeviceName = "COCO-" + deviceId;
  NimBLEDevice::init(bleDeviceName.c_str());

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(this);

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  pConfigChar = pService->createCharacteristic(
    CHAR_CONFIG_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
  );
  pConfigChar->setCallbacks(this);

  pLogChar = pService->createCharacteristic(
    CHAR_LOG_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  pStatusChar = pService->createCharacteristic(
    CHAR_STATUS_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  NimBLEDevice::startAdvertising();
}

String BleManager::getTimestamp() {
  unsigned long sec = millis() / 1000;
  char buf[20];
  snprintf(buf, sizeof(buf), "+%lu sec", sec);
  return String(buf);
}

void BleManager::onConnect(NimBLEServer* pServer) {
  deviceConnected = true;
  StateManager::changeState(STATE_BLE_CONNECTED);
  updateStatus();
}

void BleManager::onDisconnect(NimBLEServer* pServer) {
  deviceConnected = false;
  StateManager::changeState(STATE_IDLE);
  NimBLEDevice::startAdvertising();
}

void BleManager::onWrite(NimBLECharacteristic* pCharacteristic) {
  std::string val = pCharacteristic->getValue();
  if (val.length() > 0) {
    String payload = String(val.c_str());
    
    if (payload.startsWith("STICKER:")) {
      distributeStickerId = payload.substring(8);
    } else if (payload.startsWith("SPOT:")) {
      spotName = payload.substring(5);
    } else if (payload == "GET_LOGS") {
      stickerSosMgr.flushLogsToBle();
    }

    lastSyncTime = "iPad Synced";
    updateStatus();
    StateManager::changeState(STATE_IDLE);
  }
}

void BleManager::updateStatus() {
  if (!pStatusChar) return;
  String status = "{\"station_id\":\"" + deviceId + "\",\"spot_name\":\"" + spotName + "\",\"distribute_sticker_id\":\"" + distributeStickerId + "\"}";
  pStatusChar->setValue((uint8_t*)status.c_str(), status.length());
  pStatusChar->notify();
}

void BleManager::sendLogsToApp(const String& jsonLogs) {
  if (!pLogChar || !deviceConnected) return;
  pLogChar->setValue((uint8_t*)jsonLogs.c_str(), jsonLogs.length());
  pLogChar->notify();
}