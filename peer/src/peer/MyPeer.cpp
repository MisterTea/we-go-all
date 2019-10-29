#include "MyPeer.hpp"

#include "CryptoHandler.hpp"
#include "EncryptedMultiEndpointHandler.hpp"
#include "LocalIpFetcher.hpp"
#include "StunClient.hpp"

#define INPUT_SEND_WINDOW_SIZE (3)

namespace wga {
MyPeer::MyPeer(shared_ptr<NetEngine> _netEngine, const string& _userId,
               const PrivateKey& _privateKey, int _serverPort,
               const string& _lobbyHost, int _lobbyPort, const string& _name)
    : userId(_userId),
      privateKey(_privateKey),
      shuttingDown(false),
      netEngine(_netEngine),
      serverPort(_serverPort),
      lobbyHost(_lobbyHost),
      lobbyPort(_lobbyPort),
      name(_name),
      timeShiftInitialized(false),
      updateCounter(0) {
  {
    // Use STUN to get public IPs
    LOG(INFO) << "GETTTING PUBLIC IPs";

    using namespace std::chrono_literals;

    using Iterator = udp::resolver::iterator;

    asio::io_service ios;
    udp::resolver resolver(ios);
    asio::steady_timer timer(ios);

    struct Stun {
      std::string url;
      std::string port;
    };

    vector<Stun> stuns = {
        {
            "stun.l.google.com",
            "19302",
        },
        {
            "stun1.l.google.com",
            "19302",
        },
    };

    udp::socket socket_to_reflect(ios, udp::endpoint(udp::v4(), serverPort));
    auto stun_client =
        std::unique_ptr<StunClient>(new StunClient(socket_to_reflect));

    constexpr int N = 2;
    int wait_for = N;

    for (const auto& stun : stuns) {
      udp::resolver::query q(stun.url, stun.port);
      resolver.async_resolve(q, [&](error_code e, Iterator iter) {
        if (e || iter == Iterator()) {
          if (e != asio::error::operation_aborted) {
            cout << "Can't resolve " << stun.url << " " << e.message() << endl;
          }
          return;
        }

        stun_client->reflect(*iter, [&](error_code e,
                                        udp::endpoint reflective_ep) {
          if (e.value()) {
            cout << "ERROR: " << stun.url << ": " << e.message() << " "
                 << reflective_ep << endl;
          } else {
            cout << "FINISHED: " << stun.url << ": " << reflective_ep << endl;
            stunEndpoints.insert(reflective_ep);
          }

          if (!e && --wait_for == 0) {
            timer.cancel();
            stun_client.reset();
            resolver.cancel();
          }
        });
      });
    }

    timer.expires_from_now(5s);
    timer.async_wait([&](error_code ec) {
      stun_client.reset();
      resolver.cancel();
    });

    ios.run();

    if (wait_for != 0) {
      std::cerr << "stun_client test failed: make sure at least " << N
                << " stun servers are running on the following addresses."
                << std::endl;
      for (const auto& stun : stuns) {
        std::cerr << "    " << stun.url << ":" << stun.port << std::endl;
      }
    } else {
      int port = -1;
      for (const auto& it : stunEndpoints) {
        if (port == -1) {
          port = it.port();
        } else {
          if (port != it.port()) {
            LOG(ERROR) << "Stun returned two different ports: Likely a "
                          "symmetric NAT.  It may be difficult to play games.";
          }
        }
      }
    }
  }

  publicKey = CryptoHandler::makePublicFromPrivate(_privateKey);
  LOG(INFO) << "STARTING SERVER ON PORT: " << serverPort;
  localSocket.reset(netEngine->startUdpServer(serverPort));
  asio::socket_base::reuse_address option(true);
  localSocket->set_option(option);
  rpcServer.reset(new RpcServer(netEngine, localSocket));
  LOG(INFO) << "STARTED SERVER ON PORT: " << serverPort;

  client.reset(new HttpClient(lobbyHost + ":" + to_string(lobbyPort)));

  LOG(INFO) << "GETTING GAME ID";
  string path = string("/api/get_current_game_id/") + userId;
  auto response = client->request("GET", path);
  FATAL_FAIL_HTTP(response);
  json result = json::parse(response->content.string());
  gameId = result["gameId"].get<string>();
  LOG(INFO) << "GOT GAME ID: " << gameId;

  {
    string path = string("/api/get_game_info/") + gameId;
    auto response = client->request("GET", path);
    FATAL_FAIL_HTTP(response);
    json result = json::parse(response->content.string());
    hosting = (result["hostId"].get<string>() == userId);
  }
}

void MyPeer::shutdown() {
  if (!shuttingDown) {
    LOG(INFO) << "SHUTTING DOWN";
    while (rpcServer->hasWork()) {
      LOG(INFO) << "WAITING FOR WORK TO FLUSH";
      microsleep(1000 * 1000);
    }
    shuttingDown = true;
    rpcServer->finish();
    // Wait for the updates to flush
    microsleep(1000 * 1000);
    LOG(INFO) << "BEGINNING FLUSH";
    while (rpcServer->hasWork()) {
      LOG(INFO) << "WAITING FOR WORK TO FLUSH";
      microsleep(1000 * 1000);
    }
    netEngine->post([this]() { updateTimer->cancel(); });
    // Wait for the updates to flush
    microsleep(1000 * 1000);
    rpcServer->closeSocket();
    // Wait for the updates to flush
    microsleep(1000 * 1000);
    rpcServer.reset();
  }
}

void MyPeer::host(const string& gameName) {
  string path = string("/api/host");
  json request = {
      {"hostId", userId}, {"gameId", gameId}, {"gameName", gameName}};
  SimpleWeb::CaseInsensitiveMultimap header;
  header.insert(make_pair("Content-Type", "application/json"));
  auto response = client->request("POST", path, request.dump(2), header);
  FATAL_FAIL_HTTP(response);
}

void MyPeer::join() {
  string path = string("/api/join");
  json request = {{"peerId", userId},
                  {"name", name},
                  {"peerKey", CryptoHandler::keyToString(publicKey)}};
  SimpleWeb::CaseInsensitiveMultimap header;
  header.insert(make_pair("Content-Type", "application/json"));
  auto response = client->request("POST", path, request.dump(2), header);
  FATAL_FAIL_HTTP(response);
}

void MyPeer::start() {
  updateEndpointServer();

  updateTimer.reset(netEngine->createTimer(std::chrono::steady_clock::now() +
                                           std::chrono::seconds(1)));
  updateTimer->async_wait(
      std::bind(&MyPeer::checkForEndpoints, this, std::placeholders::_1));
  LOG(INFO) << "CALLING HEARTBEAT: "
            << std::chrono::duration_cast<std::chrono::seconds>(
                   updateTimer->expires_at().time_since_epoch())
                   .count()
            << " VS "
            << std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::high_resolution_clock::now().time_since_epoch())
                   .count();
}

