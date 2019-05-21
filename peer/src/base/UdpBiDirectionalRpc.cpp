#include "UdpBiDirectionalRpc.hpp"

namespace wga {
void UdpBiDirectionalRpc::send(const string& message) {
  netEngine->post([this, message]() {
    lock_guard<recursive_mutex> guard(this->mutex);
    VLOG(1) << "IN SEND LAMBDA: " << message.length() << " TO "
            << this->activeEndpoint;
    int bytesSent =
        this->localSocket->send_to(asio::buffer(message), this->activeEndpoint);
    VLOG(1) << bytesSent << " bytes sent";
  });
}

}  // namespace wga