#include "ConnectionManager.hpp"
#include "rtc/rtc.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <rtc/rtc.hpp>
#include <string>

using json = nlohmann::json;

void newCandidateCallback(int pc, const char *cand, const char *mid,
                          void *connMan) {
  ConnectionManager *cm = reinterpret_cast<ConnectionManager *>(connMan);
  cm->addCandidate(cand);
};

bool ConnectionManager::init() {
  rtcInitLogger(RTC_LOG_DEBUG, NULL);
  // Initialize peer connection
  rtcConfiguration config;
  const char *iceServers[] = {"stun:stun.barracuda.com:3478",
                              "stun:stun.actionvoip.com:3478"};
  config.iceServers = iceServers;
  config.iceServersCount = 2;

  this->pc = rtcCreatePeerConnection(&config);
  rtcSetLocalCandidateCallback(pc, &newCandidateCallback);

  return true;
};

void ConnectionManager::configureTrack(uint32_t ssrc) {
  rtcTrackInit rti;
  rti.direction = RTC_DIRECTION_SENDONLY;
  rti.codec = RTC_CODEC_H264;
  rti.payloadType = 96;
  rti.ssrc = ssrc;
  int track = rtcAddTrackEx(this->pc, &rti);
};

void ConnectionManager::sendPacket(const char *packet, int size) {
  rtcSendMessage(pc, packet, size);
}

void ConnectionManager::addCandidate(const char *cand) {
  iceCandidates.push_back(cand);
};

// WEBSOCKET MANAGEMENT

void openCallback(int id, void *user_ptr) {
  std::cout << "WS connection opened" << std::endl;
};

void closedCallback(int id, void *user_ptr) {
  std::cout << "WS connection closed" << std::endl;
};

void errorCallback(int id, const char *error, void *user_ptr) {
  std::cout << "WS connection error: " << error << std::endl;
};

void messageCallback(int id, const char *message, int size, void *user_ptr) {
  std::cout << "WS message: " << message << std::endl;
  json data = json::parse(*message);
  std::string mType;
  data.at("type").get_to(mType); // needs exception handling
  if (mType == "sd") {
    char remoteSdp;
    data.at("body").at("data").get_to(remoteSdp);
    rtcSetRemoteDescription(id, &remoteSdp, "offer");
  }
};

ConnectionManager::WebSocketManager::WebSocketManager(
    ConnectionManager *connMan) {
  this->cm = connMan;

  std::string url = WEBSOCKET_URL;
  url.append("?id=drone1&type=drone");

  this->ws = rtcCreateWebSocket(url.c_str());
  rtcSetOpenCallback(this->ws, &openCallback);
  rtcSetClosedCallback(this->ws, &closedCallback);
  rtcSetErrorCallback(this->ws, &errorCallback);
  rtcSetMessageCallback(this->ws, &messageCallback);
};
