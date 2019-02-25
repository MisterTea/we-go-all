#include "UdpBiDirectionalRpc.hpp"

namespace wga {
void UdpBiDirectionalRpc::send(const string& message) {
  netEngine->getIoService()->post([this, message]() {
    VLOG(1) << "IN SEND LAMBDA: " << message.length() << " TO "
            << activeEndpoint;
    int bytesSent = localSocket->send_to(asio::buffer(message), activeEndpoint);
    VLOG(1) << bytesSent << " bytes sent";
  });
}

}