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
  memset(&reply, 0, sizeof(reply));
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
  memset(&packet, 0, sizeof(packet));

  if (len == sizeof(CommunicationPacket)) {
    memcpy(&packet, incomingData, sizeof(packet));
  } else {
    // 文字列で送られてきた場合のフォールバック
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    strncpy(packet.device_id, macStr, sizeof(packet.device_id) - 1);
    
    String text = String((char*)incomingData).substring(0, len);
    if (text.indexOf("SOS") >= 0) {
      packet.type = 1; // SOS
    } else {
      packet.type = 0; // 通過/シール要求
    }
  }

  stickerSosMgr.handlePacket(packet, -50);
}