#include "Streamer.hpp"
#include <cstdint>
#include <glib.h>
#include <string>
#include <vector>

class ConnectionManager {
  bool isConnected = false;
  void setRemoteDescription();

  void createRtcPC();
  void createSdp();
  void sendSdp();
  void createTrack();
  void onGatheringComplete();
  int pc{-1};
  int tr = -1;
  Streamer *streamer;
  uint32_t ssrc;
  int width = FOOTAGE_WIDTH;
  int height = FOOTAGE_HEIGHT;
  std::vector<const char *> iceCandidates;
  const std::string getSdp();

  class WebSocketManager {
    ConnectionManager *cm;
    std::string id;
    std::string peerId;
    void handleMessage(char *message);
    int ws{-1};
    std::string remote;

  public:
    void startWebSocket();
    void sendSdp(const char *sdp);
    void sendCand(const char *cand);
    WebSocketManager(ConnectionManager *cm);
    void setPeerId(std::string peerId) { this->peerId = peerId; };
    int socketId() const { return ws; };
  };

  WebSocketManager wsm;

public:
  ConnectionManager(Streamer *streamer);
  void makeConnection();
  void closeConnection();
  void sendPacket(const char *packet, int size);
  void addCandidate(const char *cand);
  void setPeerId(std::string peerId);
  void setSsrc(uint32_t ssrc) { this->ssrc = ssrc; };
  void setStreamDimensions(int w, int h) {
    width = w;
    height = h;
  }
  int getPc() const { return pc; }
  bool hasRemoteSdp = false;
  std::vector<const char *> remoteIceCandidates;
};
