#ifndef __SINGLE_GAME_SERVER_H__
#define __SINGLE_GAME_SERVER_H__

#include "Headers.hpp"

namespace wga {
struct ServerPeerData {
  string key;
  string name;
  set<string> endpoints;

  ServerPeerData() {}

  ServerPeerData(const string& _key, const string& _name) {
    key = _key;
    name = _name;
  }
};

void to_json(json& j, const wga::ServerPeerData& p) {
  j = json{{"key", p.key}, {"name", p.name}, {"endpoints", p.endpoints}};
}

class SingleGameServer {
 public:
  SingleGameServer(int _port, const string& _hostKey, const string& hostName) {
    server.config.port = _port;
    gameId = sole::uuid4();
    hostKey = _hostKey;
    addPeer(hostKey, hostName);

    server.resource["^/get_current_game_id/(.+)$"]["GET"] =
        [this](shared_ptr<HttpServer::Response> response,
               shared_ptr<HttpServer::Request> request) {
          string peerKey = request->path_match[1].str();
          if (peerData.find(peerKey) == peerData.end()) {
            LOG(FATAL) << "Invalid peer key: " << peerKey;
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
          json retval;
          retval["gameName"] = gameName;
          retval["peerData"] = peerData;
          retval["hostKey"] = hostKey;
          response->write(SimpleWeb::StatusCode::success_ok, retval.dump(2));
        };

    server.resource["^/host$"]["POST"] =
        [this](shared_ptr<HttpServer::Response> response,
               shared_ptr<HttpServer::Request> request) {
          auto content = json::parse(request->content.string());
          // Here we would use signing to make sure the message is legit

          if (content["hostKey"].get<string>() != hostKey) {
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

  void addPeer(const string& key, const string& name) {
    peerData[key] = ServerPeerData(key, name);
  }

  void setPeerEndpoints(const string& key, const vector<string>& endpoints) {
    auto it = peerData.find(key);
    if (it == peerData.end()) {
      LOG(FATAL) << "Could not find peer";
    }
    LOG(INFO) << "SETTING ENDPOINTS: " << endpoints[0] << " " << endpoints[1];
    for (auto &endpoint : endpoints) {
      it->second.endpoints.insert(endpoint);
    }
  }

  uuid getGameId() { return gameId; }

 protected:
  HttpServer server;
  uuid gameId;
  string gameName;
  string hostKey;
  map<string, ServerPeerData> peerData;
  shared_ptr<thread> serverThread;
};
}  // namespace wga

#endif
