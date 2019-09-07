#ifndef __NET_ENGINE_H__
#define __NET_ENGINE_H__

#include "Headers.hpp"
#include "PortMappingHandler.hpp"

namespace wga {
class NetEngine {
 public:
  NetEngine() {
    portMappingHandler = make_shared<PortMappingHandler>();
    ioService.reset(new asio::io_service());
    work.emplace(*ioService);
  }

  void start() {
    ioServiceThread.reset(new std::thread([this]() {
      LOG(ERROR) << "NET ENGINE STARTING";
      ioService->run();
      LOG(ERROR) << "NET ENGINE FINISHED";
    }));
  }

  void shutdown() {
    LOG(INFO) << "SHUTTING DOWN: " << uint64_t(portMappingHandler.get());
    portMappingHandler.reset();
    LOG(INFO) << "Stopping work";
    ioService->post([this]() {
      LOG(INFO) << "Clearing work";
      work.reset();  // let io_service run out of work
      LOG(INFO) << "Work cleared";
    });
    LOG(INFO) << "Joining thread";
    if (ioServiceThread && ioServiceThread->joinable()) {
      LOG(INFO) << "Thread is joinable";
      ioServiceThread->join();
    }
    ioServiceThread.reset();
    while (work.has_value()) {
      LOG(INFO) << "Waiting for work to finish";
      microsleep(1000 * 1000);
    }
    LOG(INFO) << "Resetting net engine";
    ioService.reset();
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
