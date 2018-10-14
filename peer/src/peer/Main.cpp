#include "Headers.hpp"

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>

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
    ioService = shared_ptr<asio::io_service>(new asio::io_service());
    ioServiceMutex.reset(new mutex());
    shared_ptr<udp::socket> localSocket(new udp::socket(
        *ioService, udp::endpoint(udp::v4(), FLAGS_serverPort)));
    server = portMultiplexer(
        new PortMultiplexer(ioService, ioServiceMutex, localSocket));

    FATAL_IF_FALSE(Base64::Decode(&FLAGS_password[0], FLAGS_password.length(),
                                  privateKey.data(), privateKey.size()));

    json gameStatus;
    if (FLAGS_host) {
      string hostInfo = R"(
        {
          "gameName": "carpolo",
          "playerName": "DigitalGhost"
        }
      )";
      string signedHostInfoBinary = CryptoHandler::sign(privateKey, hostInfo);
      string signedHostInfoB64;
      FATAL_IF_FALSE(Base64::Encode(signedHostInfoBinary, &signedHostInfoB64));

      gameStatus = post("http://127.0.0.1:8080/host", signedHostInfoB64)._json;
    } else {
      string guestInfo = R"(
        {
          "playerName": "DogFart",
          "hostName": "DigitalGhost"
        }
      )";
      string signedGuestInfoBinary = CryptoHandler::sign(privateKey, guestInfo);
      string signedGuestInfoB64;
      FATAL_IF_FALSE(
          Base64::Encode(signedGuestInfoBinary, &signedGuestInfoB64));
      string publicKeyB64;
      FATAL_IF_FALSE(Base64::Encode(string(publicKey.data(), publicKey.size()),
                                    &publicKeyB64));

      gameId = post("http://127.0.0.1:8080/guest",
                    publicKeyB64 + "/" + signedGuestInfoB64);
    }

    ioServiceThread = std::thread([this]() { this->ioService->run(); });

    while (true) {
      {
        lock_guard<std::mutex> guard(*ioServiceMutex);
        // Make an RPC to get the peer data
        json gameInfo =
            getJson("http://127.0.0.1:8080/gameinfo", {"gameId" : gameId});

        // TODO: Reconcile the game info with previous game info

        // Heartbeat any rpcs
        for (auto rpc : rpcs) {
          rpc->heartbeat();
        }
      }
      // Wait 1s before repeating
      usleep(1000 * 1000);
    }

    try {
      curlpp::Cleanup myCleanup;

      // Creation of the URL option.
      curlpp::Easy myRequest;
      myRequest.setOpt(
          new curlpp::options::Url(std::string("https://example.com")));
      myRequest.setOpt(new curlpp::options::SslEngineDefault());

      std::ostringstream os;
      os << myRequest;
      cout << os.str() << endl;
    } catch (curlpp::RuntimeError &e) {
      std::cout << e.what() << std::endl;
    } catch (curlpp::LogicError &e) {
      std::cout << e.what() << std::endl;
    }

    return 0;
  }

  shared_ptr<asio::io_service> ioService;
  std::thread ioServiceThread;
  shared_ptr<mutex> ioServiceMutex;
  PublicKey publicKey;
  PrivateKey privateKey;
  shared_ptr<PortMultiplexer> server;
  vector<Peer> peers;
};
}  // namespace wga

int main(int argc, char **argv) {
  wga::Main main;
  return main.run(argc, argv);
}
