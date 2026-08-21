#include "EspNowManager.h"
#include "StickerSosManager.h"

static void onDataRecvWrapper(const uint8_t* mac, const uint8_t* incomingData, int len) {
  EspNowManager::onDataRecv(mac, incomingData, len);
}

void EspNowManager::init() {
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onDataRecvWrapper);
  }
}

void EspNowManager::sendSticker(const String& stationId, const String& stickerId) {
  CommunicationPacket reply;
  strncpy(reply.device_id, stationId.c_str(), sizeof(reply.device_id) - 1);
  reply.type = 0;
  strncpy(reply.stickerId, stickerId.c_str(), sizeof(reply.stickerId) - 1);

  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (!esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_add_peer(&peerInfo);
  }
  esp_now_send(broadcastAddress, (uint8_t*)&reply, sizeof(reply));
}

void EspNowManager::onDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
  CommunicationPacket packet;
  if (len == sizeof(packet)) {
    memcpy(&packet, incomingData, sizeof(packet));
    stickerSosMgr.handlePacket(packet, -50);
  }
}