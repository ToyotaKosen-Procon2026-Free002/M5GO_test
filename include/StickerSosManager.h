#pragma once
#include <vector>
#include <algorithm>
#include "Config.h"

class StickerSosManager {
private:
  std::vector<String> distributedTodayList;

public:
  std::vector<DistributeLog> pendingDistributeLogs;
  std::vector<SosLog> pendingSosLogs;

  StickerSosManager();
  void handlePacket(const CommunicationPacket& packet, int rssi = -50);
  void handleSos(const String& childId, const String& source);
  void flushLogsToBle();
};

extern StickerSosManager stickerSosMgr;