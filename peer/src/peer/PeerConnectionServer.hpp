#ifndef __PEER_CONNECTION_SERVER_H__
#define __PEER_CONNECTION_SERVER_H__

#include "Headers.hpp"

#include "NetEngine.hpp"
#include "SingleGameServer.hpp"

namespace wga {
class PeerConnectionServer {
 public:
  PeerConnectionServer(shared_ptr<NetEngine> _netEngine, int _port,
                       shared_ptr<SingleGameServer> _singleGameServer)
      : singleGameServer(_singleGameServer) {
    socket.reset(_netEngine->startUdpServer(_port));
    asio::socket_base::reuse_address option(true);
    socket->set_option(option);
    receive();
  }

  void receive() {
    socket->async_receive_from(
        asio::buffer(data), sender_endpoint,
        [this](std::error_code ec, std::size_t bytes_recvd) {
          if (!ec && bytes_recvd > 0) {
            string s(data.data(), bytes_recvd);
            LOG(INFO) << "GOT ENDPOINT DATA: " << s;
            auto tokens = split(s, '_');
            string id = tokens[0];
            string udpSendEndpoint = sender_endpoint.address().to_string() +
                                     ":" + to_string(sender_endpoint.port());
            vector<string> allEndpoints = {udpSendEndpoint};
            allEndpoints.insert(allEndpoints.end(), tokens.begin() + 1,
                                tokens.end());
            singleGameServer->setPeerEndpoints(id, allEndpoints);
            socket->async_send_to(
                asio::buffer(string("OK")), sender_endpoint,
                [this](std::error_code /*ec*/, std::size_t /*bytes_sent*/) {
                  receive();
                });
          } else {
            receive();
          }
        });
  }

 protected:
  shared_ptr<udp::socket> socket;
  udp::endpoint sender_endpoint;
  shared_ptr<SingleGameServer> singleGameServer;
  array<char, 65536> data;
};
}  // namespace wga

#endif
