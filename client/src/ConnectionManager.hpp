#include <cstdint>
#include <glib.h>
#include <string>
#include <vector>

class ConnectionManager {
  bool isConnected = false;
  void setRemoteDescription();

  void initializeTrack();
  void onGatheringComplete();
  int pc{-1};
  int tr = -1;
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
  ConnectionManager();
  ConnectionManager(const ConnectionManager &) = delete;
  ConnectionManager &operator=(const ConnectionManager &) = delete;
  void sendPacket(const char *packet, int size);
  void createSdp();
  void sendSdp();
  bool init();
  void configureTrack(uint32_t ssrc);
  void addCandidate(const char *cand);
  void setPeerId(std::string peerId);
  int getPc() const { return pc; }
  bool hasRemoteSdp = false;
  std::vector<const char *> remoteIceCandidates;
};
