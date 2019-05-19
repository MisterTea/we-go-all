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
    netEngine.reset(
        new NetEngine(shared_ptr<asio::io_service>(new asio::io_service())));
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
    server.reset(new SingleGameServer(20000, keys[0].first, names[0]));
    for (int a = 1; a < numPlayers; a++) {
      server->addPeer(keys[a].first, names[a]);
    }

    peerConnectionServer.reset(
        new PeerConnectionServer(netEngine, 20000, server));

    netEngine->start();

    this_thread::sleep_for(chrono::seconds(1));
  }

  void TearDown() override {
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
  string hostKey = CryptoHandler::keyToString(keys[0].first);

  string path = string("/api/get_current_game_id/") + hostKey;
  auto response = client.request("GET", path);
  json result = json::parse(response->content.string());
  uuid gameId = sole::rebuild(result["gameId"]);
  EXPECT_EQ(gameId.str(), server->getGameId().str());

  path = string("/api/host");
  json request = {
      {"hostKey", hostKey}, {"gameId", gameId.str()}, {"gameName", "Starwars"}};
  response = client.request("POST", path, request.dump(2));
  EXPECT_EQ(response->status_code, "200 OK");

  path = string("/api/get_game_info/") + gameId.str();
  response = client.request("GET", path);
  result = json::parse(response->content.string());
  EXPECT_EQ(result["gameName"], "Starwars");
  EXPECT_EQ(result["hostKey"], hostKey);
  for (int a = 0; a < 4; a++) {
    string stringKey = CryptoHandler::keyToString(keys[a].first);
    EXPECT_EQ(result["peerData"][stringKey]["key"].get<string>(), stringKey);
    EXPECT_EQ(result["peerData"][stringKey]["name"].get<string>(), names[a]);
    EXPECT_EQ(result["peerData"][stringKey]["endpoints"].size(), 0);
  }

  shared_ptr<udp::socket> localSocket(new udp::socket(
      *netEngine->getIoService(), udp::endpoint(udp::v4(), 12345)));
  udp::endpoint serverEndpoint = netEngine->resolve("127.0.0.1", "20000");
  string ipAddressPacket = hostKey + "_" + "192.168.0.1:" + to_string(12345);
  netEngine->getIoService()->post(
      [localSocket, serverEndpoint, ipAddressPacket]() {
        VLOG(1) << "IN SEND LAMBDA: " << ipAddressPacket.length();
        int bytesSent =
            localSocket->send_to(asio::buffer(ipAddressPacket), serverEndpoint);
        VLOG(1) << bytesSent << " bytes sent";
      });

  this_thread::sleep_for(chrono::seconds(1));

  path = string("/api/get_game_info/") + gameId.str();
  response = client.request("GET", path);
  result = json::parse(response->content.string());
  EXPECT_EQ(result["gameName"], "Starwars");
  EXPECT_EQ(result["hostKey"], hostKey);
  for (int a = 0; a < 4; a++) {
    string stringKey = CryptoHandler::keyToString(keys[a].first);
    EXPECT_EQ(result["peerData"][stringKey]["key"].get<string>(), stringKey);
    EXPECT_EQ(result["peerData"][stringKey]["name"].get<string>(), names[a]);
    if (a == 0) {
      EXPECT_EQ(result["peerData"][stringKey]["endpoints"].size(), 2);
    } else {
      EXPECT_EQ(result["peerData"][stringKey]["endpoints"].size(), 0);
    }
  }
}

TEST_F(PeerTest, TwoPeers) {
  LOG(INFO) << "STARTING GAME SERVER";
  initGameServer(2);
  LOG(INFO) << "CREATING PEERS";
  vector<shared_ptr<MyPeer>> peers = {
      shared_ptr<MyPeer>(
          new MyPeer(netEngine, keys[0].second, 11000, "localhost", 20000)),
      shared_ptr<MyPeer>(
          new MyPeer(netEngine, keys[1].second, 11001, "localhost", 20000)),
  };
  peers[0]->host("Starwars");
  LOG(INFO) << "STARTING PEERS";
  for (auto it : peers) {
    it->start();
  }
  for (int a = 0; a < peers.size(); a++) {
    while (!peers[a]->initialized()) {
      LOG(INFO) << "Waiting for initialization for peer " << a << " ...";
      sleep(1);
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
  for (auto it : peers) {
    it->shutdown();
  }
}

TEST_F(PeerTest, ThreePeers) {
  LOG(INFO) << "STARTING GAME SERVER";
  initGameServer(3);
  LOG(INFO) << "CREATING PEERS";
  vector<shared_ptr<MyPeer>> peers = {
      shared_ptr<MyPeer>(
          new MyPeer(netEngine, keys[0].second, 11000, "localhost", 20000)),
      shared_ptr<MyPeer>(
          new MyPeer(netEngine, keys[1].second, 11001, "localhost", 20000)),
      shared_ptr<MyPeer>(
          new MyPeer(netEngine, keys[2].second, 11002, "localhost", 20000)),
  };
  peers[0]->host("Starwars");
  LOG(INFO) << "STARTING PEERS";
  for (auto it : peers) {
    it->start();
  }
  for (int a = 0; a < peers.size(); a++) {
    while (!peers[a]->initialized()) {
      LOG(INFO) << "Waiting for initialization for peer " << a << " ...";
      sleep(1);
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
  for (auto it : peers) {
    it->shutdown();
  }
}
}  // namespace wga
