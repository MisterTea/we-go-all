#ifndef __UDP_BI_DIRECTIONAL_RPC_H__
#define __UDP_BI_DIRECTIONAL_RPC_H__

#include "BiDirectionalRpc.hpp"
#include "NetEngine.hpp"

namespace wga {
class UdpBiDirectionalRpc : public BiDirectionalRpc {
 public:
  UdpBiDirectionalRpc(shared_ptr<NetEngine> _netEngine,
                      shared_ptr<udp::socket> _localSocket)
      : netEngine(_netEngine), localSocket(_localSocket), flakyDelayDist(100,50) {}

  virtual ~UdpBiDirectionalRpc() {}

  virtual void send(const string& message);

  void setEndpoint(const udp::endpoint& destination) {
    activeEndpoint = destination;
  }

 protected:
  shared_ptr<NetEngine> netEngine;
  shared_ptr<udp::socket> localSocket;
  udp::endpoint activeEndpoint;
  normal_distribution<double> flakyDelayDist;
  default_random_engine generator;
};
}  // namespace wga

#endif