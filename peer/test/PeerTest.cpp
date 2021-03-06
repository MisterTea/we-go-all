#include "CryptoHandler.hpp"
#include "Headers.hpp"
#include "MyPeer.hpp"
#include "NetEngine.hpp"
#include "SingleGameServer.hpp"
#include "TimeHandler.hpp"

#undef CHECK
#include "Catch2/single_include/catch2/catch.hpp"

namespace wga {
class PeerTest {
 public:
  PeerTest() {}

  void SetUp() {
    srand(uint32_t(time(NULL)));
    DISABLE_PORT_MAPPING = true;
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
    LOG(INFO) << "Spinning up sgs";
    server.reset(new SingleGameServer(netEngine, 20000, names[0], keys[0].first,
                                      names[0], numPlayers));
    LOG(INFO) << "Done";

    netEngine->start();

    this_thread::sleep_for(chrono::seconds(1));

    server->start();

    this_thread::sleep_for(chrono::seconds(1));
  }

  void TearDown() {
    netEngine->post([this] { server->shutdown(); });

    this_thread::sleep_for(chrono::seconds(1));

    netEngine->shutdown();

    this_thread::sleep_for(chrono::seconds(1));

    server.reset();
    netEngine.reset();
  }

  void runProtocolTest() {
    HttpClient client("localhost:20000");
    string hostKey = CryptoHandler::keyToString(keys[0].first);

    string path = string("/api/get_current_game_id/") + names[0];
    auto response = client.request("GET", path);
    json result = json::parse(response->content.string());
    string gameId = result["gameId"].get<string>();
    REQUIRE(gameId == server->getGameId());

    path = string("/api/host");
    json request = {
        {"hostId", names[0]}, {"gameId", gameId}, {"gameName", "Starwars"}};
    response = client.request("POST", path, request.dump(2));
    REQUIRE(response->status_code == "200 OK");

    for (int a = 1; a < 4; a++) {
      path = string("/api/join");
      json request = {{"peerId", names[a]},
                      {"name", names[a]},
                      {"peerKey", CryptoHandler::keyToString(keys[a].first)}};
      response = client.request("POST", path, request.dump(2));
      REQUIRE(response->status_code == "200 OK");
    }

    path = string("/api/get_game_info/") + gameId;
    response = client.request("GET", path);
    result = json::parse(response->content.string());
    REQUIRE(result["gameName"] == "Starwars");
    REQUIRE(result["hostId"] == names[0]);
    for (int a = 0; a < 4; a++) {
      string id = names[a];
      string stringKey = CryptoHandler::keyToString(keys[a].first);
      REQUIRE(result["peerData"][id]["id"].get<string>() == names[a]);
      REQUIRE(result["peerData"][id]["key"].get<string>() == stringKey);
      REQUIRE(result["peerData"][id]["name"].get<string>() == names[a]);
      REQUIRE(result["peerData"][id]["endpoints"].size() == 0);
    }

    for (int a = 0; a < 4; a++) {
      path = string("/api/update_endpoints");
      json request = {{"peerId", names[a]}, {"endpoints", {"127.0.0.1:12345"}}};
      response = client.request("POST", path, request.dump(2));
      REQUIRE(response->status_code == "200 OK");
    }

    path = string("/api/get_game_info/") + gameId;
    response = client.request("GET", path);
    result = json::parse(response->content.string());
    LOG(ERROR) << "GOT RESULT: " << result;
    REQUIRE(result["gameName"] == "Starwars");
    REQUIRE(result["hostId"] == names[0]);
    for (int a = 0; a < 4; a++) {
      string id = names[a];
      string stringKey = CryptoHandler::keyToString(keys[a].first);
      REQUIRE(result["peerData"][id]["id"].get<string>() == id);
      REQUIRE(result["peerData"][id]["key"].get<string>() == stringKey);
      REQUIRE(result["peerData"][id]["name"].get<string>() == names[a]);
      REQUIRE(result["peerData"][id]["endpoints"].size() == 1);
    }
  }

  void peerTest() {
    vector<shared_ptr<MyPeer>> peers;
    for (int a = 0; a < keys.size(); a++) {
      auto peer = shared_ptr<MyPeer>(new MyPeer(
          names[a], keys[a].second, 11000 + a, "localhost", 20000, names[a]));
      peers.push_back(peer);
    };
    for (int a = 0; a < int(peers.size()); a++) {
      if (!a) {
        peers[a]->host("Starwars");
      } else {
        peers[a]->join();
      }
    }
    LOG(INFO) << "STARTING PEERS: " << peers.size();
    for (auto it : peers) {
      it->start();
    }
    for (int a = 0; a < int(peers.size()); a++) {
      while (!peers[a]->initialized()) {
        LOG(INFO) << "Waiting for initialization for peer " << a << " ...";
        microsleep(1000 * 1000);
      }
    }
    for (int64_t timestamp = 200; timestamp < 4000; timestamp += 200) {
      LOG(INFO) << "UPDATING STATE: " << timestamp;
      for (int a = 0; a < int(peers.size()); a++) {
        peers[a]->updateState(
            timestamp,
            {{std::string("button") + std::to_string(a), std::to_string(a)}});
      }
      for (int a = 0; a < int(peers.size()); a++) {
        unordered_map<string, string> state =
            peers[a]->getFullState(timestamp - 1);
        for (int b = 0; b < peers.size(); b++) {
          auto buttonName = std::string("button") + std::to_string(a);
          REQUIRE(state.find(buttonName) != state.end());
          REQUIRE(state.find(buttonName)->second == std::to_string(a));
        }
      }
    }
    microsleep(10 * 1000 * 1000);
    for (int a = 0; a < int(peers.size()); a++) {
      peers[a]->shutdown();
      LOG(INFO) << "PEER SHUT DOWN";
    }
    LOG(INFO) << "ALL PEERS SHUT DOWN";
    for (auto &it : peers) {
      while (it->getLivingPeerCount()) {
        microsleep(1000 * 1000);
      }
    }
    LOG(INFO) << "NO PEERS ALIVE";
    microsleep(5 * 1000 * 1000);
  }

  vector<pair<PublicKey, PrivateKey>> keys;
  vector<string> names = {"A", "B", "C", "D"};
  shared_ptr<SingleGameServer> server;
  shared_ptr<NetEngine> netEngine;
};

TEST_CASE("ProtocolTest", "[ProtocolTest]") {
  PeerTest testClass;
  testClass.SetUp();
  testClass.initGameServer(4);

  testClass.runProtocolTest();

  testClass.TearDown();
}

TEST_CASE("PeerTest") {
  LOG(INFO) << "STARTING GAME SERVER";
  PeerTest testClass;
  testClass.SetUp();

  SECTION("TwoPeers") { testClass.initGameServer(2); }

  SECTION("FourPeers") { testClass.initGameServer(4); }

  SECTION("FourPeersFlaky") {
    GlobalClock::addNoise();
    ALL_RPC_FLAKY = true;
    testClass.initGameServer(4);
  }

  LOG(INFO) << "CREATING PEERS";
  testClass.peerTest();
  LOG(INFO) << "TEARING DOWN";
  testClass.TearDown();
}
}  // namespace wga
