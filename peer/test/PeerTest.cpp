#include "Headers.hpp"

#include "gtest/gtest.h"

#include "CryptoHandler.hpp"
#include "MyPeer.hpp"
#include "NetEngine.hpp"
#include "PeerConnectionServer.hpp"
#include "SingleGameServer.hpp"
#include "TimeHandler.hpp"

namespace wga {
class PeerTest : public testing::Test {
 public:
  PeerTest() : testing::Test() {}

  void SetUp() override {
    srand(time(NULL));
    netEngine.reset(new NetEngine());
  }

  void initGameServer(int numPlayers) {
    for (int a = 0; a < numPlayers; a++) {
      if (!a) {
        auto privateKey = CryptoHandler::makePrivateKeyFromPassword("FooBar");
        auto publicKey = CryptoHandler::makePublicFromPrivate(privateKey);
        keys.push_back(make_pair(publicKey, privateKey));
      } else {
        keys.push_back(CryptoHandler::generateKey());
      }
    }
    server.reset(new SingleGameServer(netEngine, 20000, names[0], keys[0].first,
                                      names[0], numPlayers));

    peerConnectionServer.reset(
        new PeerConnectionServer(netEngine, 20000, server));

    netEngine->start();

    this_thread::sleep_for(chrono::seconds(1));
  }

  void TearDown() override {
    netEngine->post([this] {
      server->shutdown();
      peerConnectionServer->shutdown();
    });
    netEngine->shutdown();

    server.reset();
    peerConnectionServer.reset();
    netEngine.reset();
  }

