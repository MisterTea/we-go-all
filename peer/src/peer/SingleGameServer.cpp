#include "SingleGameServer.hpp"

namespace wga {
SingleGameServer::SingleGameServer(shared_ptr<NetEngine> netEngine, int _port,
                                   const string& _hostId,
                                   const PublicKey& _hostKey,
                                   const string& hostName, int _numPlayers)
    : numPlayers(_numPlayers) {
  netEngine->forwardPort(_port);
  server.config.port = _port;
  gameId = "MyGameId";
  hostId = _hostId;
  hostKey = _hostKey;
  addPeer(hostId, hostKey, hostName);

  server.resource["^/api/get_current_game_id/(.+)$"]["GET"] =
      [this](shared_ptr<HttpServer::Response> response,
             shared_ptr<HttpServer::Request> request) {
        auto peerId = request->path_match[1].str();
        json retval;
        retval["gameId"] = gameId;
        response->write(SimpleWeb::StatusCode::success_ok, retval.dump(2));
      };

  server.resource["^/api/get_game_info/(.+)$"]["GET"] =
      [this](shared_ptr<HttpServer::Response> response,
             shared_ptr<HttpServer::Request> request) {
        auto _gameId = request->path_match[1].str();
        if (gameId != _gameId) {
          LOGFATAL << "Invalid game id: " << _gameId;
        }
        json retval;
        retval["gameName"] = gameName;
        map<string, ServerPeerData> stringPeerData;
        for (const auto& it : peerData) {
          stringPeerData[it.first] = it.second;
        }
        bool ready = (peerData.size() >= numPlayers);
        if (ready) {
          for (const auto& it : peerData) {
            if (it.second.endpoints.empty()) {
              ready = false;
            }
          }
        }
        retval["ready"] = ready;
        retval["peerData"] = stringPeerData;
        retval["hostId"] = hostId;
        response->write(SimpleWeb::StatusCode::success_ok, retval.dump(2));
      };

  server.resource["^/api/host$"]["POST"] =
      [this](shared_ptr<HttpServer::Response> response,
             shared_ptr<HttpServer::Request> request) {
        auto content = json::parse(request->content.string());
        // Here we would use signing to make sure the message is legit

        if (content["hostId"].get<string>() != hostId) {
          LOGFATAL << "Host ID does not match: " << hostId
                   << " != " << content["hostId"].get<string>();
        }
        if (content["gameId"].get<string>() != gameId) {
          LOGFATAL << "Game ID does not match";
        }
        gameName = content["gameName"].get<string>();

        json retval = {{"status", "OK"}};
        response->write(SimpleWeb::StatusCode::success_ok, retval.dump(2));
      };

  server.resource["^/api/join$"]["POST"] =
      [this](shared_ptr<HttpServer::Response> response,
             shared_ptr<HttpServer::Request> request) {
        auto content = json::parse(request->content.string());
        auto peerId = content["peerId"].get<string>();
        auto name = content["name"].get<string>();
        auto peerKey = CryptoHandler::stringToKey<PublicKey>(
            content["peerKey"].get<string>());

        if (peerData.find(peerId) != peerData.end()) {
          LOGFATAL << "Tried to add a peer that already exists";
        }
        addPeer(peerId, peerKey, name);

        json retval = {{"status", "OK"}};
        response->write(SimpleWeb::StatusCode::success_ok, retval.dump(2));
      };

  server.resource["^/api/update_endpoints$"]["POST"] =
      [this](shared_ptr<HttpServer::Response> response,
             shared_ptr<HttpServer::Request> request) {
        auto content = json::parse(request->content.string());

        LOG(INFO) << "GOT ENDPOINT DATA: " << content;
        string id = content["peerId"].get<string>();
        vector<string> allEndpoints;
        for (auto it = content["endpoints"].begin(); it != content["endpoints"].end(); it++) {
          allEndpoints.push_back(it->get<string>());
        }
        setPeerEndpoints(id, allEndpoints);

        json retval = {{"status", "OK"}};
        response->write(SimpleWeb::StatusCode::success_ok, retval.dump(2));
      };
}

void SingleGameServer::start() {
  serverThread.reset(new thread([this]() { server.start(); }));
}

void SingleGameServer::shutdown() {
  server.stop();
  if (serverThread->joinable()) {
    serverThread->join();
  }
}

void SingleGameServer::setPeerEndpoints(const string& id, const vector<string> &endpointsWithDuplicatesInOtherPeers) {
  // Remove any endpoints that exist in other peers.  We really should remove both endpoints but then we need to keep track of removed endpoints.
  vector<string> endpointsWithoutDuplicates;
  for (auto& endpoint : endpointsWithDuplicatesInOtherPeers) {
    bool gotDupe = false;
    for (auto &entry : peerData) {
      if (entry.first == id) {
        continue;
      }
      if (entry.second.endpoints.find(endpoint) != entry.second.endpoints.end()) {
        // Found the same endpoint in another peer.
        gotDupe = true;
      }
    }
    if (!gotDupe) {
      endpointsWithoutDuplicates.push_back(endpoint);
    }
  }
  auto it = peerData.find(id);
  if (it == peerData.end()) {
    LOG(ERROR) << "Could not find peer: " << id;
    return;
  }
  VLOG(1) << "SETTING ENDPOINTS";
  for (auto& endpoint : endpointsWithoutDuplicates) {
    VLOG(1) << "SETTING ENDPOINT: " << endpoint;
    it->second.endpoints.insert(endpoint);
  }
}

}  // namespace wga
