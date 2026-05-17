#include "ConnectionManager.hpp"
#include "rtc/rtc.h"
#include "utils.hpp" // DEBUG_MSG
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <rtc/rtc.hpp>
#include <string>

using json = nlohmann::json;

// CALLBACKS

void newCandidateCallback(int pc, const char *cand, const char *mid,
                          void *user_ptr) {
  DEBUG_MSG("enter pc=" << pc << " cand=" << (cand ? cand : "null"));
  std::cout << "new candidate discovered. running callback" << std::endl;
  ConnectionManager *cm = reinterpret_cast<ConnectionManager *>(user_ptr);
  DEBUG_MSG("cm=" << cm);
  cm->addCandidate(cand);
  DEBUG_MSG("exit");
};

void openCallback(int id, void *user_ptr) {
  DEBUG_MSG("enter id=" << id << " user_ptr=" << user_ptr);
};

void closedCallback(int id, void *user_ptr) {
  DEBUG_MSG("enter id=" << id << " user_ptr=" << user_ptr);
};

void errorCallback(int id, const char *error, void *user_ptr) {
  DEBUG_MSG("enter id=" << id << " error=" << (error ? error : "null"));
  std::cout << "WS connection error: " << error << std::endl;
};

void messageCallback(int id, const char *message, int size, void *connMan) {
  DEBUG_MSG("enter id=" << id << " size=" << size << " connMan=" << connMan);
  ConnectionManager *cm = reinterpret_cast<ConnectionManager *>(connMan);
  if (cm == nullptr) {
    DEBUG_MSG("cm is null, returning");
    return;
  }

  json data;
  if (message[0] != '\0') {
    DEBUG_MSG("parsing message: " << message);
    data = json::parse(message);
  } else {
    DEBUG_MSG("empty message, returning");
    return;
  }

  std::string mType;
  data.at("type").get_to(mType);
  DEBUG_MSG("mType=" << mType);

  std::string bodyData;
  data.at("body").at("data").get_to(bodyData);

  if (mType == "sd") {
    DEBUG_MSG("handling sd");
    json offer = json::parse(bodyData.c_str());
    std::string sdp;
    offer.at("sdp").get_to(sdp);

    int remoteSdpCode =
        rtcSetRemoteDescription(cm->getPc(), sdp.c_str(), "answer");
    DEBUG_MSG("rtcSetRemoteDescription code=" << remoteSdpCode);
    if (remoteSdpCode >= 0) {
      cm->hasRemoteSdp = true;
    }

    std::string remoteSdp;
    remoteSdp.resize(4096);
    rtcGetRemoteDescription(cm->getPc(), remoteSdp.data(),
                            4096); // not resizing back. don't need to

    DEBUG_MSG("adding " << cm->remoteIceCandidates.size()
                        << " buffered remote candidates");
    for (auto cand : cm->remoteIceCandidates) {
      rtcAddRemoteCandidate(cm->getPc(), cand, NULL);
    }
  } else if (mType == "ice") {
    DEBUG_MSG("handling ice");
    json candidate = json::parse(bodyData.c_str());
    std::string cand;
    candidate.at("candidate").get_to(cand);
    // std::cout << "received candidate: '" << cand << "'" << std::endl;
    if (cand.empty()) {
      DEBUG_MSG("empty candidate, returning");
      return;
    }
    if (cm->hasRemoteSdp) {
      DEBUG_MSG("adding remote candidate (hasRemoteSdp)");
      rtcAddRemoteCandidate(cm->getPc(), cand.c_str(), NULL);
    } else {
      DEBUG_MSG("buffering remote candidate");
      cm->remoteIceCandidates.push_back(cand.c_str());
    }
  } else if (mType == "select") {
    DEBUG_MSG("handling select");
    std::string peerId;
    data.at("body").at("from").get_to(peerId);
    DEBUG_MSG("peerId=" << peerId);
    cm->setPeerId(peerId);
    json streamArgs = json::parse(bodyData);
    DEBUG_MSG("streamArgs parsed bodyData=" << bodyData);
    int width = FOOTAGE_WIDTH;
    int height = FOOTAGE_HEIGHT;
    streamArgs.at("width").get_to(width);
    streamArgs.at("height").get_to(height);
    DEBUG_MSG("stream dimensions width=" << width << " height=" << height);
    cm->setStreamDimensions(width, height);
    DEBUG_MSG("calling makeConnection");
    cm->makeConnection();
    DEBUG_MSG("makeConnection returned");
  } else if (mType == "disconnect") {
    DEBUG_MSG("handling disconnect");
    cm->closeConnection();
    DEBUG_MSG("closeConnection returned");
  } else {
    DEBUG_MSG("unhandled mType=" << mType);
  }
  DEBUG_MSG("exit");
};

