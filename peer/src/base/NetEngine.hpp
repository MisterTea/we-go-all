#ifndef __NET_ENGINE_H__
#define __NET_ENGINE_H__

#include "Headers.hpp"

namespace wga {
class NetEngine {
 public:
  explicit NetEngine(shared_ptr<asio::io_service> _ioService)
      : ioService(_ioService) {
    ioServiceMutex.reset(new recursive_mutex());
  }

  void start() {
    ioServiceThread = std::thread([this]() { this->ioService->run(); });
  }

  void shutdown() {
    ioService->stop();
    ioServiceThread.join();
  }

  shared_ptr<asio::io_service> getIoService() { return ioService; }

  shared_ptr<recursive_mutex> getMutex() { return ioServiceMutex; }

  udp::endpoint resolve(const string& hostname, const string& port) {
    udp::resolver resolver(*ioService);
    udp::resolver::query query(udp::v4(), hostname, port);
    auto it = resolver.resolve(query);
    auto remoteEndpoint = it->endpoint();
    it++;
    VLOG(1) << "GOT ENTRY: " << remoteEndpoint;
    VLOG(1) << "GOT ENTRY2: "
              << ((it) == asio::ip::basic_resolver_results<asio::ip::udp>());
    if (it != asio::ip::basic_resolver_results<asio::ip::udp>()) {
      LOG(FATAL) << "Ambiguous endpoint";
    }
    return remoteEndpoint;
  }

 protected:
  shared_ptr<asio::io_service> ioService;
  thread ioServiceThread;
  shared_ptr<recursive_mutex> ioServiceMutex;
};
}  // namespace wga

#endif