void MyPeer::updateEndpointServer() {
  auto serverEndpoints = netEngine->resolve(lobbyHost, to_string(lobbyPort));
  auto serverEndpoint = serverEndpoints[rand() % serverEndpoints.size()];
  auto localIps = LocalIpFetcher::fetch(serverPort, true);
  string ipAddressPacket = userId;
  for (const auto& stunIp : stunEndpoints) {
    ipAddressPacket +=
        "_" + stunIp.address().to_string() + ":" + to_string(stunIp.port());
  }
  for (auto it : localIps) {
    ipAddressPacket += "_" + it + ":" + to_string(serverPort);
  }
  auto localSocketStack = localSocket;
  LOG(INFO) << "SENDING ENDPOINT PACKET: " << ipAddressPacket;
  netEngine->post([localSocketStack, serverEndpoint, ipAddressPacket]() {
    LOG(INFO) << "IN SEND LAMBDA: " << ipAddressPacket.length();
    int bytesSent = localSocketStack->send_to(asio::buffer(ipAddressPacket),
                                              serverEndpoint);
    LOG(INFO) << bytesSent << " bytes sent";
  });
}

void MyPeer::checkForEndpoints(const asio::error_code& error) {
  LOG(INFO) << "CHECKING FOR ENDPOINTS";
  if (error == asio::error::operation_aborted) {
    return;
  }
  LOG(INFO) << "CHECKING FOR ENDPOINTS";
  lock_guard<recursive_mutex> guard(peerDataMutex);
  LOG(INFO) << "CHECKING FOR ENDPOINTS";

  updateEndpointServer();

  // LOG(INFO) << "CHECK FOR ENDPOINTS";
  // Bail if a peer doesn't have endpoints yet
  string path = string("/api/get_game_info/") + gameId;
  auto response = client->request("GET", path);
  FATAL_FAIL_HTTP(response);
  json result = json::parse(response->content.string());
  LOG(INFO) << "GOT RESULT: " << result;
  if (!result["ready"].get<bool>()) {
    updateTimer->expires_at(updateTimer->expires_at() +
                            asio::chrono::milliseconds(1000));
    updateTimer->async_wait(
        std::bind(&MyPeer::checkForEndpoints, this, std::placeholders::_1));
    LOG(INFO) << "Game is not ready, waiting...";
    return;
  }

  if (gameName.length()) {
    if (result["gameName"].get<string>() != gameName) {
      LOGFATAL << "Game Name does not match what I should be hosting: "
               << result["gameName"].get<string>() << " != " << gameName;
    }
    if (result["hostId"].get<string>() != userId) {
      LOGFATAL << "Game host should be me but it isn't "
               << result["hostId"].get<string>() << " != " << userId;
    }
  } else {
    gameName = result["gameName"].get<string>();
    hostId = result["hostId"].get<string>();
  }

  // Iterate over peer data and set up peers
  auto peerDataObject = result["peerData"];
  for (json::iterator it = peerDataObject.begin(); it != peerDataObject.end();
       ++it) {
    VLOG(1) << it.key() << " : " << it.value();
    string id = it.key();
    PublicKey peerKey =
        CryptoHandler::stringToKey<PublicKey>(it.value()["key"]);
    string name = it.value()["name"];
    peerData[id] = shared_ptr<PlayerData>(new PlayerData(peerKey, name));
    if (id == userId) {
      // Don't need to set up endpoint for myself
      continue;
    }
    vector<udp::endpoint> endpoints;
    for (json::iterator it2 = it.value()["endpoints"].begin();
         it2 != it.value()["endpoints"].end(); ++it2) {
      string endpointString = *it2;
      vector<string> tokens = split(endpointString, ':');
      auto newEndpoints = netEngine->resolve(tokens.at(0), tokens.at(1));
      for (auto newEndpoint : newEndpoints) {
        endpoints.push_back(newEndpoint);
      }
    }
    shared_ptr<CryptoHandler> peerCryptoHandler(
        new CryptoHandler(privateKey, peerKey));
    shared_ptr<EncryptedMultiEndpointHandler> endpointHandler(
        new EncryptedMultiEndpointHandler(localSocket, netEngine,
                                          peerCryptoHandler, endpoints));
    rpcServer->addEndpoint(id, endpointHandler);
  }

  // this_thread::sleep_for(chrono::seconds(1));

  myData = peerData[userId];

  update(asio::error_code());
}

