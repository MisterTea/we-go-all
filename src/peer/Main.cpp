#include "Headers.hpp"
#include "LogHandler.hpp"
#include "BiDirectionalRpc.hpp"

using namespace wga;

DEFINE_int32(selfport, 0, "self port");
DEFINE_int32(port, 0, "remote port");

int main(int argc, char** argv) {
  // Version string need to be set before GFLAGS parse arguments
  SetVersionString(string(WGA_VERSION));

  // Setup easylogging configurations
  el::Configurations defaultConf = LogHandler::SetupLogHandler(&argc, &argv);

  shared_ptr<asio::io_service> ioService(new asio::io_service());
  shared_ptr<udp::socket> localSocket(
      new udp::socket(*ioService, udp::endpoint(udp::v4(), FLAGS_selfport)));

  udp::resolver resolver(*ioService);
  udp::resolver::query query(udp::v4(), "127.0.0.1", std::to_string(FLAGS_port));
  auto it = resolver.resolve(query);
  auto remoteEndpoint = it->endpoint();
  LOG(INFO) << "GOT ENTRY: " << remoteEndpoint.size();

  shared_ptr<BiDirectionalRpc> rpc(new BiDirectionalRpc(ioService, localSocket, remoteEndpoint));
  rpc->setFlaky(true);

  std::thread t([ioService](){ ioService->run(); });

  auto id = rpc->request("ASDF");
  auto currentTime = time(NULL);
  while(true) {
    if (rpc->hasIncomingRequest()) {
      auto request = rpc->consumeIncomingRequest().id;
      LOG(INFO) << "GOT REQUEST, SENDING REPLY";
      rpc->reply(request, "QWER");
    }
    if (rpc->hasIncomingReplyWithId(id)) {
      LOG(INFO) << "GOT REPLY";
      auto reply = rpc->consumeIncomingReplyWithId(id);
    }
    usleep(1000);
    if (currentTime != time(NULL)) {
      currentTime = time(NULL);
      rpc->heartbeat();
    }
  }
}