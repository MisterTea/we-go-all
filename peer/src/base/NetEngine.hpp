#ifndef __NET_ENGINE_H__
#define __NET_ENGINE_H__

#include "Headers.hpp"
#include "PortMappingHandler.hpp"

#if defined(__APPLE__)
#include <pthread.h>
#endif

namespace wga {
class NetEngine {
 public:
  NetEngine() {
    portMappingHandler = make_shared<PortMappingHandler>();
    ioService.reset(new asio::io_service());
    work.emplace(*ioService);
  }

  ~NetEngine() {
    if (work) {
      shutdown();
    }
  }

  void start() {
    ioServiceThread.reset(new std::thread([this]() {
#if defined(__APPLE__)
      // A background MAME window must not have its lockstep transport timer
      // coalesced or deprioritized by macOS. Missing this timer directly
      // stalls the foreground peer's emulation thread.
      pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
      pthread_setname_np("mamehub-network");
#endif
      LOG(ERROR) << "NET ENGINE STARTING";
      ioService->run();
      LOG(ERROR) << "NET ENGINE FINISHED";
    }));
  }

  void shutdown() {
    if (!ioService) {
      return;
    }
    LOG(INFO) << "SHUTTING DOWN: " << uint64_t(portMappingHandler.get());
    portMappingHandler.reset();
    LOG(INFO) << "Stopping work";
    work.reset();
    if (ioService) {
      ioService->stop();
    }
    LOG(INFO) << "Joining thread";
    if (ioServiceThread && ioServiceThread->joinable()) {
      LOG(INFO) << "Thread is joinable";
      ioServiceThread->join();
    }
    LOG(INFO) << "Resetting net engine";
    ioService.reset();
    ioServiceThread.reset();
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
      std::chrono::time_point<std::chrono::steady_clock> launchPoint) {
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

  inline shared_ptr<asio::io_service> getIoService() { return ioService; }

 protected:
  shared_ptr<PortMappingHandler> portMappingHandler;
  shared_ptr<asio::io_service> ioService;
  shared_ptr<thread> ioServiceThread;
  optional<asio::io_service::work> work;
};
}  // namespace wga

#endif
