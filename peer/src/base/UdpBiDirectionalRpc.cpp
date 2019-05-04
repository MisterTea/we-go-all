#include "UdpBiDirectionalRpc.hpp"

namespace wga {
void UdpBiDirectionalRpc::send(const string& message) {
  auto localSocket_ = localSocket;
  auto activeEndpoint_ = activeEndpoint;
  netEngine->getIoService()->post([localSocket_, activeEndpoint_, message]() {
    VLOG(1) << "IN SEND LAMBDA: " << message.length() << " TO "
            << activeEndpoint_;
    int bytesSent =
        localSocket_->send_to(asio::buffer(message), activeEndpoint_);
    VLOG(1) << bytesSent << " bytes sent";
  });
}

}  // namespace wga