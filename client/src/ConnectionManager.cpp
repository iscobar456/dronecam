#include "ConnectionManager.hpp"
#include "rtc/rtc.h"
#include "utils.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <rtc/rtc.hpp>
#include <string>

using json = nlohmann::json;

// CALLBACKS

void newCandidateCallback(int pc, const char *cand, const char *mid,
                          void *user_ptr) {
  std::cout << "new candidate discovered. running callback" << std::endl;
  ConnectionManager *cm = reinterpret_cast<ConnectionManager *>(user_ptr);
  cm->addCandidate(cand);
};

void openCallback(int id, void *user_ptr) {
  // std::cout << "WS connection opened" << std::endl;
};

void closedCallback(int id, void *user_ptr) {
  // std::cout << "WS connection closed" << std::endl;
};

void errorCallback(int id, const char *error, void *user_ptr) {
  std::cout << "WS connection error: " << error << std::endl;
};

void messageCallback(int id, const char *message, int size, void *connMan) {
  ConnectionManager *cm = reinterpret_cast<ConnectionManager *>(connMan);
  if (cm == nullptr) {
    return;
  }

  json data;
  if (message[0] != '\0') {
    data = json::parse(message);
  }

  std::string mType;
  data.at("type").get_to(mType);

  std::string bodyData;
  data.at("body").at("data").get_to(bodyData);

  if (mType == "sd") {
    json offer = json::parse(bodyData.c_str());
    std::string sdp;
    offer.at("sdp").get_to(sdp);

    int remoteCode =
        rtcSetRemoteDescription(cm->getPc(), sdp.c_str(), "answer");

    std::string remoteSdp;
    remoteSdp.resize(12288);

    int getRemoteCode =
        rtcGetRemoteDescription(cm->getPc(), remoteSdp.data(), 12288);
    if (getRemoteCode > 0) {
      remoteSdp.resize(static_cast<size_t>(getRemoteCode) - 1);
    }
    std::cout << "get observer sdp returned code: " << getRemoteCode
              << std::endl;
    std::cout << remoteSdp << std::endl;

    for (auto cand : cm->remoteIceCandidates) {
      rtcAddRemoteCandidate(cm->getPc(), cand, NULL);
    }
  } else if (mType == "ice") {
    json candidate = json::parse(bodyData.c_str());
    std::string cand;
    candidate.at("candidate").get_to(cand);
    // std::cout << "received candidate: '" << cand << "'" << std::endl;
    if (cand.empty()) {
      return;
    }
    if (cm->hasRemoteSdp) {
      rtcAddRemoteCandidate(cm->getPc(), cand.c_str(), NULL);
    } else {
      cm->remoteIceCandidates.push_back(cand.c_str());
    }
  } else if (mType == "select") {
    std::string peerId;
    data.at("body").at("from").get_to(peerId);
    cm->setPeerId(peerId);
    cm->createSdp();
    cm->sendSdp();
  }
};

void trackOpenCallback(int id, void *user_ptr) {
  std::cout << "track open" << std::endl;
};
void trackClosedCallback(int id, void *user_ptr) {
  std::cout << "track closed" << std::endl;
};
void trackErrorCallback(int id, const char *error, void *user_ptr) {
  std::cout << error << std::endl;
};

// CONNECTION MANAGER IMPLEMENTATIONS

ConnectionManager::ConnectionManager() : wsm(WebSocketManager(this)) {}

bool ConnectionManager::init() {
  rtcInitLogger(RTC_LOG_DEBUG, NULL);

  wsm.startWebSocket();

  // Initialize peer connection
  rtcConfiguration config{};
  const char *iceServers[] = {"stun:stun.barracuda.com:3478",
                              "stun:stun.actionvoip.com:3478"};
  config.iceServers = iceServers;
  config.iceServersCount = 2;
  config.bindAddress = "0.0.0.0";
  config.disableAutoNegotiation = false;

  pc = rtcCreatePeerConnection(&config);

  rtcSetUserPointer(pc, this);
  rtcSetLocalCandidateCallback(pc, newCandidateCallback);

  std::cout << "successful init" << std::endl;
  return true;
};

