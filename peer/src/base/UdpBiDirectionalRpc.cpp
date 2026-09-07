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
  for (int a=0;a<(doubleSends?2:1); a++) {
    int64_t delay = 0;
    if (flaky) {
      while (true) {
        int64_t number = int64_t(flakyDelayDist(generator));
        if (number > 0 && number < 2000) {
          delay = number;
          break;
        }
      }
    } else if (a > 0) {
      // Stagger duplicate sends by 4ms to avoid shared micro-burst packet loss
      delay = 4 * a;
    }

    if (delay) {
      auto timer = shared_ptr<asio::steady_timer>(netEngine->createTimer(
          std::chrono::steady_clock::now() + std::chrono::milliseconds(delay)));
      weak_ptr<UdpBiDirectionalRpc> weakSelf = weak_from_this();
      timer->async_wait([weakSelf, localMessage, timer](const asio::error_code& error) {
        if (error) {
          return;
        }
        auto self = weakSelf.lock();
        if (self) {
          self->_send(localMessage);
        }
      });
    } else {
        _send(localMessage);
    }
  }
}

void UdpBiDirectionalRpc::_send(const string& localMessage) {
  // Snapshot the destination before posting.  Endpoint selection may change
  // while this send is waiting on the network thread.
  auto const destination = activeEndpoint;
  weak_ptr<UdpBiDirectionalRpc> weakSelf = weak_from_this();
  netEngine->post([weakSelf, localMessage, destination]() {
    auto self = weakSelf.lock();
    if (!self) {
      return;
    }
    lock_guard<recursive_mutex> guard(self->mutex);
    VLOG(1) << "IN SEND LAMBDA: " << localMessage.length() << " TO "
            << destination;
    try {
      int bytesSent = int(self->localSocket->send_to(
          asio::buffer(localMessage), destination));
      VLOG(1) << bytesSent << " bytes sent";
    } catch (const system_error& se) {
      LOG(ERROR) << "Got error trying to send: " << se.what();
      self->onSendError(destination);
    }
  });
}

}  // namespace wga
