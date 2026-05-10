#include <cstdint>
#include <glib.h>
#include <string>
#include <vector>

class ConnectionManager {
  bool isTrackConfigured = false;
  void setRemoteDescription();

  void initializeTrack();
  void onGatheringComplete();
  int pc;
  std::vector<const char *> iceCandidates;

  class WebSocketManager {
    ConnectionManager *cm;
    void handleMessage(char *message);
    int ws;
    std::string remote;

  public:
    void sendDesc();
    void sendIce();
    WebSocketManager(ConnectionManager *cm);
  };

  WebSocketManager wsm;

public:
  ConnectionManager() : wsm(WebSocketManager(this)) {};
  void sendPacket(const char *packet, int size);
  bool init();
  void configureTrack(uint32_t ssrc);
  void addCandidate(const char *cand);
};
