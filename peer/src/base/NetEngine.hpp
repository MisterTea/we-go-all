#ifndef __NET_ENGINE_H__
#define __NET_ENGINE_H__

#include "Headers.hpp"
#include "PortMappingHandler.hpp"

namespace wga {
class NetEngine {
 public:
  explicit NetEngine(shared_ptr<asio::io_service> _ioService)
      : ioService(_ioService) {
    portMappingHandler = make_shared<PortMappingHandler>();
  }

  void start() {
    ioServiceThread = std::thread([this]() {
      this->ioService->run();
      LOG(ERROR) << "NET ENGINE FINISHED";
    });
  }

  void shutdown() {
    LOG(INFO) << "SHUTTING DOWN: " << uint64_t(portMappingHandler.get());
    portMappingHandler.reset();
    ioService->stop();
    ioServiceThread.join();
  }

  template <typename F>
  inline void post(F f) {
    return ioService->post(f);
  }

  inline udp::socket* startUdpServer(int serverPort) {
    portMappingHandler->mapPort(
        serverPort, std::string("WGA: ") + std::to_string(serverPort));
    return new udp::socket(*ioService, udp::endpoint(udp::v4(), serverPort));
  }

  inline void forwardPort(int serverPort) {
    portMappingHandler->mapPort(
        serverPort, std::string("WGA: ") + std::to_string(serverPort));
  }

  inline asio::steady_timer* createTimer(
      std::chrono::time_point<std::chrono::high_resolution_clock> launchPoint) {
    return new asio::steady_timer(*ioService, launchPoint);
  }

  vector<udp::endpoint> resolve(const string& hostname, const string& port) {
    udp::resolver resolver(*ioService);
    udp::resolver::query query(udp::v4(), hostname, port);
    auto it = resolver.resolve(query);
    auto remoteEndpoint = it->endpoint();
    vector<udp::endpoint> retval = {remoteEndpoint};
    it++;
    VLOG(1) << "GOT ENTRY: " << remoteEndpoint;
    VLOG(1) << "GOT ENTRY2: "
            << ((it) == asio::ip::basic_resolver_results<asio::ip::udp>());
    while (it != asio::ip::basic_resolver_results<asio::ip::udp>()) {
      retval.push_back(it->endpoint());
      it++;
    }
    return retval;
  }

 protected:
  shared_ptr<PortMappingHandler> portMappingHandler;
  shared_ptr<asio::io_service> ioService;
  thread ioServiceThread;
};
}  // namespace wga

#endif
