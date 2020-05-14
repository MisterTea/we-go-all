#include "UdpBiDirectionalRpc.hpp"

namespace wga {
void UdpBiDirectionalRpc::send(const string& message) {
  if (lastSendTime != time(NULL)) {
    lastSendTime = time(NULL);
    sendBytes = 0;
  }
  string localMessage = message;  // Needed to keep message in RAM
  sendBytes += localMessage.size();
  if (sendBytes > 20 * 1024) {
    // 10 kb/sec max
    LOG(INFO) << "Reached max throughput, dropping";
    return;
  }
  int64_t delay = 0;
  if (flaky) {
    while (true) {
      int64_t number = int64_t(flakyDelayDist(generator));
      if (number > 0 && number < 2000) {
        delay = number;
        break;
      }
    }
  }
  auto timer = shared_ptr<asio::steady_timer>(netEngine->createTimer(
      std::chrono::steady_clock::now() + std::chrono::milliseconds(delay)));
  timer->async_wait([this, localMessage, timer](const asio::error_code& error) {
    if (error) {
      return;
    }
    netEngine->post([this, localMessage, timer]() {
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