void MyPeer::update(const asio::error_code& error) {
  if (error == asio::error::operation_aborted) {
    LOG(ERROR) << "PEER UPDATE FINISHED";
    return;
  }
  if (error) {
    LOG(FATAL) << "Peer Update error: " << error.message();
  }
  lock_guard<recursive_mutex> guard(peerDataMutex);

  if (updateCounter % 1000 == 0) {
    // Per-second update
    updateEndpointServer();
    LOG(INFO) << "UPDATING";
    string path = string("/api/get_game_info/") + gameId;
    auto response = client->request("GET", path);
    auto result = json::parse(response->content.string());
    // Iterate over peer data and update peers
    auto peerDataObject = result["peerData"];
    for (json::iterator it = peerDataObject.begin(); it != peerDataObject.end();
         ++it) {
      std::cout << it.key() << " : " << it.value() << "\n";
      if (it.key() == userId) {
        continue;
      }
      vector<udp::endpoint> endpoints;
      for (json::iterator it2 = it.value()["endpoints"].begin();
           it2 != it.value()["endpoints"].end(); ++it2) {
        string endpointString = *it2;
        vector<string> tokens = split(endpointString, ':');
        auto newEndpoints = netEngine->resolve(tokens.at(0), tokens.at(1));
        for (auto newEndpoint : newEndpoints) {
          endpoints.push_back(newEndpoint);
        }
      }

      auto endpointHandler = rpcServer->getEndpointHandler(it.key());
      endpointHandler->addEndpoints(endpoints);
    }
  }

  if (updateCounter % 100 == 0) {
    VLOG(1) << "CALLING HEARTBEAT";
    rpcServer->heartbeat();
  }

  for (const auto& it : peerData) {
    auto peerKey = it.first;
    if (peerKey == userId) {
      continue;
    }
    auto endpointHandler = rpcServer->getEndpointHandler(peerKey);
    while (endpointHandler->hasIncomingRequest()) {
      auto idPayload = endpointHandler->getFirstIncomingRequest();
      MessageReader reader;
      reader.load(idPayload.payload);
      for (int a = 0; a < INPUT_SEND_WINDOW_SIZE; a++) {
        int64_t startTime = reader.readPrimitive<int64_t>();
        int64_t endTime = reader.readPrimitive<int64_t>();
        unordered_map<string, string> m =
            reader.readMap<unordered_map<string, string>>();
        VLOG(1) << "GOT INPUTS: " << peerKey << " " << startTime << " "
                << endTime;
        {
          lock_guard<recursive_mutex> guard(peerDataMutex);
          peerData[peerKey]->playerInputData.put(startTime, endTime, m);
        }
      }
      endpointHandler->replyOneWay(idPayload.id);
    }
    while (endpointHandler->hasIncomingReply()) {
      auto idPayload = endpointHandler->getFirstIncomingReply();
      // We don't need to handle replies
    }
  }

  updateCounter++;

  if (!shuttingDown) {
    updateTimer->expires_at(updateTimer->expires_at() +
                            asio::chrono::milliseconds(1));
    updateTimer->async_wait(
        std::bind(&MyPeer::update, this, std::placeholders::_1));
  } else {
    LOG(ERROR) << "Shutting down, stopping updates";
  }
}