 protected:
  vector<pair<PublicKey, PrivateKey>> keys;
  vector<string> names = {"A", "B", "C", "D"};
  shared_ptr<SingleGameServer> server;
  shared_ptr<NetEngine> netEngine;
  shared_ptr<PeerConnectionServer> peerConnectionServer;
};

TEST_F(PeerTest, ProtocolTest) {
  initGameServer(4);
  HttpClient client("localhost:20000");
  //client.io_service = netEngine->getIoService();
  string hostKey = CryptoHandler::keyToString(keys[0].first);

  string path = string("/api/get_current_game_id/") + names[0];
  auto response = client.request("GET", path);
  json result = json::parse(response->content.string());
  string gameId = result["gameId"].get<string>();
  EXPECT_EQ(gameId, server->getGameId());

  path = string("/api/host");
  json request = {
      {"hostId", names[0]}, {"gameId", gameId}, {"gameName", "Starwars"}};
  response = client.request("POST", path, request.dump(2));
  EXPECT_EQ(response->status_code, "200 OK");

  for (int a = 1; a < 4; a++) {
    path = string("/api/join");
    json request = {{"peerId", names[a]},
                    {"name", names[a]},
                    {"peerKey", CryptoHandler::keyToString(keys[a].first)}};
    response = client.request("POST", path, request.dump(2));
    EXPECT_EQ(response->status_code, "200 OK");
  }

  path = string("/api/get_game_info/") + gameId;
  response = client.request("GET", path);
  result = json::parse(response->content.string());
  EXPECT_EQ(result["gameName"], "Starwars");
  EXPECT_EQ(result["hostId"], names[0]);
  for (int a = 0; a < 4; a++) {
    string id = names[a];
    string stringKey = CryptoHandler::keyToString(keys[a].first);
    EXPECT_EQ(result["peerData"][id]["id"].get<string>(), names[a]);
    EXPECT_EQ(result["peerData"][id]["key"].get<string>(), stringKey);
    EXPECT_EQ(result["peerData"][id]["name"].get<string>(), names[a]);
    EXPECT_EQ(result["peerData"][id]["endpoints"].size(), 0);
  }

  shared_ptr<udp::socket> localSocket(netEngine->startUdpServer(12345));
  udp::endpoint serverEndpoint = netEngine->resolve("127.0.0.1", "20000")[0];
  string ipAddressPacket = names[0] + "_" + "192.168.0.1:" + to_string(12345);
  netEngine->post([localSocket, serverEndpoint, ipAddressPacket]() {
    LOG(INFO) << "IN SEND LAMBDA: " << ipAddressPacket.length();
    int bytesSent =
        localSocket->send_to(asio::buffer(ipAddressPacket), serverEndpoint);
    LOG(INFO) << bytesSent << " bytes sent";
  });
  microsleep(1000 * 1000);

  netEngine->post([localSocket]() mutable {
    localSocket->shutdown(asio::socket_base::shutdown_both);
    localSocket->close();
  });
  microsleep(1000 * 1000);

  path = string("/api/get_game_info/") + gameId;
  response = client.request("GET", path);
  result = json::parse(response->content.string());
  EXPECT_EQ(result["gameName"], "Starwars");
  EXPECT_EQ(result["hostId"], names[0]);
  for (int a = 0; a < 4; a++) {
    string id = names[a];
    string stringKey = CryptoHandler::keyToString(keys[a].first);
    EXPECT_EQ(result["peerData"][id]["id"].get<string>(), id);
    EXPECT_EQ(result["peerData"][id]["key"].get<string>(), stringKey);
    EXPECT_EQ(result["peerData"][id]["name"].get<string>(), names[a]);
    if (a == 0) {
      EXPECT_EQ(result["peerData"][id]["endpoints"].size(), 2);
    } else {
      EXPECT_EQ(result["peerData"][id]["endpoints"].size(), 0);
    }
  }
}

TEST_F(PeerTest, TwoPeers) {
  LOG(INFO) << "STARTING GAME SERVER";
  initGameServer(2);
  LOG(INFO) << "CREATING PEERS";
  vector<shared_ptr<MyPeer>> peers = {
      shared_ptr<MyPeer>(new MyPeer(netEngine, names[0], keys[0].second, 11000,
                                    "localhost", 20000, names[0])),
      shared_ptr<MyPeer>(new MyPeer(netEngine, names[1], keys[1].second, 11001,
                                    "localhost", 20000, names[1])),
  };
  for (int a = 0; a < peers.size(); a++) {
    if (!a) {
      peers[a]->host("Starwars");
    } else {
      peers[a]->join();
    }
  }
  LOG(INFO) << "STARTING PEERS";
  for (auto it : peers) {
    it->start();
  }
  for (int a = 0; a < peers.size(); a++) {
    while (!peers[a]->initialized()) {
      LOG(INFO) << "Waiting for initialization for peer " << a << " ...";
      microsleep(1000 * 1000);
    }
  }
  peers[0]->updateState(200, {{"button0", "0"}});
  peers[1]->updateState(200, {{"button1", "1"}});
  unordered_map<string, string> state = peers[0]->getFullState(199);
  EXPECT_NE(state.find("button0"), state.end());
  EXPECT_EQ(state.find("button0")->second, "0");
  EXPECT_NE(state.find("button1"), state.end());
  EXPECT_EQ(state.find("button1")->second, "1");
  state = peers[1]->getFullState(199);
  EXPECT_NE(state.find("button0"), state.end());
  EXPECT_EQ(state.find("button0")->second, "0");
  EXPECT_NE(state.find("button1"), state.end());
  EXPECT_EQ(state.find("button1")->second, "1");
  microsleep(5 * 1000 * 1000);
  peers[0]->shutdown();
  for (auto &it : peers) {
    while (it->getLivingPeerCount()) {
      microsleep(1000 * 1000);
    }
  }
}

#if 0
// Doesn't terminate correctly yet
TEST_F(PeerTest, ThreePeers) {
  LOG(INFO) << "STARTING GAME SERVER";
  initGameServer(3);
  LOG(INFO) << "CREATING PEERS";
  vector<shared_ptr<MyPeer>> peers = {
      shared_ptr<MyPeer>(new MyPeer(netEngine, names[0], keys[0].second, 11000,
                                    "localhost", 20000, names[0])),
      shared_ptr<MyPeer>(new MyPeer(netEngine, names[1], keys[1].second, 11001,
                                    "localhost", 20000, names[1])),
      shared_ptr<MyPeer>(new MyPeer(netEngine, names[2], keys[2].second, 11002,
                                    "localhost", 20000, names[2])),
  };
  for (int a = 0; a < peers.size(); a++) {
    if (!a) {
      peers[a]->host("Starwars");
    } else {
      peers[a]->join();
    }
  }
  LOG(INFO) << "STARTING PEERS";
  for (auto it : peers) {
    it->start();
  }
  for (int a = 0; a < peers.size(); a++) {
    while (!peers[a]->initialized()) {
      LOG(INFO) << "Waiting for initialization for peer " << a << " ...";
      microsleep(1000 * 1000);
    }
  }
  peers[0]->updateState(200, {{"button0", "0"}});
  peers[1]->updateState(200, {{"button1", "1"}});
  peers[2]->updateState(200, {{"button2", "2"}});
  for (int a = 0; a < peers.size(); a++) {
    unordered_map<string, string> state = peers[a]->getFullState(199);
    EXPECT_NE(state.find("button0"), state.end());
    EXPECT_EQ(state.find("button0")->second, "0");
    EXPECT_NE(state.find("button1"), state.end());
    EXPECT_EQ(state.find("button1")->second, "1");
    EXPECT_NE(state.find("button2"), state.end());
    EXPECT_EQ(state.find("button2")->second, "2");
  }
  microsleep(1000 * 1000);
  peers[0]->shutdown();
  microsleep(1000 * 1000);
  peers[1]->shutdown();
  microsleep(1000 * 1000);
  for (auto &it : peers) {
    while (it->getLivingPeerCount()) {
      microsleep(1000 * 1000);
    }
  }
}
#endif
}  // namespace wga
