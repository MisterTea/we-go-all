#include "MyPeer.hpp"

#include "CryptoHandler.hpp"
#include "EncryptedMultiEndpointHandler.hpp"

namespace wga {
MyPeer::MyPeer(shared_ptr<NetEngine> _netEngine, const PrivateKey& _privateKey,
               bool _host, int _serverPort)
    : netEngine(_netEngine),
      privateKey(_privateKey),
      host(_host),
      serverPort(_serverPort) {
  publicKey = CryptoHandler::makePublicFromPrivate(privateKey);
  publicKeyString = CryptoHandler::keyToString(publicKey);
  LOG(INFO) << "STARTING SERVER ON PORT: " << serverPort;
  localSocket.reset(new udp::socket(*netEngine->getIoService(),
                                    udp::endpoint(udp::v4(), serverPort)));
  asio::socket_base::reuse_address option(true);
  localSocket->set_option(option);
  rpcServer.reset(new RpcServer(netEngine, localSocket));
  LOG(INFO) << "STARTED SERVER ON PORT: " << serverPort;
}

void MyPeer::shutdown() {
  LOG(INFO) << "SHUTTING DOWN";
  {
    lock_guard<recursive_mutex> guard(*(netEngine->getMutex()));
    updateTimer->cancel();
    updateTimer.reset();
  }
  // Wait 1s for the updates to flush
  sleep(1);
  lock_guard<recursive_mutex> guard(*(netEngine->getMutex()));
  rpcServer.reset();
}

void MyPeer::start() {
  client.reset(new HttpClient("localhost:20000"));

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

  udp::endpoint serverEndpoint = netEngine->resolve("127.0.0.1", "20000");
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

  updateTimer.reset(new asio::steady_timer(
      *(netEngine->getIoService()),
      std::chrono::steady_clock::now() + std::chrono::seconds(1)));
  updateTimer->async_wait(
      std::bind(&MyPeer::checkForEndpoints, this, std::placeholders::_1));
  LOG(INFO) << "CALLING HEARTBEAT: "
            << std::chrono::duration_cast<std::chrono::seconds>(
                   updateTimer->expires_at().time_since_epoch())
                   .count()
            << " VS "
            << std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
}

void MyPeer::checkForEndpoints(const asio::error_code& error) {
  if (error == asio::error::operation_aborted) {
    return;
  }
  lock_guard<recursive_mutex> guard(peerDataMutex);

  // LOG(INFO) << "CHECK FOR ENDPOINTS";
  // Bail if a peer doesn't have endpoints yet
  string path = string("/get_game_info/") + gameId.str();
  auto response = client->request("GET", path);
  FATAL_FAIL_HTTP(response);
  json result = json::parse(response->content.string());
  LOG(INFO) << "GOT RESULT: " << result;
  if (host) {
    if (result["gameName"].get<string>() != "Starwars") {
      LOG(FATAL) << "Game Name does not match what I should be hosting: "
                 << result["gameName"].get<string>() << " != "
                 << "Starwars";
    }
    if (result["hostKey"].get<string>() != publicKeyString) {
      LOG(FATAL) << "Game host should be me but it isn't "
                 << result["hostKey"].get<string>()
                 << " != " << publicKeyString;
    }
  } else {
    gameName = result["gameName"].get<string>();
    hostKey =
        CryptoHandler::stringToKey<PublicKey>(result["hostKey"].get<string>());
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
      updateTimer->expires_at(updateTimer->expires_at() +
                              asio::chrono::milliseconds(1000));
      updateTimer->async_wait(
          std::bind(&MyPeer::checkForEndpoints, this, std::placeholders::_1));
      LOG(INFO) << "CALLING HEARTBEAT: "
                << std::chrono::duration_cast<std::chrono::seconds>(
                       updateTimer->expires_at().time_since_epoch())
                       .count()
                << " VS "
                << std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
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
    rpcServer->addEndpoint(peerKey, shared_ptr<EncryptedMultiEndpointHandler>(
                                        new EncryptedMultiEndpointHandler(
                                            localSocket, netEngine,
                                            peerCryptoHandler, endpoints)));
  }

  this_thread::sleep_for(chrono::seconds(1));

  myData = peerData[publicKey];

  update(asio::error_code());
}

void MyPeer::update(const asio::error_code& error) {
  if (error == asio::error::operation_aborted) {
    return;
  }
  lock_guard<recursive_mutex> guard(peerDataMutex);

  static int counter = 0;

  if (counter % 1000 == 0) {
    // Per-second update
    LOG(INFO) << "UPDATING";
    string path = string("/get_game_info/") + gameId.str();
    auto response = client->request("GET", path);
    auto result = json::parse(response->content.string());
    // Iterate over peer data and update peers
    auto peerDataObject = result["peerData"];
    for (json::iterator it = peerDataObject.begin(); it != peerDataObject.end();
         ++it) {
      // std::cout << it.key() << " : " << it.value() << "\n";
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

      auto endpointHandler = rpcServer->getEndpointHandler(peerKey);
      endpointHandler->updateEndpoints(endpoints);
    }

    LOG(INFO) << "CALLING HEARTBEAT: "
              << std::chrono::duration_cast<std::chrono::seconds>(
                     updateTimer->expires_at().time_since_epoch())
                     .count()
              << " VS "
              << std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::steady_clock::now().time_since_epoch())
                     .count();
    rpcServer->heartbeat();
  }

