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
  PublicKey destinationKey;
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
    string secretKeyString = base64::encode((const char*)keys[0].second.data(),
                                            keys[0].second.size());
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
        shared_ptr<EncryptedMultiEndpointHandler> endpointHandler(
            new EncryptedMultiEndpointHandler(servers[a]->getLocalSocket(),
                                              netEngine, cryptoHandlers[a][b],
                                              {remoteEndpoint}));
        endpointHandler->setFlaky(true);
        servers[a]->addEndpoint(
            endpointHandler->getCryptoHandler()->getOtherPublicKey(),
            endpointHandler);
      }
    }

    netEngine->start();
  }

  void runTestOneNode(shared_ptr<RpcServer> server, int numTrials, int numTotal,
                      int* numFinished) {
    server->runUntilInitialized();

    vector<PublicKey> peerKeys = server->getPeerKeys();
    EXPECT_EQ(peerKeys.size(), numTotal - 1);
    map<RpcId, RpcDetails> allRpcDetails;
    {
      lock_guard<recursive_mutex> guard(*netEngine->getMutex());
      for (int trials = 0; trials < numTrials; trials++) {
        RpcDetails rpcDetails;
        rpcDetails.destinationKey = peerKeys[rand() % peerKeys.size()];
        rpcDetails.request = string("AAAAAAAA");
        for (int a = 0; a < int(rpcDetails.request.length()); a++) {
          rpcDetails.request[a] += rand() % 26;
        }
        rpcDetails.reply = rpcDetails.request;
        transform(rpcDetails.reply.begin(), rpcDetails.reply.end(),
                  rpcDetails.reply.begin(), ::tolower);
        rpcDetails.id =
            server->request(rpcDetails.destinationKey, rpcDetails.request);
        VLOG(1) << "SENDING " << rpcDetails.id.id << " FROM "
                << CryptoHandler::keyToString(server->getMyPublicKey())
                << " TO "
                << CryptoHandler::keyToString(rpcDetails.destinationKey);
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
        lock_guard<recursive_mutex> guard(*netEngine->getMutex());
        auto request = server->getIncomingRequest();
        if (request) {
          string payload = request->payload;
          transform(payload.begin(), payload.end(), payload.begin(), ::tolower);
          VLOG(1) << "GOT REQUEST, SENDING " << request->id.id << " TO "
                  << CryptoHandler::keyToString(request->key) << " WITH "
                  << payload;
          server->reply(request->key, request->id, payload);
        }

        auto reply = server->getIncomingReply();
        if (reply) {
          if (allRpcDetails.find(reply->id) == allRpcDetails.end()) {
            LOG(INFO) << CryptoHandler::keyToString(server->getMyPublicKey());
            for (auto it : allRpcDetails) {
              LOG(INFO) << it.first.id;
            }
            LOG(FATAL) << "Got invalid rpc id: " << reply->id.id;
          }
          RpcDetails& rpcDetails = allRpcDetails.find(reply->id)->second;
          if (reply->payload != rpcDetails.reply) {
            LOG(FATAL) << "Reply doesn't match expectation: " << reply->payload
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
