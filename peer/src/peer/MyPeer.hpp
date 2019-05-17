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
         int _serverPort, const string& _lobbyHost, int _lobbyPort);

  void shutdown();

  void host(const string& gameName);
  void start();
  void checkForEndpoints(const asio::error_code& error);
  void update(const asio::error_code& error);

  bool initialized();

  void updateState(int64_t timestamp, unordered_map<string, string> data);

  vector<string> getAllInputValues(int64_t timestamp, const string& key);

  unordered_map<string, string> getFullState(int64_t timestamp);
  int64_t getNearestExpirationTime();

  void finish() {
    while (rpcServer->hasWork()) {
      LOG(INFO) << "WAITING FOR WORK TO FINISH";
      usleep(1000 * 1000);
    }
  }

  map<PublicKey, pair<double, double>> getPeerLatency() {
    return rpcServer->getPeerLatency();
  }

 protected:
  bool shuttingDown;
  shared_ptr<NetEngine> netEngine;
  PrivateKey privateKey;
  PublicKey publicKey;
  string publicKeyString;
  shared_ptr<HttpClient> client;
  uuid gameId;
  int serverPort;
  PublicKey hostKey;
  shared_ptr<RpcServer> rpcServer;
  map<PublicKey, shared_ptr<PlayerData>> peerData;
  recursive_mutex peerDataMutex;
  shared_ptr<PlayerData> myData;
  shared_ptr<udp::socket> localSocket;
  shared_ptr<asio::steady_timer> updateTimer;
  string lobbyHost;
  int lobbyPort;
  string gameName;

  void updateEndpointServer();
};
}  // namespace wga

#endif
