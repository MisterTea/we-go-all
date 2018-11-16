#include "Headers.hpp"

#include "CryptoHandler.hpp"
#include "LogHandler.hpp"
#include "NetEngine.hpp"
#include "PlayerData.hpp"
#include "RpcServer.hpp"
#include "TimeHandler.hpp"

#include <cxxopts.hpp>

namespace wga {
class Main {
 public:
  Main() {}

  int run(int argc, char **argv) {
    srand(time(NULL));
    CryptoHandler::init();
    TimeHandler::init();

    cxxopts::Options options("Peer", "Peer Program for WGA");
    options.add_options()
      ("serverport", "Port to run server on", cxxopts::value<int>())
      ("host", "True if hosting", cxxopts::value<bool>()->default_value("false"))
      ("password", "Secret key for cryptography", cxxopts::value<std::string>())
      ("v,verbose", "Log verbosity", cxxopts::value<int>()->default_value("0"))
    ;
    auto params = options.parse(argc, argv);

    // Setup easylogging configurations
    el::Configurations defaultConf = LogHandler::SetupLogHandler(&argc, &argv);
    defaultConf.setGlobally(el::ConfigurationType::ToFile, "false");
    el::Loggers::setVerboseLevel(params["v"].as<int>());

    // Reconfigure default logger to apply settings above
    el::Loggers::reconfigureLogger("default", defaultConf);

    shared_ptr<NetEngine> netEngine(
        new NetEngine(shared_ptr<asio::io_service>(new asio::io_service())));
    shared_ptr<udp::socket> localSocket(
        new udp::socket(*netEngine->getIoService(),
                        udp::endpoint(udp::v4(), params["serverport"].as<int>())));
    server.reset(new RpcServer(netEngine, localSocket));

    privateKey = CryptoHandler::stringToKey<PrivateKey>(params["password"].as<string>());
    publicKey = CryptoHandler::makePublicFromPrivate(privateKey);

    string gameId;

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
