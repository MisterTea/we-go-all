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
  MyPeer(const string& _userId, const PrivateKey& _privateKey, int _serverPort,
         const string& _lobbyHost, int _lobbyPort, const string& _name);

  void shutdown();
  inline bool isShutdown() { return shuttingDown; }
  int getLivingPeerCount() {
    if (shuttingDown || rpcServer.get() == NULL) {
      return 0;
    } else {
      return rpcServer->getLivingPeerCount();
    }
  }

  void host(const string& gameName);
  void join();

  void start();
  void checkForEndpoints(const asio::error_code& error);
  void update(const asio::error_code& error);

  bool initialized();

  unordered_map<string, string> getStateChanges(const unordered_map<string, string>& data);
  void updateState(int64_t timestamp,
                   const unordered_map<string, string>& data);

  unordered_map<string,vector<string>> getAllInputValues(int64_t timestamp);

  // TODO: This causes collisions and should be removed
  unordered_map<string, string> getFullState(int64_t timestamp);

  int64_t getNearestExpirationTime();

  void finish() {
    while (rpcServer->hasWork()) {
      LOG(INFO) << "WAITING FOR WORK TO FINISH";
      microsleep(1000 * 1000);
    }
  }

  map<string, pair<double, double>> getPeerLatency() {
    return rpcServer->getPeerLatency();
  }

  double getHalfPingUpperBound() { return rpcServer->getHalfPingUpperBound(); }

  string getGameName() { return gameName; }

  bool isHosting() { return hosting; }

  int getPosition() { return position; }

 protected:
  string userId;
  PrivateKey privateKey;
  PublicKey publicKey;
  bool shuttingDown;
  bool updateFinished;
  shared_ptr<NetEngine> netEngine;
  shared_ptr<HttpClient> client;
  string gameId;
  int serverPort;
  string hostId;
  shared_ptr<RpcServer> rpcServer;
  map<string, shared_ptr<PlayerData>> peerData;
  recursive_mutex peerDataMutex;
  shared_ptr<PlayerData> myData;
  shared_ptr<udp::socket> localSocket;
  shared_ptr<asio::steady_timer> updateTimer;
  deque<tuple<int64_t, int64_t, unordered_map<string, string>>> lastSendBuffer;
  string lobbyHost;
  int lobbyPort;
  string gameName;
  string name;
  bool timeShiftInitialized;
  bool hosting;
  int updateCounter;
  set<udp::endpoint> stunEndpoints;
  int position;

  void updateEndpointServer();
  void updateEndpointServerHttp();
  void getInitialPosition();
};
}  // namespace wga

#endif