void trackOpenCallback(int id, void *user_ptr) {
  DEBUG_MSG("enter id=" << id << " user_ptr=" << user_ptr);
  std::cout << "track open" << std::endl;
};
void trackClosedCallback(int id, void *user_ptr) {
  DEBUG_MSG("enter id=" << id << " user_ptr=" << user_ptr);
  std::cout << "track closed" << std::endl;
};
void trackErrorCallback(int id, const char *error, void *user_ptr) {
  DEBUG_MSG("enter id=" << id << " error=" << (error ? error : "null"));
  std::cout << error << std::endl;
};

// CONNECTION MANAGER IMPLEMENTATIONS

ConnectionManager::ConnectionManager(Streamer *streamer)
    : streamer(streamer), wsm(WebSocketManager(this)) {
  DEBUG_MSG("enter streamer=" << streamer);
  wsm.startWebSocket();
  DEBUG_MSG("websocket started");
  rtcInitLogger(RTC_LOG_INFO, NULL);
  DEBUG_MSG("exit");
}

void ConnectionManager::makeConnection() {
  DEBUG_MSG("enter streamer=" << streamer << " width=" << width
                              << " height=" << height << " ssrc=" << ssrc);
  createRtcPC();
  DEBUG_MSG("createRtcPC done pc=" << pc);
  createTrack(); // requires ssrc to have been set
  DEBUG_MSG("createTrack done tr=" << tr);
  DEBUG_MSG("calling streamer->startStream");
  streamer->startStream(tr, width, height);
  DEBUG_MSG("streamer->startStream returned");
  createSdp();
  DEBUG_MSG("createSdp done");
  sendSdp();
  DEBUG_MSG("exit");
}

void ConnectionManager::closeConnection() {
  DEBUG_MSG("enter pc=" << pc << " tr=" << tr);
  DEBUG_MSG("calling streamer->stopStream");
  streamer->stopStream();
  DEBUG_MSG("streamer->stopStream returned");
  rtcDelete(tr);
  DEBUG_MSG("rtcDelete tr done");
  rtcDeletePeerConnection(pc);
  DEBUG_MSG("rtcDeletePeerConnection done");
  tr = -1;
  pc = -1;
  setPeerId("");
  hasRemoteSdp = false;
  DEBUG_MSG("exit");
}

void ConnectionManager::createRtcPC() {
  DEBUG_MSG("enter");
  rtcConfiguration config{};
  const char *iceServers[] = {"stun:stun.relay.metered.ca:80",
                              "turn:bfc22cc224cb894f60cff28a:"
                              "KkdyHKXNNh8ptCyj@standard.relay.metered.ca:"
                              "80",
                              "turn:bfc22cc224cb894f60cff28a:"
                              "KkdyHKXNNh8ptCyj@standard.relay.metered.ca:"
                              "443"};

  config.iceServers = iceServers;
  config.iceServersCount = 2;
  config.bindAddress = "0.0.0.0";
  config.disableAutoNegotiation = false;

  pc = rtcCreatePeerConnection(&config);
  DEBUG_MSG("rtcCreatePeerConnection pc=" << pc);

  rtcSetUserPointer(pc, this);
  rtcSetLocalCandidateCallback(pc, newCandidateCallback);
  DEBUG_MSG("exit");
}