  for (const auto& it : peerData) {
    auto peerKey = it.first;
    if (peerKey == publicKey) {
      continue;
    }
    auto endpointHandler = rpcServer->getEndpointHandler(peerKey);
    if (endpointHandler->hasIncomingRequest()) {
      auto idPayload = endpointHandler->getFirstIncomingRequest();
      MessageReader reader;
      reader.load(idPayload.payload);
      int64_t startTime = reader.readPrimitive<int64_t>();
      int64_t endTime = reader.readPrimitive<int64_t>();
      map<string, string> m = reader.readMap<string, string>();
      LOG(INFO) << "GOT INPUTS: " << startTime << " " << endTime;
      unordered_map<string, string> mHashed(m.begin(), m.end());
      {
        lock_guard<recursive_mutex> guard(peerDataMutex);
        peerData[peerKey]->playerInputData.put(startTime, endTime, mHashed);
      }
      endpointHandler->reply(idPayload.id, "OK");
    }
  }

  counter++;

  updateTimer->expires_at(updateTimer->expires_at() +
                          asio::chrono::milliseconds(1));
  updateTimer->async_wait(
      std::bind(&MyPeer::update, this, std::placeholders::_1));
}

bool MyPeer::initialized() {
  if (myData.get() == NULL) {
    return false;
  }

  if (!rpcServer->readyToSend()) {
    return false;
  }

  return true;
}

void MyPeer::updateState(int64_t timestamp,
                         unordered_map<string, string> data) {
  lock_guard<recursive_mutex> guard(peerDataMutex);
  int64_t lastExpirationTime = myData->playerInputData.getExpirationTime();
  myData->playerInputData.put(lastExpirationTime, timestamp, data);
  LOG(INFO) << "CREATING CHRONOMAP FOR TIME: " << lastExpirationTime << " -> "
            << timestamp;
  MessageWriter writer;
  writer.start();
  writer.writePrimitive(lastExpirationTime);
  writer.writePrimitive(timestamp);
  map<string, string> m(data.begin(), data.end());
  writer.writeMap(m);
  string s = writer.finish();
  rpcServer->broadcast(s);
}

unordered_map<string, string> MyPeer::getFullState(int64_t timestamp) {
  unordered_map<string, string> state;
  for (auto& it : peerData) {
    it.second->playerInputData.blockUntilTime(timestamp);
    lock_guard<recursive_mutex> guard(peerDataMutex);
    unordered_map<string, string> peerState =
        it.second->playerInputData.getAll(timestamp);
    state.insert(peerState.begin(), peerState.end());
  }
  return state;
}

}  // namespace wga