#include "UdpBiDirectionalRpc.hpp"

namespace wga {
void UdpBiDirectionalRpc::send(const string& message) {
  auto timer = shared_ptr<asio::steady_timer>(
      netEngine->createTimer(std::chrono::steady_clock::now() +
                             std::chrono::milliseconds(flaky ? 200 : 0)));
  timer->async_wait([this, message, timer](const asio::error_code& error) {
    netEngine->post([this, message, timer]() {
      string localMessage = message;  // Needed to keep message in RAM
      lock_guard<recursive_mutex> guard(this->mutex);
      VLOG(1) << "IN SEND LAMBDA: " << localMessage.length() << " TO "
              << this->activeEndpoint;
      try {
        int bytesSent = int(this->localSocket->send_to(
            asio::buffer(localMessage), this->activeEndpoint));
        VLOG(1) << bytesSent << " bytes sent";
      } catch (const system_error& se) {
        LOG(ERROR) << "Got error trying to send: " << se.what();
        // At this point we should try a new endpoint
      }
    });
  });
}

}  // namespace wga