#pragma once
#include <NimBLEDevice.h>
#include "Config.h"

class BleManager : public NimBLEServerCallbacks, public NimBLECharacteristicCallbacks {
private:
  NimBLEServer* pServer = nullptr;
  NimBLECharacteristic* pConfigChar = nullptr;
  NimBLECharacteristic* pLogChar = nullptr;
  NimBLECharacteristic* pStatusChar = nullptr;
  bool deviceConnected = false;

public:
  String deviceId;
  String spotName;
  String distributeStickerId;
  String lastSyncTime;

  void init();
  void updateStatus();
  void sendLogsToApp(const String& jsonLogs);
  String getTimestamp();

  void onConnect(NimBLEServer* pServer) override;
  void onDisconnect(NimBLEServer* pServer) override;
  void onWrite(NimBLECharacteristic* pCharacteristic) override;
};

extern BleManager bleMgr;