void ConnectionManager::configureTrack(uint32_t ssrc) {
  std::string cname = generate_uuid();
  /* fmtp must match rtph264pay (FU-A / packetization-mode=1). See libdatachannel
   * Description::Video — profile string becomes a=fmtp:<pt> <this>. */
  static const std::string kH264Fmtp =
      "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f";

  rtcTrackInit rti{};
  rti.direction = RTC_DIRECTION_SENDONLY;
  rti.codec = RTC_CODEC_H264;
  rti.payloadType = 96;
  rti.ssrc = ssrc;
  rti.mid = "drone_feed";
  rti.name = cname.c_str();
  rti.profile = kH264Fmtp.c_str();

  tr = rtcAddTrackEx(this->pc, &rti);
  rtcSetOpenCallback(tr, &trackOpenCallback);
  rtcSetClosedCallback(tr, &trackClosedCallback);
  rtcSetErrorCallback(tr, &trackErrorCallback);

  std::cout << "successful configure" << std::endl;
};

void ConnectionManager::createSdp() { rtcSetLocalDescription(pc, "offer"); }
void ConnectionManager::sendSdp() { wsm.sendSdp(getSdp().c_str()); }

void ConnectionManager::sendPacket(const char *packet, int size) {
  if (!rtcIsOpen(tr)) {
    return;
  }
  rtcSendMessage(tr, packet, size);
}

void ConnectionManager::addCandidate(const char *cand) {
  if (isConnected) {
    wsm.sendCand(cand);
  } else {
    iceCandidates.push_back(cand);
  }
};

void ConnectionManager::setPeerId(std::string peerId) {
  wsm.setPeerId(peerId);
  isConnected = true;
  for (auto cand : iceCandidates) {
    wsm.sendCand(cand);
  }
  iceCandidates.clear();
};

const std::string ConnectionManager::getSdp() {
  std::string buffer;
  buffer.resize(12288);
  int code = rtcGetLocalDescription(pc, buffer.data(), 12288);
  if (code <= 0) {
    return {};
  }
  /* copyAndReturn returns (sdp.size() + 1) including trailing '\0' */
  buffer.resize(static_cast<size_t>(code) - 1);
  return buffer;
}

// WEBSOCKET MANAGEMENT IMPLEMENTATIONS

ConnectionManager::WebSocketManager::WebSocketManager(
    ConnectionManager *connMan) {
  this->cm = connMan;
  this->id = generate_uuid();
}

void ConnectionManager::WebSocketManager::startWebSocket() {
  std::string url = WEBSOCKET_URL;
  url.append("?id=" + id + "&type=drone");
  std::cout << url << std::endl;

  ws = rtcCreateWebSocket(url.c_str());
  rtcSetUserPointer(ws, this->cm);
  rtcSetOpenCallback(ws, openCallback);
  rtcSetClosedCallback(ws, closedCallback);
  rtcSetErrorCallback(ws, errorCallback);
  rtcSetMessageCallback(ws, messageCallback);
}

void ConnectionManager::WebSocketManager::sendSdp(const char *sdp) {
  json message;
  message["type"] = "sd";
  json body;
  body["to"] = peerId;
  body["from"] = id;
  body["data"] = sdp;
  message["body"] = body;
  try {
    std::string message_str =
        message.dump(-1, ' ', false, json::error_handler_t::replace);
    std::cout << "sending sdp message: " << message_str << std::endl;
    rtcSendMessage(ws, message_str.c_str(), message_str.size());
  } catch (const std::exception &e) {
    // This is where the "silence" ends
    std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
  }
};

void ConnectionManager::WebSocketManager::sendCand(const char *cand) {
  json message;
  message["type"] = "ice";
  json body;
  body["to"] = peerId;
  body["from"] = id;
  body["data"] = cand;
  message["body"] = body;
  std::string message_str = message.dump();
  std::cout << "sending ice message" << std::endl;
  rtcSendMessage(ws, message_str.c_str(), message_str.size());
};
