#include "Headers.hpp"

#include "gtest/gtest.h"

#include "BiDirectionalRpc.hpp"
#include "LogHandler.hpp"

using namespace wga;

class FlakyRpcTest : public testing::Test {
 protected:
  void SetUp() override {
    ioService = shared_ptr<asio::io_service>(new asio::io_service());
    shared_ptr<asio::io_service> localService = ioService;
    rpc1 = setupConnection(6888, 6889);
    rpc2 = setupConnection(6889, 6888);
    ioServiceThread = std::thread([localService]() { localService->run(); });
  }

  void TearDown() override {
    LOG(INFO) << "TEARING DOWN";
    ioService->stop();
    ioServiceThread.join();
    rpc1->shutdown();
    rpc2->shutdown();
    LOG(INFO) << "TEAR DOWN COMPLETE";
  }

  shared_ptr<BiDirectionalRpc> setupConnection(
      int selfPort, int remotePort) {
    shared_ptr<udp::socket> localSocket(new udp::socket(
        *ioService, udp::endpoint(udp::v4(), selfPort)));
    udp::resolver resolver(*ioService);
    udp::resolver::query query(udp::v4(), "127.0.0.1", std::to_string(remotePort));
    auto it = resolver.resolve(query);
    auto remoteEndpoint = it->endpoint();
    LOG(INFO) << "GOT ENTRY: " << remoteEndpoint.size();

    shared_ptr<udp::socket> remoteSocket(new udp::socket(*ioService));
    remoteSocket->open(udp::v4());

    shared_ptr<BiDirectionalRpc> rpc(new BiDirectionalRpc(
        ioService, localSocket, remoteSocket, remoteEndpoint));
    rpc->setFlaky(true);
    return rpc;
  }

  shared_ptr<asio::io_service> ioService;
  shared_ptr<BiDirectionalRpc> rpc1;
  shared_ptr<BiDirectionalRpc> rpc2;
  std::thread ioServiceThread;
};

TEST_F(FlakyRpcTest, ReadWrite) {
  auto id1 = rpc1->request("1234");
  auto id2 = rpc2->request("ABCD");
  auto currentTime = time(NULL);
  int numAcks = 0;
  while (true) {
    if (rpc1->hasIncomingRequest()) {
      auto request = rpc1->consumeIncomingRequest();
      LOG(INFO) << "GOT REQUEST, SENDING REPLY";
      EXPECT_EQ(request.payload, "ABCD");
      rpc1->reply(request.id, "EFGH");
    }
    if (rpc2->hasIncomingRequest()) {
      auto request = rpc2->consumeIncomingRequest();
      LOG(INFO) << "GOT REQUEST, SENDING REPLY";
      EXPECT_EQ(request.payload, "1234");
      rpc2->reply(request.id, "5678");
    }

    if (rpc1->hasIncomingReplyWithId(id1)) {
      LOG(INFO) << "GOT REPLY";
      auto reply = rpc1->consumeIncomingReplyWithId(id1);
      EXPECT_EQ(reply, "5678");
      numAcks++;
    }
    if (rpc2->hasIncomingReplyWithId(id2)) {
      LOG(INFO) << "GOT REPLY";
      auto reply = rpc2->consumeIncomingReplyWithId(id2);
      EXPECT_EQ(reply, "EFGH");
      numAcks++;
    }

    if (numAcks == 2) {
      // Test complete
      break;
    }

    usleep(1000);
    if (currentTime != time(NULL)) {
      currentTime = time(NULL);
      rpc1->heartbeat();
      rpc2->heartbeat();
    }
  }
}