bool MyPeer::initialized() {
  if (myData.get() == NULL) {
    return false;
  }

  if (!rpcServer->readyToSend()) {
    return false;
  }

  if (!timeShiftInitialized) {
    timeShiftInitialized = true;
    for (const auto& it : peerData) {
      auto peerKey = it.first;
      if (peerKey == userId) {
        continue;
      }
      auto endpointHandler = rpcServer->getEndpointHandler(peerKey);
      endpointHandler->initTimeShift();
    }
  }

  return true;
}

vector<string> MyPeer::getAllInputValues(int64_t timestamp, const string& key) {
  vector<string> values;
  for (auto& it : peerData) {
    while (true) {
      if (it.second->playerInputData.waitForExpirationTime(timestamp)) {
        lock_guard<recursive_mutex> guard(peerDataMutex);
        values.push_back(it.second->playerInputData.getOrDie(timestamp, key));
        break;
      }
      // Check if the peer is dead
      auto peerId = it.first;
      if (rpcServer->isPeerShutDown(peerId)) {
        break;
      }
    }
  }
  return values;
}

void MyPeer::updateState(int64_t timestamp,
                         const unordered_map<string, string>& data) {
  lock_guard<recursive_mutex> guard(peerDataMutex);
  int64_t lastExpirationTime = myData->playerInputData.getExpirationTime();
  myData->playerInputData.put(lastExpirationTime, timestamp, data);
  lastSendBuffer.push_front(make_tuple(lastExpirationTime, timestamp, data));
  while (lastSendBuffer.size() > INPUT_SEND_WINDOW_SIZE) {
    lastSendBuffer.pop_back();
  }
  VLOG(1) << "CREATING CHRONOMAP FOR TIME: " << lastExpirationTime << " -> "
          << timestamp;
  MessageWriter writer;
  writer.start();
  int numMaps = 0;
  for (const auto& it : lastSendBuffer) {
    writer.writePrimitive(get<0>(it) /*lastExpirationTime*/);
    writer.writePrimitive(get<1>(it) /*timestamp*/);
    writer.writeMap(get<2>(it) /*data*/);
    numMaps++;
  }
  for (int a = numMaps; a < INPUT_SEND_WINDOW_SIZE; a++) {
    writer.writePrimitive(get<0>(lastSendBuffer.back()) /*lastExpirationTime*/);
    writer.writePrimitive(get<1>(lastSendBuffer.back()) /*timestamp*/);
    writer.writeMap(get<2>(lastSendBuffer.back()) /*data*/);
    numMaps++;
  }
  string s = writer.finish();
  rpcServer->broadcast(s);
}

unordered_map<string, string> MyPeer::getFullState(int64_t timestamp) {
  unordered_map<string, string> state;
  for (auto& it : peerData) {
    while (true) {
      lock_guard<recursive_mutex> guard(peerDataMutex);
      auto expirationTime = it.second->playerInputData.getExpirationTime();
      if (timestamp < expirationTime) {
        VLOG(1) << "GOT STATE: " << it.first << " " << timestamp << " "
                << expirationTime;
        unordered_map<string, string> peerState =
            it.second->playerInputData.getAll(timestamp);
        state.insert(peerState.begin(), peerState.end());
        break;
      }
      // Check if the peer is dead
      auto peerId = it.first;
      if (rpcServer->isPeerShutDown(peerId)) {
        break;
      }
    }
  }
  return state;
}

int64_t MyPeer::getNearestExpirationTime() {
  int64_t retval =
      peerData.begin()->second->playerInputData.getExpirationTime();
  for (auto& it : peerData) {
    retval = min(retval, it.second->playerInputData.getExpirationTime());
  }
  return retval;
}

}  // namespace wga