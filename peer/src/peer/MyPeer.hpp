#ifndef __MYPEER_H__
#define __MYPEER_H__

#include "Headers.hpp"

#include "CryptoHandler.hpp"
#include "MultiEndpointHandler.hpp"
#include "NetEngine.hpp"
#include "PlayerData.hpp"
#include "RpcServer.hpp"

namespace wga {
class MyPeer {
 public:
  MyPeer(shared_ptr<NetEngine> _netEngine, const PrivateKey& _privateKey,
         bool _host, int _serverPort);

  void shutdown();

  void start(const string& lobbyHost, int lobbyPort);
  void checkForEndpoints(const asio::error_code& error);
  void update(const asio::error_code& error);

  bool initialized();

  void updateState(int64_t timestamp, unordered_map<string, string> data);

  unordered_map<string, string> getFullState(int64_t timestamp);

 protected:
  bool shuttingDown;
  shared_ptr<NetEngine> netEngine;
  PrivateKey privateKey;
  PublicKey publicKey;
  string publicKeyString;
  shared_ptr<HttpClient> client;
  uuid gameId;
  bool host;
  int serverPort;
  string gameName;
  PublicKey hostKey;
  shared_ptr<RpcServer> rpcServer;
  map<PublicKey, shared_ptr<PlayerData>> peerData;
  recursive_mutex peerDataMutex;
  shared_ptr<PlayerData> myData;
  shared_ptr<udp::socket> localSocket;
  shared_ptr<asio::steady_timer> updateTimer;
};
}  // namespace wga

#endif
