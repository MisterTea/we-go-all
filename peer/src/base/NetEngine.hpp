#ifndef __NET_ENGINE_H__
#define __NET_ENGINE_H__

#include "Headers.hpp"

namespace wga {
class NetEngine {
 public:
  explicit NetEngine(shared_ptr<asio::io_service> _ioService)
      : ioService(_ioService) {}

  void start() {
    ioServiceThread = std::thread([this]() {
      this->ioService->run();
      LOG(ERROR) << "NET ENGINE FINISHED";
    });
  }

  void shutdown() {
    ioService->stop();
    ioServiceThread.join();
  }

  shared_ptr<asio::io_service> getIoService() { return ioService; }

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
  shared_ptr<asio::io_service> ioService;
  thread ioServiceThread;
};
}  // namespace wga

#endif
