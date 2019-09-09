#include "Headers.hpp"

#include "EncryptedMultiEndpointHandler.hpp"
#include "LogHandler.hpp"
#include "MultiEndpointHandler.hpp"
#include "NetEngine.hpp"
#include "PortMultiplexer.hpp"
#include "RpcServer.hpp"
#include "TimeHandler.hpp"

#undef CHECK
#include "Catch2/single_include/catch2/catch.hpp"

const int START_PORT = 30000;
namespace {
recursive_mutex testMutex;
}

using namespace wga;

struct RpcDetails {
  RpcId id;
  string request;
  string reply;
  string destinationId;
};

class FlakyRpcTest {
 public:
  void SetUp() {
    srand(uint32_t(time(NULL)));
    DISABLE_PORT_MAPPING = true;
    netEngine.reset(new NetEngine());
    netEngine->start();
  }

  void TearDown() {
    LOG(INFO) << "Shutting down servers";
    while (true) {
      bool done = true;
      for (const auto& it : servers) {
        if (it->hasWork()) {
          done = false;
        }
        it->heartbeat();
      }
      if (done) {
        break;
      } else {
        microsleep(1000 * 1000);
      }
    }
    for (const auto& it : servers) {
      it->finish();
    }
    microsleep(1000*1000);
    for (const auto& it : servers) {
      it->closeSocket();
    }
    microsleep(1000 * 1000);
    netEngine->shutdown();
    servers.clear();
    LOG(INFO) << "TEARING DOWN";
    netEngine.reset();
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
      shared_ptr<udp::socket> localSocket(
          netEngine->startUdpServer(START_PORT + a));
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
            netEngine->resolve("127.0.0.1", std::to_string(START_PORT + b));
        shared_ptr<EncryptedMultiEndpointHandler> endpointHandler(
            new EncryptedMultiEndpointHandler(servers[a]->getLocalSocket(),
                                              netEngine, cryptoHandlers[a][b],
                                              {remoteEndpoint}));
        endpointHandler->setFlaky(true);
        servers[a]->addEndpoint(to_string(b), endpointHandler);
      }
    }
  }

  void runTestOneNode(shared_ptr<RpcServer> server, int numTrials, int numTotal,
                      volatile int* numFinished) {
    server->runUntilInitialized();

    vector<string> peerIds = server->getPeerIds();
    static mutex catchMutex;
    {
      lock_guard<mutex> lock(catchMutex);
      REQUIRE(peerIds.size() == (numTotal - 1));
    }
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
    while (true) {
      {
        lock_guard<recursive_mutex> lock(testMutex);
        if ((*numFinished) == numTotal) {
          break;
        }
      }
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
          currentTime = iterationTime;
        }

        if (numAcks >= numTrials && !complete) {
          // Test complete
          complete = true;
          {
            lock_guard<recursive_mutex> lock(testMutex);
            (*numFinished)++;
            LOG(INFO) << "Got all replies:" << (*numFinished) << "/"
                      << numTotal;
          }
        }
      }

      microsleep(1000);
    }
    LOG(INFO) << "Exiting test loop";
  }

  void runTest(int numTrials) {
    vector<shared_ptr<thread>> runThreads;
    int numTotal = int(servers.size());
    volatile int numFinished = 0;
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

TEST_CASE("FlakyRpcTest", "[FlakyRpcTest]") {
  FlakyRpcTest testClass;
  LOG(INFO) << "SETTING UP";
  testClass.SetUp();

  SECTION("Two nodes") {
    const int numNodes = 2;
    const int numTrials = 100;
    testClass.initFullyConnectedMesh(numNodes);

    testClass.runTest(numTrials);
  }

  SECTION("Three nodes") {
    const int numNodes = 3;
    const int numTrials = 100;
    testClass.initFullyConnectedMesh(numNodes);

    testClass.runTest(numTrials);
  }

  SECTION("Ten nodes") {
    const int numNodes = 10;
    const int numTrials = 100;
    testClass.initFullyConnectedMesh(numNodes);

    testClass.runTest(numTrials);
  }

  LOG(INFO) << "TEARING DOWN";
  testClass.TearDown();
}
