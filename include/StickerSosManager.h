#pragma once
#include <vector>
#include <algorithm>
#include "Config.h"

class StickerSosManager {
private:
  std::vector<String> distributedTodayList;
  int lastCheckedDay;

  void checkDailyReset();

public:
  std::vector<DistributeLog> pendingDistributeLogs;
  std::vector<SosLog> pendingSosLogs;

  StickerSosManager();
  void handlePacket(const CommunicationPacket& packet, int rssi = -50);
};

extern StickerSosManager stickerSosMgr;