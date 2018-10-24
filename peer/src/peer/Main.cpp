#include "Headers.hpp"

#include "CryptoHandler.hpp"
#include "CurlHandler.hpp"
#include "NetEngine.hpp"
#include "PlayerData.hpp"
#include "RpcServer.hpp"

DEFINE_int32(serverPort, 0, "Port to listen on");
DEFINE_bool(host, false, "True if hosting");
DEFINE_string(password, "", "Secret key for cryptography");

namespace wga {
class Main {
 public:
  Main() {}

  int run(int argc, char **argv) {
    srand(time(NULL));
    CryptoHandler::init();
    shared_ptr<NetEngine> netEngine(
        new NetEngine(shared_ptr<asio::io_service>(new asio::io_service())));
    shared_ptr<udp::socket> localSocket(
        new udp::socket(*netEngine->getIoService(),
                        udp::endpoint(udp::v4(), FLAGS_serverPort)));
    server.reset(new RpcServer(netEngine, localSocket));

    privateKey = CryptoHandler::stringToKey<PrivateKey>(FLAGS_password);
    publicKey = CryptoHandler::makePublicFromPrivate(privateKey);

    string gameId;

    {
      json gameIdRequest;
      gameIdRequest["publicKey"] = publicKey;

      auto gameIdResponseString = CurlHandler::post(
          "http://127.0.0.1:3000/get_game_id", gameIdRequest.dump(4));
      if (gameIdResponseString == nullopt) {
        LOG(FATAL) << "Could not make initial RPC to server";
      }
      json gameIdResponse = json::parse(*gameIdResponseString);
      gameId = gameIdResponse["gameId"];
    }

    if (FLAGS_host) {
      json hostInfo;
      hostInfo["gameName"] = "carpolo";
      hostInfo["publicKey"] = publicKey;
      hostInfo["gameId"] = gameId;

      auto hostData = CurlHandler::post("http://127.0.0.1:3000/set_game_name",
                                        hostInfo.dump(4));
      if (hostData == nullopt) {
        LOG(FATAL) << "Could not make initial RPC to server";
      }
      json hostJson = json::parse(*hostData);
    }

    json gameJson;
    json gameInfo;
    gameInfo["publicKey"] = publicKey;
    gameInfo["gameId"] = gameId;

    while (true) {
      auto gameInfoResult =
          CurlHandler::post("http://127.0.0.1:3000/gameinfo", gameInfo.dump(4));
      if (gameInfoResult == nullopt) {
        LOG(FATAL) << "Could not make initial RPC to server";
      }
      gameJson = json::parse(*gameInfoResult);
      if (gameJson["gameName"].get<string>().length()) {
        break;
      }
      usleep(1000 * 1000);
    }

    for (auto it = gameJson["players"].begin(); it != gameJson["players"].end();
         it++) {
      PublicKey playerKey = CryptoHandler::stringToKey<PublicKey>(it.key());
      string playerName = it.value()["name"];
      vector<string> endpointsString = it.value()["endpoints"];
      vector<udp::endpoint> endpoints;
      for (auto s : endpointsString) {
        auto tokens = split(s, ':');
        endpoints.push_back(netEngine->resolve(tokens[0], tokens[1]));
      }
      allPlayerData.insert(
          make_pair(playerKey, PlayerData(playerKey, playerName)));
      shared_ptr<CryptoHandler> cryptoHandler(
          new CryptoHandler(privateKey, playerKey));
      shared_ptr<MultiEndpointHandler> endpointHandler(new MultiEndpointHandler(
          localSocket, netEngine, cryptoHandler, endpoints));
      server->addEndpoint(endpointHandler);
    }

    netEngine->start();

    server->runUntilInitialized();

    auto lastUpdateTime = 0;
    while (true) {
      {
        lock_guard<recursive_mutex> guard(*netEngine->getMutex());
        auto currentTime = time(NULL);
        if (currentTime != lastUpdateTime) {
          currentTime = lastUpdateTime;

          // TODO: Get local ip addresses
          udp::endpoint serverEndpoint =
              netEngine->resolve("127.0.0.1", "3000");
          string localIpAddresses =
              "192.168.0.1:" + to_string(FLAGS_serverPort);
          netEngine->getIoService()->post(
              [localSocket, serverEndpoint, localIpAddresses]() {
                VLOG(1) << "IN SEND LAMBDA: " << localIpAddresses.length();
                int bytesSent = localSocket->send_to(
                    asio::buffer(localIpAddresses), serverEndpoint);
                VLOG(1) << bytesSent << " bytes sent";
              });

          // Make an RPC to get the peer data
          json guestInfo;
          guestInfo["publicKey"] = publicKey;

          // TODO: Make async
          auto gameData = CurlHandler::post("http://127.0.0.1:3000/gameinfo",
                                            guestInfo.dump(4));
          if (gameData == nullopt) {
            LOG(FATAL) << "Rpc to server failed";
          }
          gameJson = json::parse(*gameData);

          for (auto it = gameJson["players"].begin();
               it != gameJson["players"].end(); it++) {
            PublicKey playerKey =
                CryptoHandler::stringToKey<PublicKey>(it.key());
            vector<string> endpointsString = it.value()["endpoints"];
            vector<udp::endpoint> endpoints;
            for (auto s : endpointsString) {
              auto tokens = split(s, ':');
              endpoints.push_back(netEngine->resolve(tokens[0], tokens[1]));
            }
            server->updateEndpoints(playerKey, endpoints);
          }

          server->heartbeat();
        }
      }
      // Wait before repeating
      usleep(3000 * 1000);
    }

    return 0;
  }

  shared_ptr<NetEngine> netEngine;
  PublicKey publicKey;
  PrivateKey privateKey;
  shared_ptr<RpcServer> server;
  map<PublicKey, PlayerData> allPlayerData;
};
}  // namespace wga

int main(int argc, char **argv) {
  wga::Main main;
  return main.run(argc, argv);
}
