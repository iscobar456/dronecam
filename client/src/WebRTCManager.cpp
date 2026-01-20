#include "WebRTCManager.hpp"
#include <rtc/rtc.hpp>

bool WebRTCManager::init() { return true; }

void WebRTCManager::sendRTPPacket(guint8 *packet, const gsize size) {
  rtc::InitLogger(rtc::LogLevel::Debug);
  auto pc = std::make_shared<rtc::PeerConnection>();

  pc->onStateChange([](rtc::PeerConnection::State state) {
    std::cout << "State: " << state << std::endl;
  });

  // pc->onGatheringStateChange([pc](rtc::PeerConnection::GatheringState state)
  // {
  //   std::cout << "Gathering State: " << state << std::endl;
  //   if (state == rtc::PeerConnection::GatheringState::Complete) {
  //     auto description = pc->localDescription();
  //     json message = {{"type", description->typeString()},
  //                     {"sdp", std::string(description.value())}};
  //     std::cout << message << std::endl;
  //   }
  // });

  const rtc::SSRC ssrc = 42;
  rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
  media.addH264Codec(
      96); // Must match the payload type of the external h264 RTP stream
  media.addSSRC(ssrc, "video-send");
  auto track = pc->addTrack(media);

  pc->setLocalDescription();

  track->send(reinterpret_cast<std::byte *>(packet), size);
  return;
}
