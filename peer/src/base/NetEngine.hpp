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

 protected:
  shared_ptr<asio::io_service> ioService;
  thread ioServiceThread;
  shared_ptr<recursive_mutex> ioServiceMutex;
};
}  // namespace wga

#endif
