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
    CryptoHandler::init();
    TimeHandler::init();
    netEngine.reset(
        new NetEngine(shared_ptr<asio::io_service>(new asio::io_service())));
  }

  void initGameServer(int numPlayers) {
    for (int a = 0; a < numPlayers; a++) {
      keys.push_back(CryptoHandler::generateKey());
    }
    server.reset(new SingleGameServer(
        30000, CryptoHandler::keyToString(keys[0].first), names[0]));
    for (int a = 1; a < numPlayers; a++) {
      server->addPeer(CryptoHandler::keyToString(keys[a].first), names[a]);
    }

    peerConnectionServer.reset(
        new PeerConnectionServer(netEngine, 30000, server));

    netEngine->start();

    this_thread::sleep_for(chrono::seconds(1));
  }

  void TearDown() override {
    server.reset();
    peerConnectionServer.reset();
    netEngine->shutdown();
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
  HttpClient client("localhost:30000");
  string hostKey = CryptoHandler::keyToString(keys[0].first);

  string path = string("/get_current_game_id/") + hostKey;
  auto response = client.request("GET", path);
  json result = json::parse(response->content.string());
  uuid gameId = sole::rebuild(result["gameId"]);
  ASSERT_EQ(gameId.str(), server->getGameId().str());

  path = string("/host");
  json request = {
      {"hostKey", hostKey}, {"gameId", gameId.str()}, {"gameName", "Starwars"}};
  response = client.request("POST", path, request.dump(2));
  ASSERT_EQ(response->status_code, "200 OK");

  path = string("/get_game_info/") + gameId.str();
  response = client.request("GET", path);
  result = json::parse(response->content.string());
  ASSERT_EQ(result["gameName"], "Starwars");
  ASSERT_EQ(result["hostKey"], hostKey);
  for (int a = 0; a < 4; a++) {
    string stringKey = CryptoHandler::keyToString(keys[a].first);
    ASSERT_EQ(result["peerData"][stringKey]["key"].get<string>(), stringKey);
    ASSERT_EQ(result["peerData"][stringKey]["name"].get<string>(), names[a]);
    ASSERT_EQ(result["peerData"][stringKey]["endpoints"].size(), 0);
  }

  shared_ptr<udp::socket> localSocket(new udp::socket(
      *netEngine->getIoService(), udp::endpoint(udp::v4(), 12345)));
  udp::endpoint serverEndpoint = netEngine->resolve("127.0.0.1", "30000");
  string ipAddressPacket = hostKey + "_" + "192.168.0.1:" + to_string(12345);
  netEngine->getIoService()->post(
      [localSocket, serverEndpoint, ipAddressPacket]() {
        VLOG(1) << "IN SEND LAMBDA: " << ipAddressPacket.length();
        int bytesSent =
            localSocket->send_to(asio::buffer(ipAddressPacket), serverEndpoint);
        VLOG(1) << bytesSent << " bytes sent";
      });

  this_thread::sleep_for(chrono::seconds(1));

  path = string("/get_game_info/") + gameId.str();
  response = client.request("GET", path);
  result = json::parse(response->content.string());
  ASSERT_EQ(result["gameName"], "Starwars");
  ASSERT_EQ(result["hostKey"], hostKey);
  for (int a = 0; a < 4; a++) {
    string stringKey = CryptoHandler::keyToString(keys[a].first);
    ASSERT_EQ(result["peerData"][stringKey]["key"].get<string>(), stringKey);
    ASSERT_EQ(result["peerData"][stringKey]["name"].get<string>(), names[a]);
    if (a == 0) {
      ASSERT_EQ(result["peerData"][stringKey]["endpoints"].size(), 2);
    } else {
      ASSERT_EQ(result["peerData"][stringKey]["endpoints"].size(), 0);
    }
  }
}

TEST_F(PeerTest, SinglePeer) {
  initGameServer(1);
  shared_ptr<MyPeer> firstPeer(
      new MyPeer(netEngine, keys[0].second, true, 12345));
  firstPeer->start();
  while (!firstPeer->initialized()) {
    LOG(INFO) << "Waiting for initialization...";
    sleep(1);
  }
  firstPeer->updateState(200, {{"button", "0"}});
  sleep(3);
  firstPeer->shutdown();
}
}  // namespace wga
