#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <vector>
#include "Config.h"

class NetworkManager {
public:
  String deviceId;
  String spotName;
  String distributeStickerId;
  String lastSyncTime;

  void init();
  void resetSettings();
  String getTimestamp();
  void fetchStationConfig();
  void syncDistributeLogs(std::vector<DistributeLog>& logs);
  bool verifyAndSendSos(const String& childId, const String& timestamp);
};

extern NetworkManager networkMgr;