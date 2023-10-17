#ifndef __SINGLE_GAME_SERVER_H__
#define __SINGLE_GAME_SERVER_H__

#include "Headers.hpp"

#include "CryptoHandler.hpp"
#include "NetEngine.hpp"

namespace wga {
struct ServerPeerData {
  string id;
  PublicKey key;
  string name;
  set<string> endpoints;

  ServerPeerData() {}

  ServerPeerData(const string& _id, const PublicKey& _key, const string& _name)
      : id(_id), key(_key), name(_name) {}
};

inline void to_json(json& j, const wga::ServerPeerData& p) {
  j = json{{"id", p.id},
           {"key", CryptoHandler::keyToString(p.key)},
           {"name", p.name},
           {"endpoints", p.endpoints}};
}

class SingleGameServer {
 public:
  SingleGameServer(shared_ptr<NetEngine> netEngine, int _port,
                   const string& _hostId, const PublicKey& _hostKey,
                   const string& hostName, int _numPlayers);

  void start();
  void shutdown();

  virtual ~SingleGameServer() {}

  void addPeer(const string& id, const PublicKey& key, const string& name) {
    peerData[id] = ServerPeerData(id, key, name);
  }

  void setPeerEndpoints(const string& id, const vector<string> &endpointsWithDuplicatesInOtherPeers);

  string getGameId() { return gameId; }

 protected:
  int numPlayers;
  HttpServer server;
  string gameId;
  string gameName;
  string hostId;
  PublicKey hostKey;
  map<string, ServerPeerData> peerData;
  shared_ptr<thread> serverThread;
};
}  // namespace wga

#endif
