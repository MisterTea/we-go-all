#include "Headers.hpp"

#include "gtest/gtest.h"

#include "LogHandler.hpp"
#include "MultiEndpointHandler.hpp"
#include "NetEngine.hpp"
#include "PortMultiplexer.hpp"
#include "RpcServer.hpp"

using namespace wga;

struct RpcDetails {
  RpcId id;
  string request;
  string reply;
  int from;
  PublicKey destinationKey;
};

class FlakyRpcTest : public testing::Test {
 protected:
  void SetUp() override {
    srand(time(NULL));
    CryptoHandler::init();
    netEngine.reset(
        new NetEngine(shared_ptr<asio::io_service>(new asio::io_service())));
  }

  void TearDown() override {
    LOG(INFO) << "TEARING DOWN";
    netEngine->shutdown();
    LOG(INFO) << "TEAR DOWN COMPLETE";
  }

  void initFullyConnectedMesh(int numNodes) {
    // Create public/private keys
    for (int a = 0; a < numNodes; a++) {
      keys.push_back(CryptoHandler::generateKey());
    }
    string secretKeyString;
    FATAL_IF_FALSE(Base64::Encode(
        string((const char*)keys[0].second.data(), keys[0].second.size()),
        &secretKeyString));
    LOG(INFO) << secretKeyString;

    // Create port multiplexers
    for (int a = 0; a < numNodes; a++) {
      shared_ptr<udp::socket> localSocket(new udp::socket(
          *netEngine->getIoService(), udp::endpoint(udp::v4(), 20000 + a)));
      servers.push_back(
          shared_ptr<RpcServer>(new RpcServer(netEngine, localSocket)));
    }

    // Create crypto handlers
    vector<vector<shared_ptr<CryptoHandler>>> cryptoHandlers;
    for (int a = 0; a < numNodes; a++) {
      cryptoHandlers.push_back(vector<shared_ptr<CryptoHandler>>());
      for (int b = 0; b < numNodes; b++) {
        if (a == b) {
          cryptoHandlers[a].push_back(shared_ptr<CryptoHandler>());
          continue;
        }
        shared_ptr<CryptoHandler> cryptoHandler(
            new CryptoHandler(keys[a].second, keys[b].first));
        cryptoHandlers[a].push_back(cryptoHandler);
      }
    }

    // Create endpoint handlers
    for (int a = 0; a < numNodes; a++) {
      for (int b = 0; b < numNodes; b++) {
        if (a == b) {
          continue;
        }
        auto remoteEndpoint =
            netEngine->resolve("127.0.0.1", std::to_string(20000 + b));
        shared_ptr<MultiEndpointHandler> endpointHandler(
            new MultiEndpointHandler(servers[a]->getLocalSocket(), netEngine,
                                     cryptoHandlers[a][b], {remoteEndpoint}));
        endpointHandler->setFlaky(true);
        servers[a]->addEndpoint(endpointHandler);
      }
    }

    netEngine->start();

    int numFinished = 0;
    vector<shared_ptr<thread>> initThreads;
    int numTotal = servers.size();
    for (auto& server : servers) {
      initThreads.push_back(shared_ptr<thread>(
          new std::thread([server, this, &numFinished, numTotal]() {
            server->runUntilInitialized();
            numFinished++;
            while (numFinished < numTotal) {
              {
                lock_guard<recursive_mutex> guard(*this->netEngine->getMutex());
                server->heartbeat();
              }
              usleep(1000 * 1000);
            }
          })));
    }
    for (auto it : initThreads) {
      it->join();
    }
  }

  void runTest(int numNodes, int numTrials) {
    map<RpcId, RpcDetails> allRpcDetails;
    for (int trials = 0; trials < numTrials; trials++) {
      RpcDetails rpcDetails;
      rpcDetails.from = rand() % numNodes;
      int to = rpcDetails.from;
      do {
        to = rand() % numNodes;
      } while (to == rpcDetails.from);
      rpcDetails.destinationKey = keys[to].first;
      rpcDetails.request = string("AAAAAAAA");
      for (int a = 0; a < rpcDetails.request.length(); a++) {
        rpcDetails.request[a] += rand() % 26;
      }
      rpcDetails.reply = rpcDetails.request;
      transform(rpcDetails.reply.begin(), rpcDetails.reply.end(),
                rpcDetails.reply.begin(), ::tolower);
      rpcDetails.id = servers[rpcDetails.from]->request(
          rpcDetails.destinationKey, rpcDetails.request);
      allRpcDetails[rpcDetails.id] = rpcDetails;
    }

    auto currentTime =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()) /
        100;
    int numAcks = 0;
    while (true) {
      lock_guard<recursive_mutex> guard(*netEngine->getMutex());
      auto iterationTime =
          duration_cast<milliseconds>(system_clock::now().time_since_epoch()) /
          100;
      for (auto& server : servers) {
        auto request = server->getIncomingRequest();
        if (!request.empty()) {
          LOG(INFO) << "GOT REQUEST, SENDING REPLY";
          RpcDetails& rpcDetails = allRpcDetails.find(request.id)->second;
          EXPECT_EQ(request.payload, rpcDetails.request);
          server->reply(request.key, request.id, rpcDetails.reply);
        }

        auto reply = server->getIncomingReply();
        if (!reply.empty()) {
          RpcDetails& rpcDetails = allRpcDetails.find(reply.id)->second;
          EXPECT_EQ(reply.payload, rpcDetails.reply);
          numAcks++;
          LOG(INFO) << "GOT REPLY: " << numAcks;
        }

        if (currentTime != iterationTime) {
          server->heartbeat();
        }
      }

      if (numAcks >= numTrials) {
        // Test complete
        break;
      }

      usleep(1000);
      if (currentTime != iterationTime) {
        currentTime = iterationTime;
      }
    }
  }

  shared_ptr<NetEngine> netEngine;
  vector<shared_ptr<RpcServer>> servers;
  vector<pair<PublicKey, PrivateKey>> keys;
};

TEST_F(FlakyRpcTest, TwoNodes) {
  const int numNodes = 2;
  const int numTrials = 1000;
  initFullyConnectedMesh(numNodes);

  runTest(numNodes, numTrials);
}

TEST_F(FlakyRpcTest, ThreeNodes) {
  const int numNodes = 3;
  const int numTrials = 1000;
  initFullyConnectedMesh(numNodes);

  runTest(numNodes, numTrials);
}

TEST_F(FlakyRpcTest, TenNodes) {
  const int numNodes = 10;
  const int numTrials = 1000;
  initFullyConnectedMesh(numNodes);

  runTest(numNodes, numTrials);
}
