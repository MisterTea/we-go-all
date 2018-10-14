#include "Headers.hpp"

#include "gtest/gtest.h"

#include "LogHandler.hpp"
#include "MultiEndpointHandler.hpp"
#include "PortMultiplexer.hpp"

using namespace wga;

struct RpcDetails {
  RpcId id;
  string request;
  string reply;
  int from;
  int to;
};

class FlakyRpcTest : public testing::Test {
 protected:
  void SetUp() override {
    srand(time(NULL));
    CryptoHandler::init();
    ioService = shared_ptr<asio::io_service>(new asio::io_service());
    ioServiceMutex.reset(new mutex());
  }

  void TearDown() override {
    LOG(INFO) << "TEARING DOWN";
    ioService->stop();
    ioServiceThread.join();
    LOG(INFO) << "TEAR DOWN COMPLETE";
  }

  void initFullyConnectedMesh(int numNodes) {
    // Create public/private keys
    for (int a = 0; a < numNodes; a++) {
      keys.push_back(CryptoHandler::generateKey());
    }
    string secretKeyString;
    FATAL_IF_FALSE(
        Base64::Encode(string(keys[0].second.data(), keys[0].second.size()),
                       &secretKeyString));
    LOG(INFO) << secretKeyString;

    // Create port multiplexers
    for (int a = 0; a < numNodes; a++) {
      shared_ptr<udp::socket> localSocket(
          new udp::socket(*ioService, udp::endpoint(udp::v4(), 20000 + a)));
      servers.push_back(shared_ptr<PortMultiplexer>(
          new PortMultiplexer(ioService, ioServiceMutex, localSocket)));
    }

    // Create crypto handlers
    for (int a = 0; a < numNodes; a++) {
      cryptoHandlers.push_back(vector<shared_ptr<CryptoHandler>>());
      for (int b = 0; b < numNodes; b++) {
        if (a == b) {
          cryptoHandlers[a].push_back(shared_ptr<CryptoHandler>());
          continue;
        }
        shared_ptr<CryptoHandler> cryptoHandler(
            new CryptoHandler(keys[a].first, keys[a].second));
        cryptoHandler->setOtherPublicKey(keys[b].first);
        cryptoHandlers[a].push_back(cryptoHandler);
      }
    }

    // Create session keys
    for (int a = 0; a < numNodes; a++) {
      for (int b = 0; b < numNodes; b++) {
        if (a == b) {
          continue;
        }
        if (!cryptoHandlers[a][b]->recieveIncomingSessionKey(
                cryptoHandlers[b][a]->generateOutgoingSessionKey(
                    cryptoHandlers[a][b]->getMyPublicKey()))) {
          LOG(FATAL) << "Receive session key failed";
        }
      }
    }

    // Create endpoint handlers
    for (int a = 0; a < numNodes; a++) {
      rpcs.push_back(vector<shared_ptr<MultiEndpointHandler>>());
      for (int b = 0; b < numNodes; b++) {
        if (a == b) {
          rpcs[a].push_back(shared_ptr<MultiEndpointHandler>());
          continue;
        }
        udp::resolver resolver(*ioService);
        udp::resolver::query query(udp::v4(), "127.0.0.1",
                                   std::to_string(20000 + b));
        auto it = resolver.resolve(query);
        auto remoteEndpoint = it->endpoint();
        LOG(INFO) << "GOT ENTRY: " << remoteEndpoint;
        LOG(INFO) << "GOT ENTRY2: "
                  << ((++it) ==
                      asio::ip::basic_resolver_results<asio::ip::udp>());
        shared_ptr<MultiEndpointHandler> endpointHandler(
            new MultiEndpointHandler(servers[a]->getLocalSocket(), ioService,
                                     cryptoHandlers[a][b], remoteEndpoint));
        endpointHandler->setFlaky(true);
        rpcs[a].push_back(endpointHandler);
        servers[a]->addEndpointHandler(endpointHandler);
      }
    }

    ioServiceThread = std::thread([this]() { this->ioService->run(); });
  }

  void runTest(int numNodes, int numTrials) {
    map<RpcId, RpcDetails> allRpcDetails;
    for (int trials = 0; trials < numTrials; trials++) {
      RpcDetails rpcDetails;
      rpcDetails.from = rand() % numNodes;
      do {
        rpcDetails.to = rand() % numNodes;
      } while (rpcDetails.to == rpcDetails.from);
      rpcDetails.request = string("AAAAAAAA");
      for (int a = 0; a < rpcDetails.request.length(); a++) {
        rpcDetails.request[a] += rand() % 26;
      }
      rpcDetails.reply = rpcDetails.request;
      transform(rpcDetails.reply.begin(), rpcDetails.reply.end(),
                rpcDetails.reply.begin(), ::tolower);
      rpcDetails.id =
          rpcs[rpcDetails.from][rpcDetails.to]->request(rpcDetails.request);
      allRpcDetails[rpcDetails.id] = rpcDetails;
    }

    auto currentTime =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()) /
        100;
    int numAcks = 0;
    while (true) {
      lock_guard<std::mutex> guard(*ioServiceMutex);
      auto iterationTime =
          duration_cast<milliseconds>(system_clock::now().time_since_epoch()) /
          100;
      for (auto& a : rpcs) {
        for (auto& rpc : a) {
          if (rpc.get() == NULL) {
            continue;
          }

          if (rpc->hasIncomingRequest()) {
            auto request = rpc->consumeIncomingRequest();
            LOG(INFO) << "GOT REQUEST, SENDING REPLY";
            RpcDetails& rpcDetails = allRpcDetails.find(request.id)->second;
            EXPECT_EQ(request.payload, rpcDetails.request);
            rpc->reply(request.id, rpcDetails.reply);
          }

          if (rpc->hasIncomingReply()) {
            auto reply = rpc->consumeIncomingReply();
            LOG(INFO) << "GOT REPLY";
            RpcDetails& rpcDetails = allRpcDetails.find(reply.id)->second;
            EXPECT_EQ(reply.payload, rpcDetails.reply);
            numAcks++;
          }

          if (currentTime != iterationTime) {
            rpc->heartbeat();
          }
        }
      }

      if (numAcks == numTrials) {
        // Test complete
        break;
      }

      usleep(1000);
      if (currentTime != iterationTime) {
        currentTime = iterationTime;
      }
    }
  }

  shared_ptr<asio::io_service> ioService;
  std::thread ioServiceThread;
  shared_ptr<mutex> ioServiceMutex;
  vector<pair<PublicKey, PrivateKey>> keys;
  vector<shared_ptr<PortMultiplexer>> servers;
  vector<vector<shared_ptr<CryptoHandler>>> cryptoHandlers;
  vector<vector<shared_ptr<MultiEndpointHandler>>> rpcs;
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
