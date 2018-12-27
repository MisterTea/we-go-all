#include "MyPeer.hpp"

#include "CryptoHandler.hpp"

namespace wga {
MyPeer::MyPeer(shared_ptr<NetEngine> _netEngine, const PrivateKey &_privateKey,
               bool _host, int _serverPort)
    : netEngine(_netEngine),
      privateKey(_privateKey),
      host(_host),
      serverPort(_serverPort) {
  publicKey = CryptoHandler::makePublicFromPrivate(privateKey);
  publicKeyString = CryptoHandler::keyToString(publicKey);
  localSocket.reset(new udp::socket(*netEngine->getIoService(),
                                    udp::endpoint(udp::v4(), serverPort)));
  updateTimer.reset(new asio::steady_timer(*(netEngine->getIoService()),
                                           asio::chrono::milliseconds(10)));
  updateTimer->async_wait(std::bind(&MyPeer::start, this));
}

void MyPeer::shutdown() {
  updateTimer->cancel();
  updateTimer.reset();
  // Wait 1s for the updates to flush
  sleep(1);
}

void MyPeer::start() {
  client.reset(new HttpClient("localhost:30000"));

  string path = string("/get_current_game_id/") + publicKeyString;
  auto response = client->request("GET", path);
  FATAL_FAIL_HTTP(response);
  json result = json::parse(response->content.string());
  gameId = sole::rebuild(result["gameId"]);

  if (host) {
    path = string("/host");
    json request = {{"hostKey", publicKeyString},
                    {"gameId", gameId.str()},
                    {"gameName", "Starwars"}};
    response = client->request("POST", path, request.dump(2));
    FATAL_FAIL_HTTP(response);
  }

  updateTimer->async_wait(std::bind(&MyPeer::checkForEndpoints, this));

  udp::endpoint serverEndpoint = netEngine->resolve("127.0.0.1", "30000");
  string ipAddressPacket =
      publicKeyString + "_" + "127.0.0.1:" + to_string(serverPort);
  auto localSocketStack = localSocket;
  LOG(INFO) << "SENDING ENDPOINT";
  netEngine->getIoService()->post(
      [localSocketStack, serverEndpoint, ipAddressPacket]() {
        // TODO: Need mutex here
        LOG(INFO) << "IN SEND LAMBDA: " << ipAddressPacket.length();
        int bytesSent = localSocketStack->send_to(asio::buffer(ipAddressPacket),
                                                  serverEndpoint);
        LOG(INFO) << bytesSent << " bytes sent";
      });
}

void MyPeer::checkForEndpoints() {
  // LOG(INFO) << "CHECK FOR ENDPOINTS";
  // Bail if a peer doesn't have endpoints yet
  string path = string("/get_game_info/") + gameId.str();
  auto response = client->request("GET", path);
  FATAL_FAIL_HTTP(response);
  json result = json::parse(response->content.string());
  if (host) {
    if (result["gameName"] != "Starwars") {
      LOG(FATAL) << "Game Name does not match what I should be hosting: "
                 << result["gameName"] << " != "
                 << "Starwars";
    }
    if (result["hostKey"] != publicKeyString) {
      LOG(FATAL) << "Game host should be me but it isn't " << result["hostKey"]
                 << " != " << publicKeyString;
    }
  } else {
    gameName = result["gameName"];
    hostKey = CryptoHandler::stringToKey<PublicKey>(result["hostKey"]);
  }
  auto peerDataObject = result["peerData"];
  for (json::iterator it = peerDataObject.begin(); it != peerDataObject.end();
       ++it) {
    // std::cout << it.key() << " : " << it.value() << "\n";
    string peerKey = it.key();
    vector<udp::endpoint> endpoints;
    for (json::iterator it2 = it.value()["endpoints"].begin();
         it2 != it.value()["endpoints"].end(); ++it2) {
      string endpointString = *it2;
      vector<string> tokens = split(endpointString, ':');
      endpoints.push_back(netEngine->resolve(tokens.at(0), tokens.at(1)));
    }
    if (endpoints.empty()) {
      updateTimer->async_wait(std::bind(&MyPeer::checkForEndpoints, this));
      return;
    }
  }

  // Iterate over peer data and set up peers
  peerDataObject = result["peerData"];
  for (json::iterator it = peerDataObject.begin(); it != peerDataObject.end();
       ++it) {
    std::cout << it.key() << " : " << it.value() << "\n";
    PublicKey peerKey = CryptoHandler::stringToKey<PublicKey>(it.key());
    string name = it.value()["name"];
    peerData[peerKey] = shared_ptr<PlayerData>(new PlayerData(peerKey, name));
    if (peerKey == publicKey) {
      // Don't need to set up endpoint for myself
      continue;
    }
    vector<udp::endpoint> endpoints;
    for (json::iterator it2 = it.value()["endpoints"].begin();
         it2 != it.value()["endpoints"].end(); ++it2) {
      string endpointString = *it2;
      vector<string> tokens = split(endpointString, ':');
      endpoints.push_back(netEngine->resolve(tokens.at(0), tokens.at(1)));
    }
    shared_ptr<CryptoHandler> peerCryptoHandler(
        new CryptoHandler(privateKey, peerKey));
    peerHandlers[peerKey] =
        shared_ptr<MultiEndpointHandler>(new MultiEndpointHandler(
            localSocket, netEngine, peerCryptoHandler, endpoints));
  }

  this_thread::sleep_for(chrono::seconds(1));

  myData = peerData[publicKey];

  updateTimer->async_wait(std::bind(&MyPeer::update, this));
}

void MyPeer::update() {
  LOG(INFO) << "UPDATING";
  string path = string("/get_game_info/") + gameId.str();
  auto response = client->request("GET", path);
  auto result = json::parse(response->content.string());
  // Iterate over peer data and update peers
  auto peerDataObject = result["peerData"];
  for (json::iterator it = peerDataObject.begin(); it != peerDataObject.end();
       ++it) {
    std::cout << it.key() << " : " << it.value() << "\n";
    PublicKey peerKey = CryptoHandler::stringToKey<PublicKey>(it.key());
    if (peerKey == publicKey) {
      continue;
    }
    vector<udp::endpoint> endpoints;
    for (json::iterator it2 = it.value()["endpoints"].begin();
         it2 != it.value()["endpoints"].end(); ++it2) {
      string endpointString = *it2;
      vector<string> tokens = split(endpointString, ':');
      endpoints.push_back(netEngine->resolve(tokens.at(0), tokens.at(1)));
    }
    peerHandlers[peerKey]->updateEndpoints(endpoints);
  }

  if (updateTimer.get()) {
    updateTimer->async_wait(std::bind(&MyPeer::update, this));
  }
}
}  // namespace wga