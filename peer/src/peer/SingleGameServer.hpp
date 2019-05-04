#ifndef __SINGLE_GAME_SERVER_H__
#define __SINGLE_GAME_SERVER_H__

#include "Headers.hpp"

namespace wga {
struct ServerPeerData {
  PublicKey key;
  string name;
  set<string> endpoints;

  ServerPeerData() {}

  ServerPeerData(const PublicKey& _key, const string& _name) {
    key = _key;
    name = _name;
  }
};

inline void to_json(json& j, const wga::ServerPeerData& p) {
  j = json{{"key", CryptoHandler::keyToString(p.key)},
           {"name", p.name},
           {"endpoints", p.endpoints}};
}

class SingleGameServer {
 public:
  SingleGameServer(int _port, const PublicKey& _hostKey,
                   const string& hostName) {
    server.config.port = _port;
    gameId = sole::uuid4();
    hostKey = _hostKey;
    addPeer(hostKey, hostName);

    server.resource["^/get_current_game_id/(.+)$"]["GET"] =
        [this](shared_ptr<HttpServer::Response> response,
               shared_ptr<HttpServer::Request> request) {
          auto peerKey = CryptoHandler::stringToKey<PublicKey>(
              request->path_match[1].str());
          if (peerData.find(peerKey) == peerData.end()) {
            LOG(FATAL) << "Invalid peer key: "
                       << CryptoHandler::keyToString(peerKey);
          }
          json retval;
          retval["gameId"] = gameId.str();
          response->write(SimpleWeb::StatusCode::success_ok, retval.dump(2));
        };

    server.resource["^/get_game_info/(.+)$"]["GET"] =
        [this](shared_ptr<HttpServer::Response> response,
               shared_ptr<HttpServer::Request> request) {
          auto _gameId = sole::rebuild(request->path_match[1].str());
          if (gameId != _gameId) {
            LOG(FATAL) << "Invalid game id: " << _gameId.str();
          }
          string hostKeyB64 = CryptoHandler::keyToString(hostKey);
          json retval;
          retval["gameName"] = gameName;
          map<string, ServerPeerData> stringPeerData;
          for (const auto& it : peerData) {
            stringPeerData[CryptoHandler::keyToString(it.first)] = it.second;
          }
          retval["peerData"] = stringPeerData;
          retval["hostKey"] = hostKeyB64;
          response->write(SimpleWeb::StatusCode::success_ok, retval.dump(2));
        };

    server.resource["^/host$"]["POST"] =
        [this](shared_ptr<HttpServer::Response> response,
               shared_ptr<HttpServer::Request> request) {
          auto content = json::parse(request->content.string());
          // Here we would use signing to make sure the message is legit

          string hostKeyB64 = CryptoHandler::keyToString(hostKey);
          if (content["hostKey"].get<string>() != hostKeyB64) {
            LOG(FATAL) << "Host key does not match";
          }
          if (sole::rebuild(content["gameId"].get<string>()) != gameId) {
            LOG(FATAL) << "Game ID does not match";
          }
          gameName = content["gameName"].get<string>();

          json retval = {{"status", "OK"}};
          response->write(SimpleWeb::StatusCode::success_ok, retval.dump(2));
        };

    serverThread.reset(new thread([this]() {
      // Start server
      server.start();
    }));
  }

  virtual ~SingleGameServer() {
    server.stop();
    serverThread->join();
    serverThread.reset();
  }

  void addPeer(const PublicKey& key, const string& name) {
    peerData[key] = ServerPeerData(key, name);
  }

  void setPeerEndpoints(const PublicKey& key, const vector<string>& endpoints) {
    auto it = peerData.find(key);
    if (it == peerData.end()) {
      LOG(FATAL) << "Could not find peer: " << CryptoHandler::keyToString(key);
    }
    LOG(INFO) << "SETTING ENDPOINTS";
    for (auto& endpoint : endpoints) {
      LOG(INFO) << "SETTING ENDPOINT: " << endpoint;
      it->second.endpoints.insert(endpoint);
    }
  }

  uuid getGameId() { return gameId; }

 protected:
  HttpServer server;
  uuid gameId;
  string gameName;
  PublicKey hostKey;
  map<PublicKey, ServerPeerData> peerData;
  shared_ptr<thread> serverThread;
};
}  // namespace wga

#endif
