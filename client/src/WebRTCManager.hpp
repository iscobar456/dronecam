#include <glib.h>

class WebRTCManager {
public:
  void sendRTPPacket(guint8 *packet, const gsize size);
  bool init();
};
