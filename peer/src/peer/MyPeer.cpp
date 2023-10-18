#include "MyPeer.hpp"

#include "CryptoHandler.hpp"
#include "EncryptedMultiEndpointHandler.hpp"
#include "LocalIpFetcher.hpp"
#include "StunClient.hpp"

#define INPUT_SEND_WINDOW_SIZE (3)

namespace wga {
MyPeer::MyPeer(const string& _userId, const PrivateKey& _privateKey,
               int _serverPort, const string& _lobbyHost, int _lobbyPort,
               const string& _name)
    : userId(_userId),
      privateKey(_privateKey),
      shuttingDown(false),
      updateFinished(false),
      serverPort(_serverPort),
      lobbyHost(_lobbyHost),
      lobbyPort(_lobbyPort),
      name(_name),
      timeShiftInitialized(false),
      updateCounter(0),
      position(-1) {
  {
    netEngine.reset(new NetEngine());
    netEngine->start();

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
            "stun1.l.google.com",
            "19302",
        },
        {
            "stun2.l.google.com",
            "19302",
        },
    };

    udp::socket socket_to_reflect(ios, udp::endpoint(udp::v4(), serverPort));
    auto stun_client =
        std::unique_ptr<StunClient>(new StunClient(socket_to_reflect));

    constexpr int N = 2;
    int wait_for = N;

