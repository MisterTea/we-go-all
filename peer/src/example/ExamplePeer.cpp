#include "Headers.hpp"

#include "CryptoHandler.hpp"
#include "LogHandler.hpp"
#include "MyPeer.hpp"
#include "NetEngine.hpp"
#include "PeerConnectionServer.hpp"
#include "PlayerData.hpp"
#include "PortMappingHandler.hpp"
#include "SingleGameServer.hpp"
#include "TimeHandler.hpp"

#include <cxxopts/include/cxxopts.hpp>

namespace wga {
class ExamplePeer {
 public:
  ExamplePeer() {}

  int run(int argc, char **argv) {
    srand(time(NULL));

    cxxopts::Options options("Peer", "Peer Program for WGA");
    options.add_options()                                                    //
        ("localport", "Port to run local server on", cxxopts::value<int>())  //
        ("host", "True if hosting",
         cxxopts::value<bool>()->default_value("false"))  //
        ("selflobby", "True if hosting a self lobby",
         cxxopts::value<bool>()->default_value("false"))  //
        ("v,verbose", "Log verbosity",
         cxxopts::value<int>()->default_value("0"))  //
        ("lobbyhost", "Hostname of lobby server",
         cxxopts::value<string>()->default_value("localhost"))  //
        ("lobbyport", "Port of lobby server",
         cxxopts::value<int>()->default_value("20000"))  //
        ;
    auto params = options.parse(argc, argv);

    // Setup easylogging configurations
    el::Configurations defaultConf = LogHandler::SetupLogHandler(&argc, &argv);
    defaultConf.setGlobally(el::ConfigurationType::ToFile, "false");
    el::Loggers::setVerboseLevel(params["v"].as<int>());

    // Reconfigure default logger to apply settings above
    el::Loggers::reconfigureLogger("default", defaultConf);

    bool host = params["host"].as<bool>();

    if (host) {
      privateKey = CryptoHandler::stringToKey<PrivateKey>(
          "ZFhRa1dVWGhiQzc5UGt2YWkySFQ0RHZyQXpSYkxEdmg=");
      publicKey = CryptoHandler::makePublicFromPrivate(privateKey);
    } else {
      privateKey = CryptoHandler::stringToKey<PrivateKey>(
          "M0JVZTd4aTN0M3dyUXdYQnlMNTdmQU1pRHZnaU9ITU8=");
      publicKey = CryptoHandler::makePublicFromPrivate(privateKey);
    }

    shared_ptr<NetEngine> netEngine(new NetEngine());
    int lobbyPort = params["lobbyport"].as<int>();

    if (params["selflobby"].as<bool>()) {
      LOG(INFO) << "Running local lobby";
      server.reset(
          new SingleGameServer(netEngine, lobbyPort, "Host", publicKey, "Host", 2));
      peerConnectionServer.reset(
          new PeerConnectionServer(netEngine, lobbyPort, server));
    }

    myPeer.reset(new MyPeer(host ? "Host" : "Client", privateKey,
                            params["localport"].as<int>(),
                            params["lobbyhost"].as<string>(), lobbyPort,
                            host ? "Host" : "Client"));
    if (host) {
      myPeer->host("Starwars");
    } else {
      myPeer->join();
    }

    netEngine->start();
    myPeer->start();
    while (!myPeer->initialized()) {
      LOG(INFO) << "Waiting for initialization for peer...";
      microsleep(1000 * 1000);
    }
    microsleep(1000 * 1000);

    for (int expirationTime = 200; expirationTime < 1000 * 60 * 10;
         expirationTime += 200) {
      if (host) {
        LOG(INFO) << "UPDATING STATE " << expirationTime;
        myPeer->updateState(expirationTime,
                            {{"button0", std::to_string(expirationTime)}});
      } else {
        LOG(INFO) << "UPDATING STATE " << expirationTime;
        myPeer->updateState(expirationTime,
                            {{"button1", std::to_string(expirationTime)}});
      }
      auto expirationTimeString = std::to_string(expirationTime);
      unordered_map<string, string> state =
          myPeer->getFullState(expirationTime - 1);
      auto it = state.find("button0");
      if (it == state.end()) {
        LOGFATAL << "MISSING BUTTON";
      }
      if (it->second != expirationTimeString) {
        LOGFATAL << "Button is the wrong state: " << it->second
                 << " != " << expirationTimeString;
      }
      it = state.find("button1");
      if (it == state.end()) {
        LOGFATAL << "MISSING BUTTON";
      }
      if (it->second != expirationTimeString) {
        LOGFATAL << "Button is the wrong state: " << it->second
                 << " != " << expirationTimeString;
      }
    }

    myPeer->finish();
    netEngine->shutdown();
    myPeer->shutdown();
    myPeer.reset();
    netEngine.reset();

    return 0;
  }

  PublicKey publicKey;
  PrivateKey privateKey;
  shared_ptr<MyPeer> myPeer;
  shared_ptr<SingleGameServer> server;
  shared_ptr<PeerConnectionServer> peerConnectionServer;
};
}  // namespace wga

int main(int argc, char **argv) {
  wga::ExamplePeer examplePeer;
  return examplePeer.run(argc, argv);
}
