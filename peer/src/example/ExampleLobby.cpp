#include "Headers.hpp"

#include "CryptoHandler.hpp"
#include "LogHandler.hpp"
#include "MyPeer.hpp"
#include "NetEngine.hpp"
#include "PeerConnectionServer.hpp"
#include "PlayerData.hpp"
#include "SingleGameServer.hpp"

#include <cxxopts.hpp>

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

    shared_ptr<NetEngine> netEngine(
        new NetEngine(shared_ptr<asio::io_service>(new asio::io_service())));

    server.reset(new SingleGameServer(
        params["port"].as<int>(),
        CryptoHandler::makePublicFromPrivate(
            CryptoHandler::stringToKey<PrivateKey>(
                "ZFhRa1dVWGhiQzc5UGt2YWkySFQ0RHZyQXpSYkxEdmg=")),
        "Host"));
    server->addPeer(CryptoHandler::makePublicFromPrivate(
                        CryptoHandler::stringToKey<PrivateKey>(
                            "M0JVZTd4aTN0M3dyUXdYQnlMNTdmQU1pRHZnaU9ITU8=")),
                    "Client");

    peerConnectionServer.reset(
        new PeerConnectionServer(netEngine, params["port"].as<int>(), server));

    netEngine->start();

    while (true) {
      LOG(INFO) << "Running server...";
      sleep(60);
    }
  }

  shared_ptr<SingleGameServer> server;
  shared_ptr<NetEngine> netEngine;
  shared_ptr<PeerConnectionServer> peerConnectionServer;
};
}  // namespace wga

int main(int argc, char **argv) {
  wga::ExampleLobby ExampleLobby;
  return ExampleLobby.run(argc, argv);
}