    for (const auto& stun : stuns) {
      udp::resolver::query q(udp::v4(), stun.url, stun.port);
      resolver.async_resolve(q, [&](error_code e, Iterator iter) {
        if (e || iter == Iterator()) {
          if (e != asio::error::operation_aborted) {
            LOG(INFO) << "Can't resolve " << stun.url << " " << e.message()
                      << endl;
          }
          return;
        }

        stun_client->reflect(
            *iter, [&](error_code e, udp::endpoint reflective_ep) {
              if (e.value()) {
                LOG(INFO) << "ERROR: " << stun.url << ": " << e.message() << " "
                          << reflective_ep << endl;
              } else {
                LOG(INFO) << "FINISHED: " << stun.url << ": " << reflective_ep
                          << endl;
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
      LOG(FATAL)
          << "Stun test failed.  Did not receive packets from STUN server";
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
  string publicKeyB64 = b64::Base64::Encode(
      std::string(std::begin(publicKey), std::end(publicKey)));
  LOG(INFO) << "STARTING SERVER ON PORT: " << serverPort;
  localSocket.reset(netEngine->startUdpServer(serverPort));
  try {
    asio::socket_base::reuse_address option(true);
    localSocket->set_option(option);
  } catch (...) {
    LOG(ERROR) << "Setting reuse failed.  Socket may be bocked after exiting";
  }
  rpcServer.reset(new RpcServer(netEngine, localSocket));
  LOG(INFO) << "STARTED SERVER ON PORT: " << serverPort;

  client.reset(new HttpClientMuxer(lobbyHost + ":" + to_string(lobbyPort)));

  LOG(INFO) << "GETTING GAME ID FROM " << lobbyHost << ":" << lobbyPort;
  string path = string("/api/get_current_game_id/") + userId;
  json result = client->request("GET", path);
  gameId = result["gameId"].get<string>();
  LOG(INFO) << "GOT GAME ID: " << gameId;

  LOG(INFO) << "Updated endpoints";

  {
    string path = string("/api/get_game_info/") + gameId;
    json result = client->request("GET", path);
    hosting = (result["hostId"].get<string>() == userId);
    hostId = result["hostId"].get<string>();
  }
}

void MyPeer::shutdown() {
  if (!shuttingDown) {
    LOG(INFO) << "SHUTTING DOWN";
    while (true) {
      {
        lock_guard<recursive_mutex> guard(peerDataMutex);
        if (!rpcServer->hasWork()) {
          break;
        }
      }
      LOG(INFO) << "WAITING FOR WORK TO FLUSH";
      microsleep(1000 * 1000);
    }
    rpcServer->sendShutdown();
    // Wait for the updates to flush
    microsleep(1000 * 1000);
    LOG(INFO) << "BEGINNING FLUSH";
    while (true) {
      {
        lock_guard<recursive_mutex> guard(peerDataMutex);
        if (!rpcServer->hasWork()) {
          break;
        }
      }
      LOG(INFO) << "WAITING FOR WORK TO FLUSH";
      microsleep(1000 * 1000);
    }
    shuttingDown = true;
    while (true) {
      {
        lock_guard<recursive_mutex> guard(peerDataMutex);
        if (updateFinished) {
          break;
        }
      }
      microsleep(1000 * 1000);
    }
    {
      lock_guard<recursive_mutex> guard(peerDataMutex);
      rpcServer->finish();
      rpcServer->closeSocket();
    }

    rpcServer.reset();
    client.reset();

    netEngine->post([this]() {
      localSocket.reset();
      updateTimer.reset();
      stunEndpoints.clear();
    });

    microsleep(1000 * 1000);
    netEngine->shutdown();
    netEngine.reset();
    LOG(INFO) << "SHUT DOWN FINISHED";
  }
}

void MyPeer::host(const string& gameName) {
  string path = string("/api/host");
  json request = {
      {"hostId", userId}, {"gameId", gameId}, {"gameName", gameName}};
  SimpleWeb::CaseInsensitiveMultimap header;
  header.insert(make_pair("Content-Type", "application/json"));
  json result = client->request("POST", path, request.dump(2), header);
}

void MyPeer::join() {
  string path = string("/api/join");
  json request = {{"peerId", userId},
                  {"name", name},
                  {"peerKey", CryptoHandler::keyToString(publicKey)}};
  SimpleWeb::CaseInsensitiveMultimap header;
  header.insert(make_pair("Content-Type", "application/json"));
  json result = client->request("POST", path, request.dump(2), header);
}

void MyPeer::getInitialPosition() {
  string path = string("/api/get_game_info/") + gameId;
  json result = client->request("GET", path);
  hosting = (result["hostId"].get<string>() == userId);
  auto hostId = result["hostId"].get<string>();
  position = -1;
  auto peerData = result["peerData"];
  string publicKeyB64 = b64::Base64::Encode(
      std::string(std::begin(publicKey), std::end(publicKey)));
  int a = 0;
  vector<string> peerIds;
  for (auto& element : peerData.items()) {
    if (element.key() != hostId) {
	    peerIds.push_back(element.key());
    }
  }
  sort(peerIds.begin(), peerIds.end()); 
  // Make sure the host is always player 1
  peerIds.insert(peerIds.begin(), hostId);
  for (auto& peerId : peerIds) {
    auto& peerDataValue = peerData[peerId];
    if (peerId == userId) {
      if (position != -1) {
        LOGFATAL << "Got multiple peers with the same user id: " << peerId
                 << endl;
      }
      if (peerDataValue["key"] != publicKeyB64) {
        LOGFATAL << "Got userid match " << peerId
                 << " but public key doesn't match: " << peerDataValue["key"]
                 << " != " << publicKeyB64;
      }
      position = a;
    }
    a++;
  }
  if (position == -1) {
    LOGFATAL << "Could not find peer " << userId << " in game info: " << result;
  }
}

void MyPeer::start() {
  updateEndpointServerHttp();

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

void MyPeer::updateEndpointServerHttp() {
  auto myIps = LocalIpFetcher::fetch(serverPort, true);
  for (unsigned int a = 0; a < myIps.size(); a++) {
    myIps[a] = myIps[a] + ":" + to_string(serverPort);
  }
  for (const auto& stunIp : stunEndpoints) {
    myIps.push_back(stunIp.address().to_string() + ":" +
                    to_string(stunIp.port()));
  }

  string path = string("/api/update_endpoints");
  json request = {{"endpoints", myIps}, {"peerId", userId}};
  LOG(INFO) << "SENDING ENDPOINT PACKET (HTTP): " << request;
  SimpleWeb::CaseInsensitiveMultimap header;
  header.insert(make_pair("Content-Type", "application/json"));
  header.insert(make_pair("Cookie", string("peerId=") + userId));
  json result = client->request("POST", path, request.dump(2), header);
}

void MyPeer::checkForEndpoints(const asio::error_code& error) {
  LOG(INFO) << "CHECKING FOR ENDPOINTS";
  if (error == asio::error::operation_aborted) {
    return;
  }
  LOG(INFO) << "CHECKING FOR ENDPOINTS";
  lock_guard<recursive_mutex> guard(peerDataMutex);
  LOG(INFO) << "CHECKING FOR ENDPOINTS";

  // updateEndpointServerHttp();

  // LOG(INFO) << "CHECK FOR ENDPOINTS";
  // Bail if a peer doesn't have endpoints yet
  string path = string("/api/get_game_info/") + gameId;
  json result = client->request("GET", path);
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
  }

  // Iterate over peer data and set up peers
  auto peerDataObject = result["peerData"];
  for (json::iterator it = peerDataObject.begin(); it != peerDataObject.end();
       ++it) {
    VLOG(1) << it.key() << " : " << it.value();
    string id = it.key();
    PublicKey peerKey =
        CryptoHandler::stringToKey<PublicKey>(it.value()["key"]);
    string peerName = it.value()["name"];
    peerData[id] = shared_ptr<PlayerData>(new PlayerData(peerKey, peerName));
    if (id == userId) {
      // Don't need to set up endpoint for myself, but update my user name
      name = peerName;
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
                                          peerCryptoHandler, endpoints,
                                          (id == hostId)));
    rpcServer->addEndpoint(id, endpointHandler);
  }

  // this_thread::sleep_for(chrono::seconds(1));

  {
    lock_guard<recursive_mutex> guard(peerDataMutex);
    myData = peerData[userId];
  }

  // Need to get initial position after everyone has connected.
  getInitialPosition();

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
  if (rpcServer.get() == NULL) {
    // Connection has finished
    return;
  }
  VLOG(1) << "UPDATE START";

  if (updateCounter % 1000 == 0) {
    // Per-second update
    // updateEndpointServerHttp();
    LOG(INFO) << "UPDATING";
    string path = string("/api/get_game_info/") + gameId;
    // TODO: This doesn't work yet because async calls require external
    // io_service
    /*
    client->request(
        "GET", path,
        [this](shared_ptr<HttpClient::Response> response,
               const SimpleWeb::error_code& ec) {
          LOG(INFO) << "GOT GAME INFO";
          lock_guard<recursive_mutex> guard(peerDataMutex);
          auto result = json::parse(response->content.string());
          // Iterate over peer data and update peers
          auto peerDataObject = result["peerData"];
          for (json::iterator it = peerDataObject.begin();
               it != peerDataObject.end(); ++it) {
            LOG(INFO) << it.key() << " : " << it.value() << "\n";
            if (it.key() == userId) {
              continue;
            }
            vector<udp::endpoint> endpoints;
            for (json::iterator it2 = it.value()["endpoints"].begin();
                 it2 != it.value()["endpoints"].end(); ++it2) {
              string endpointString = *it2;
              vector<string> tokens = split(endpointString, ':');
              auto newEndpoints =
                  netEngine->resolve(tokens.at(0), tokens.at(1));
              for (auto newEndpoint : newEndpoints) {
                endpoints.push_back(newEndpoint);
              }
            }

            auto endpointHandler = rpcServer->getEndpointHandler(it.key());
            endpointHandler->addEndpoints(endpoints);
          }
        });
        */
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
        LOG_EVERY_N(60, INFO)
            << "GOT INPUTS: " << peerKey << " " << startTime << " " << endTime;
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
  VLOG(1) << "UPDATE END";

  if (!shuttingDown) {
    updateTimer->expires_at(updateTimer->expires_at() +
                            asio::chrono::milliseconds(1));
    updateTimer->async_wait(
        std::bind(&MyPeer::update, this, std::placeholders::_1));
  } else {
    LOG(ERROR) << "Shutting down, stopping updates";
    updateFinished = true;
  }
}

bool MyPeer::initialized() {
  {
    lock_guard<recursive_mutex> guard(peerDataMutex);
    if (myData.get() == NULL) {
      return false;
    }

    if (!rpcServer->readyToSend()) {
      return false;
    }
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

unordered_map<string, map<string, string>> MyPeer::getAllInputValues(
    int64_t timestamp) {
  unordered_map<string, map<string, string>> values;
  for (auto& it : peerData) {
    auto peerId = it.first;
    if (rpcServer->isPeerShutDown(peerId)) {
      continue;
    }
    if (timestamp >= it.second->playerInputData.getExpirationTime()) {
      LOG_EVERY_N(1, INFO) << "WE GOT AN INPUT TOO LATE: " << timestamp << " >= " << it.second->playerInputData.getExpirationTime();
    }
    while (true) {
      LOG_EVERY_N(600, INFO)
          << "WAITING FOR EXPIRATION TIME: " << timestamp << " > "
          << it.second->playerInputData.getExpirationTime();
      if (it.second->playerInputData.waitForExpirationTime(timestamp)) {
        LOG_EVERY_N(600, INFO)
            << "GOT EXPIRATION TIME: " << timestamp << " > "
            << it.second->playerInputData.getExpirationTime();
        lock_guard<recursive_mutex> guard(peerDataMutex);
        auto allKeyValuesForPeer = it.second->playerInputData.getAll(timestamp);
        for (auto it2 : allKeyValuesForPeer) {
          // [] operator creates a vector if needed
          values[it2.first].insert(make_pair(it.first, it2.second));
        }
        break;
      }
      LOG(INFO) << "TIMED OUT WAITING FOR EXPIRATION TIME: " << timestamp
                << " > " << it.second->playerInputData.getExpirationTime();
      // Check if the peer is dead
      if (rpcServer->isPeerShutDown(peerId)) {
        LOG(INFO) << "Peer is shut down, giving up on input values";
        break;
      }
    }
  }
  return values;
}

unordered_map<string, string> MyPeer::getStateChanges(
    const unordered_map<string, string>& data) {
  return myData->playerInputData.getChanges(data);
}

void MyPeer::updateState(int64_t timestamp,
                         const unordered_map<string, string>& data) {
  string s;
  {
    lock_guard<recursive_mutex> guard(peerDataMutex);
    int64_t lastExpirationTime = myData->playerInputData.getExpirationTime();
    auto changedData = myData->playerInputData.getChanges(data);
    myData->playerInputData.put(lastExpirationTime, timestamp, changedData);
    lastSendBuffer.push_back(
        make_tuple(lastExpirationTime, timestamp, changedData));
    while (lastSendBuffer.size() > INPUT_SEND_WINDOW_SIZE) {
      lastSendBuffer.pop_front();
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
      writer.writePrimitive(
          get<0>(lastSendBuffer.back()) /*lastExpirationTime*/);
      writer.writePrimitive(get<1>(lastSendBuffer.back()) /*timestamp*/);
      writer.writeMap(get<2>(lastSendBuffer.back()) /*data*/);
      numMaps++;
    }
    s = writer.finish();
  }
  rpcServer->broadcast(s);
}

unordered_map<string, string> MyPeer::getFullState(int64_t timestamp) {
  unordered_map<string, string> state;
  for (auto& it : peerData) {
    bool printed = false;
    while (true) {
      {
        lock_guard<recursive_mutex> guard(peerDataMutex);
        auto expirationTime = it.second->playerInputData.getExpirationTime();
        if (timestamp < expirationTime) {
          VLOG(1) << "GOT STATE: " << it.first << " " << timestamp << " "
                  << expirationTime;
          unordered_map<string, string> peerState =
              it.second->playerInputData.getAll(timestamp);
          state.insert(peerState.begin(), peerState.end());
          break;
        } else {
          if (!printed) {
            printed = true;
            LOG(INFO) << "MISSING STATE, NEED TO WAIT:" << timestamp << ' '
                      << expirationTime;
          }
        }
        // Check if the peer is dead
        auto peerId = it.first;
        if (rpcServer->isPeerShutDown(peerId)) {
          break;
        }
      }
      wga::microsleep(0);
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
