#include "Headers.hpp"

#include "gtest/gtest.h"

#include "BiDirectionalRpc.hpp"
#include "FanOutRpc.hpp"
#include "LogHandler.hpp"

using namespace wga;

class FlakyFanOutTest : public testing::Test {
 protected:
  void SetUp() override {
    ioService = shared_ptr<asio::io_service>(new asio::io_service());
    shared_ptr<asio::io_service> localService = ioService;
    client1 = setupConnection(12000, 12001);
    client2 = setupConnection(12002, 12001);
    server = setupFanOutConnection(12001, {12000, 12002});
    ioServiceThread = std::thread([localService]() { localService->run(); });
  }

  void TearDown() override {
    LOG(INFO) << "TEARING DOWN";
    ioService->stop();
    ioServiceThread.join();
    client1->shutdown();
    client2->shutdown();
    server->shutdown();
    LOG(INFO) << "TEAR DOWN COMPLETE";
  }

  shared_ptr<BiDirectionalRpc> setupConnection(int selfPort, int remotePort) {
    shared_ptr<udp::socket> localSocket(
        new udp::socket(*ioService, udp::endpoint(udp::v4(), selfPort)));
    udp::resolver resolver(*ioService);
    udp::resolver::query query(udp::v4(), "127.0.0.1",
                               std::to_string(remotePort));
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

  shared_ptr<FanOutRpc> setupFanOutConnection(int selfPort,
                                              vector<int> remotePorts) {
    shared_ptr<udp::socket> localSocket(
        new udp::socket(*ioService, udp::endpoint(udp::v4(), selfPort)));
    vector<shared_ptr<BiDirectionalRpc>> rpcs;
    for (int remotePort : remotePorts) {
      udp::resolver resolver(*ioService);
      udp::resolver::query query(udp::v4(), "127.0.0.1",
                                 std::to_string(remotePort));
      auto it = resolver.resolve(query);
      auto remoteEndpoint = it->endpoint();
      LOG(INFO) << "GOT ENTRY: " << remoteEndpoint.size();

      shared_ptr<udp::socket> remoteSocket(new udp::socket(*ioService));
      remoteSocket->open(udp::v4());

      shared_ptr<BiDirectionalRpc> rpc(new BiDirectionalRpc(
          ioService, localSocket, remoteSocket, remoteEndpoint));
      rpc->setFlaky(true);
      rpcs.push_back(rpc);
    }
    return shared_ptr<FanOutRpc>(new FanOutRpc(rpcs));
  }

  shared_ptr<asio::io_service> ioService;
  shared_ptr<BiDirectionalRpc> client1, client2;
  shared_ptr<FanOutRpc> server;
  std::thread ioServiceThread;
};

TEST_F(FlakyFanOutTest, ReadWrite) {
  auto serverId = server->request("abcd");
  auto currentTime = time(NULL);
  int numAcks = 0;
  while (true) {
    if (client1->hasIncomingRequest()) {
      auto request = client1->consumeIncomingRequest();
      LOG(INFO) << "GOT REQUEST, SENDING REPLY";
      EXPECT_EQ(request.payload, "abcd");
      client1->reply(request.id, "efgh1");
    }
    if (client2->hasIncomingRequest()) {
      auto request = client2->consumeIncomingRequest();
      LOG(INFO) << "GOT REQUEST, SENDING REPLY";
      EXPECT_EQ(request.payload, "abcd");
      client2->reply(request.id, "efgh2");
    }

    if (server->hasIncomingReplyWithId(serverId)) {
      auto request = server->consumeIncomingReplyWithId(serverId);
      LOG(INFO) << "GOT REQUEST, SENDING REPLY";
      EXPECT_EQ(request[0], "efgh1");
      numAcks++;
      EXPECT_EQ(request[1], "efgh2");
      numAcks++;
    }

    if (numAcks == 2) {
      // Test complete
      break;
    }

    usleep(1000);
    if (currentTime != time(NULL)) {
      currentTime = time(NULL);
      client1->heartbeat();
      client2->heartbeat();
      server->heartbeat();
    }
  }
}
