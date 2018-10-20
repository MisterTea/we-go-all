#include "Headers.hpp"

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include "CryptoHandler.hpp"
#include "NetEngine.hpp"
#include "PortMultiplexer.hpp"

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
    shared_ptr<asio::io_service> ioService(new asio::io_service());
    shared_ptr<NetEngine> netEngine(new NetEngine(ioService));
    shared_ptr<udp::socket> localSocket(new udp::socket(
        *ioService, udp::endpoint(udp::v4(), FLAGS_serverPort)));
    server.reset(new PortMultiplexer(netEngine, localSocket));

    FATAL_IF_FALSE(Base64::Decode(&FLAGS_password[0], FLAGS_password.length(),
                                  (char *)privateKey.data(),
                                  privateKey.size()));
    publicKey = CryptoHandler::makePublicFromPrivate(privateKey);

    json gameJson;
    if (FLAGS_host) {
      json hostInfo;
      hostInfo["gameName"] = "carpolo";
      hostInfo["playerName"] = "DigitalGhost";
      hostInfo["publicKey"] = publicKey;

      gameJson =
          json::parse(post("http://127.0.0.1:3000/host", hostInfo.dump(4)));
    } else {
      json guestInfo;
      guestInfo["name"] = "DogFart";
      guestInfo["publicKey"] = publicKey;

      gameJson =
          json::parse(post("http://127.0.0.1:3000/join", guestInfo.dump(4)));
    }

    netEngine->start();

    while (true) {
      {
        lock_guard<recursive_mutex> guard(*netEngine->getMutex());
        // Make an RPC to get the peer data
        map<string, string> params = {{"id", gameJson["id"]}};
        json newGameJson =
            json::parse(get("http://127.0.0.1:3000/game", params));

        // TODO: Reconcile the game info with previous game info
        LOG(INFO) << newGameJson.dump(4);

        // Heartbeat any rpcs
        // for (auto rpc : rpcs) {
        //   rpc->heartbeat();
        // }
      }
      // Wait 1s before repeating
      usleep(1000 * 1000);
    }

    return 0;
  }

  string post(const string &url, const string &payload) {
    try {
      curlpp::Cleanup myCleanup;

      // Creation of the URL option.
      curlpp::Easy myRequest;
      myRequest.setOpt(new curlpp::options::Url(url));
      myRequest.setOpt(new curlpp::options::Verbose(true));

      std::list<std::string> header;
      header.push_back("Content-Type: application/json");
      myRequest.setOpt(new curlpp::options::HttpHeader(header));

      myRequest.setOpt(new curlpp::options::PostFields(payload));
      myRequest.setOpt(new curlpp::options::PostFieldSize(payload.length()));

      std::ostringstream os;
      os << myRequest;
      LOG(INFO) << os.str();
      return os.str();
    } catch (curlpp::RuntimeError &e) {
      std::cout << e.what() << std::endl;
    } catch (curlpp::LogicError &e) {
      std::cout << e.what() << std::endl;
    }

    return string();
  }

  string get(const string &url, map<string, string> queryParameters) {
    try {
      curlpp::Cleanup myCleanup;

      string urlWithParams = url + "?";
      bool first = true;
      for (auto &it : queryParameters) {
        if (!first) {
          urlWithParams += "&";
        }
        first = false;
        urlWithParams += it.first + "=" + it.second;
      }

      // Creation of the URL option.
      curlpp::Easy myRequest;
      myRequest.setOpt(new curlpp::options::Url(urlWithParams));
      myRequest.setOpt(new curlpp::options::Verbose(true));
      myRequest.setOpt(new curlpp::options::HttpGet(true));

      std::ostringstream os;
      os << myRequest;
      LOG(INFO) << os.str();
      return os.str();
    } catch (curlpp::RuntimeError &e) {
      std::cout << e.what() << std::endl;
    } catch (curlpp::LogicError &e) {
      std::cout << e.what() << std::endl;
    }

    return string();
  }

  shared_ptr<NetEngine> netEngine;
  PublicKey publicKey;
  PrivateKey privateKey;
  shared_ptr<PortMultiplexer> server;
};
}  // namespace wga

int main(int argc, char **argv) {
  wga::Main main;
  return main.run(argc, argv);
}
