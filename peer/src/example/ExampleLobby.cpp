#include <cxxopts/include/cxxopts.hpp>

#include "CryptoHandler.hpp"
#include "Headers.hpp"
#include "LogHandler.hpp"
#include "MyPeer.hpp"
#include "NetEngine.hpp"
#include "PlayerData.hpp"
#include "SingleGameServer.hpp"

namespace wga {
class ExampleLobby {
 public:
  ExampleLobby() {}

  int run(int argc, char **argv) {
    srand(time(NULL));

    cxxopts::Options options("Peer", "Peer Program for WGA");
    options.add_options()  //
        ("port", "Port to run lobby on",
         cxxopts::value<int>()->default_value("20000"))  //
        ("v,verbose", "Log verbosity",
         cxxopts::value<int>()->default_value("0")  //
        );
    auto params = options.parse(argc, argv);

    // Setup easylogging configurations
    el::Configurations defaultConf = LogHandler::SetupLogHandler(&argc, &argv);
    defaultConf.setGlobally(el::ConfigurationType::ToFile, "false");
    el::Loggers::setVerboseLevel(params["v"].as<int>());

    // Reconfigure default logger to apply settings above
    el::Loggers::reconfigureLogger("default", defaultConf);

    shared_ptr<NetEngine> netEngine(new NetEngine());

    server.reset(new SingleGameServer(
        netEngine, params["port"].as<int>(), "Host",
        CryptoHandler::makePublicFromPrivate(
            CryptoHandler::stringToKey<PrivateKey>(
                "ZFhRa1dVWGhiQzc5UGt2YWkySFQ0RHZyQXpSYkxEdmg=")),
        "Host", 2));
    server->addPeer("Client",
                    CryptoHandler::makePublicFromPrivate(
                        CryptoHandler::stringToKey<PrivateKey>(
                            "M0JVZTd4aTN0M3dyUXdYQnlMNTdmQU1pRHZnaU9ITU8=")),
                    "Client");

    netEngine->start();

    while (true) {
      LOG(INFO) << "Running server...";
      microsleep(60 * 1000 * 1000);
    }
  }

  shared_ptr<SingleGameServer> server;
  shared_ptr<NetEngine> netEngine;
};
}  // namespace wga

int main(int argc, char **argv) {
  wga::ExampleLobby ExampleLobby;
  return ExampleLobby.run(argc, argv);
}
