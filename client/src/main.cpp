#include "ConnectionManager.hpp"
#include "Streamer.hpp"
#include "utils.hpp"

int main() {
  DEBUG_MSG("enter");
  Streamer *streamer = new Streamer();
  DEBUG_MSG("Streamer created streamer=" << streamer);
  ConnectionManager *connMan = new ConnectionManager(streamer);
  DEBUG_MSG("ConnectionManager created connMan=" << connMan);
  connMan->setSsrc(streamer->getSsrc());
  DEBUG_MSG("ssrc set, main idle (process should stay alive)");
  DEBUG_MSG("exit main (if you see this, main returned unexpectedly)");
}