void ConnectionManager::createTrack() {
  DEBUG_MSG("enter pc=" << pc << " ssrc=" << ssrc);
  std::string cname = generate_uuid();
  DEBUG_MSG("cname=" << cname);
  /* fmtp must match rtph264pay (FU-A / packetization-mode=1). See
   * libdatachannel Description::Video — profile string becomes a=fmtp:<pt>
   * <this>. */
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
  DEBUG_MSG("rtcAddTrackEx tr=" << tr);
  rtcSetOpenCallback(tr, &trackOpenCallback);
  rtcSetClosedCallback(tr, &trackClosedCallback);
  rtcSetErrorCallback(tr, &trackErrorCallback);

  int nackErr = rtcChainRtcpNackResponder(tr, NACK_CACHE_PACKETS);
  DEBUG_MSG("rtcChainRtcpNackResponder err=" << nackErr);

  const double pacedBps = static_cast<double>(VIDEO_BITRATE) * 1.25;
  int paceErr =
      rtcChainPacingHandler(tr, pacedBps, 5); // not too sure about 5ms pacing
  DEBUG_MSG("rtcChainPacingHandler err=" << paceErr);
  DEBUG_MSG("exit");
};

void ConnectionManager::createSdp() {
  DEBUG_MSG("enter pc=" << pc);
  rtcSetLocalDescription(pc, "offer");
  DEBUG_MSG("exit");
}
void ConnectionManager::sendSdp() {
  DEBUG_MSG("enter");
  wsm.sendSdp(getSdp().c_str());
  DEBUG_MSG("exit");
}

void ConnectionManager::sendPacket(const char *packet, int size) {
  DEBUG_MSG("enter size=" << size);
}

void ConnectionManager::addCandidate(const char *cand) {
  DEBUG_MSG("enter cand=" << (cand ? cand : "null")
                          << " isConnected=" << isConnected);
  if (isConnected) {
    wsm.sendCand(cand);
  } else {
    iceCandidates.push_back(cand);
    DEBUG_MSG("buffered candidate, count=" << iceCandidates.size());
  }
  DEBUG_MSG("exit");
};

void ConnectionManager::setPeerId(std::string peerId) {
  DEBUG_MSG("enter peerId=" << peerId);
  wsm.setPeerId(peerId);
  isConnected = true;
  DEBUG_MSG("flushing " << iceCandidates.size() << " buffered candidates");
  for (auto cand : iceCandidates) {
    wsm.sendCand(cand);
  }
  iceCandidates.clear();
  DEBUG_MSG("exit");
};

const std::string ConnectionManager::getSdp() {
  DEBUG_MSG("enter pc=" << pc);
  std::string buffer;
  buffer.resize(4096);
  int code = rtcGetLocalDescription(pc, buffer.data(), 4096);
  DEBUG_MSG("rtcGetLocalDescription code=" << code);
  return buffer;
}

// WEBSOCKET MANAGEMENT IMPLEMENTATIONS

ConnectionManager::WebSocketManager::WebSocketManager(
    ConnectionManager *connMan) {
  DEBUG_MSG("enter connMan=" << connMan);
  this->cm = connMan;
  this->id = generate_uuid();
  DEBUG_MSG("exit id=" << id);
}

void ConnectionManager::WebSocketManager::startWebSocket() {
  DEBUG_MSG("enter");
  std::string url = WEBSOCKET_URL;
  url.append("?id=" + id + "&type=drone");
  if (std::string(DEVICE_NAME) != "UNSET") {
    url.append(std::string("&name=") + DEVICE_NAME);
  }
  std::cout << url << std::endl;

  ws = rtcCreateWebSocket(url.c_str());
  DEBUG_MSG("rtcCreateWebSocket ws=" << ws);
  rtcSetUserPointer(ws, this->cm);
  rtcSetOpenCallback(ws, openCallback);
  rtcSetClosedCallback(ws, closedCallback);
  rtcSetErrorCallback(ws, errorCallback);
  rtcSetMessageCallback(ws, messageCallback);
  DEBUG_MSG("exit");
}

void ConnectionManager::WebSocketManager::sendSdp(const char *sdp) {
  DEBUG_MSG("enter peerId=" << peerId << " ws=" << ws);
  json message;
  message["type"] = "sd";
  json body;
  body["to"] = peerId;
  body["from"] = id;
  body["data"] = sdp;
  message["body"] = body;
  std::string message_str =
      message.dump(-1, ' ', false, json::error_handler_t::replace);
  std::cout << "sending sdp message: " << message_str << std::endl;
  rtcSendMessage(ws, message_str.c_str(), message_str.size());
  DEBUG_MSG("exit");
};

void ConnectionManager::WebSocketManager::sendCand(const char *cand) {
  DEBUG_MSG("enter peerId=" << peerId << " ws=" << ws);
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
  DEBUG_MSG("exit");
};
