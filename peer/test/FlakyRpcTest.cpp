#include "Headers.hpp"

#include "gtest/gtest.h"

#include "EncryptedMultiEndpointHandler.hpp"
#include "LogHandler.hpp"
#include "MultiEndpointHandler.hpp"
#include "NetEngine.hpp"
#include "PortMultiplexer.hpp"
#include "RpcServer.hpp"
#include "TimeHandler.hpp"

using namespace wga;

struct RpcDetails {
  RpcId id;
  string request;
  string reply;
  string destinationId;
};

class FlakyRpcTest : public testing::Test {
 protected:
  void SetUp() override {
    srand(time(NULL));
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
    vector<pair<PublicKey, PrivateKey>> keys;
    for (int a = 0; a < numNodes; a++) {
      keys.push_back(CryptoHandler::generateKey());
    }

    // Create port multiplexers
    for (int a = 0; a < numNodes; a++) {
      shared_ptr<udp::socket> localSocket(netEngine->startUdpServer(20000 + a));
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
        shared_ptr<EncryptedMultiEndpointHandler> endpointHandler(
            new EncryptedMultiEndpointHandler(servers[a]->getLocalSocket(),
                                              netEngine, cryptoHandlers[a][b],
                                              {remoteEndpoint}));
        endpointHandler->setFlaky(true);
        servers[a]->addEndpoint(to_string(b), endpointHandler);
      }
    }

    netEngine->start();
  }

  void runTestOneNode(shared_ptr<RpcServer> server, int numTrials, int numTotal,
                      int* numFinished) {
    server->runUntilInitialized();

    vector<string> peerIds = server->getPeerIds();
    EXPECT_EQ(peerIds.size(), numTotal - 1);
    map<RpcId, RpcDetails> allRpcDetails;
    {
      for (int trials = 0; trials < numTrials; trials++) {
        RpcDetails rpcDetails;
        rpcDetails.destinationId = peerIds[rand() % peerIds.size()];
        rpcDetails.request = string("AAAAAAAA");
        for (int a = 0; a < int(rpcDetails.request.length()); a++) {
          rpcDetails.request[a] += rand() % 26;
        }
        rpcDetails.reply = rpcDetails.request;
        transform(rpcDetails.reply.begin(), rpcDetails.reply.end(),
                  rpcDetails.reply.begin(), ::tolower);
        rpcDetails.id =
            server->request(rpcDetails.destinationId, rpcDetails.request);
        VLOG(1) << "SENDING " << rpcDetails.id.id << " TO "
                << rpcDetails.destinationId;
        allRpcDetails[rpcDetails.id] = rpcDetails;
      }
    }

    auto currentTime =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()) /
        100;
    int numAcks = 0;
    bool complete = false;
    while ((*numFinished) < numTotal) {
      auto iterationTime =
          duration_cast<milliseconds>(system_clock::now().time_since_epoch()) /
          100;
      {
        auto request = server->getIncomingRequest();
        if (request) {
          string payload = request->payload;
          transform(payload.begin(), payload.end(), payload.begin(), ::tolower);
          VLOG(1) << "GOT REQUEST, SENDING " << request->id.id << " TO "
                  << request->userId << " WITH " << payload;
          server->reply(request->userId, request->id, payload);
        }

        auto reply = server->getIncomingReply();
        if (reply) {
          if (allRpcDetails.find(reply->id) == allRpcDetails.end()) {
            for (auto it : allRpcDetails) {
              LOG(INFO) << it.first.id;
            }
            LOGFATAL << "Got invalid rpc id: " << reply->id.id;
          }
          RpcDetails& rpcDetails = allRpcDetails.find(reply->id)->second;
          if (reply->payload != rpcDetails.reply) {
            LOGFATAL << "Reply doesn't match expectation: " << reply->payload
                     << " != " << rpcDetails.reply;
          }
          numAcks++;
          VLOG(1) << "GOT REPLY: " << numAcks;
        }

        if (currentTime != iterationTime) {
          server->heartbeat();
        }

        if (numAcks >= numTrials && !complete) {
          // Test complete
          complete = true;
          (*numFinished)++;
          LOG(INFO) << "Got all replies:" << (*numFinished) << "/" << numTotal;
        }
      }

      usleep(1000);
      if (currentTime != iterationTime) {
        currentTime = iterationTime;
      }
    }
    LOG(INFO) << "Exiting test loop";
  }

  void runTest(int numTrials) {
    vector<shared_ptr<thread>> runThreads;
    int numTotal = servers.size();
    int numFinished = 0;
    for (auto& server : servers) {
      runThreads.push_back(shared_ptr<thread>(
          new thread(&FlakyRpcTest::runTestOneNode, this, server, numTrials,
                     numTotal, &numFinished)));
    }
    for (auto it : runThreads) {
      it->join();
      LOG(INFO) << "THREAD FINISHED";
    }
    LOG(INFO) << "TEST FINISHED";
  }

  shared_ptr<NetEngine> netEngine;
  vector<shared_ptr<RpcServer>> servers;
};

TEST_F(FlakyRpcTest, TwoNodes) {
  const int numNodes = 2;
  const int numTrials = 100;
  initFullyConnectedMesh(numNodes);

  runTest(numTrials);
}

TEST_F(FlakyRpcTest, ThreeNodes) {
  const int numNodes = 3;
  const int numTrials = 100;
  initFullyConnectedMesh(numNodes);

  runTest(numTrials);
}

TEST_F(FlakyRpcTest, TenNodes) {
  const int numNodes = 10;
  const int numTrials = 100;
  initFullyConnectedMesh(numNodes);

  runTest(numTrials);
}
