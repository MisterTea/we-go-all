#ifndef __MYPEER_H__
#define __MYPEER_H__

#include "CryptoHandler.hpp"
#include "Headers.hpp"
#include "HttpClientMuxer.hpp"
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
  void signalGameOver();
  bool isGameOver() const { return gameOver.load(); }
  void resetReachabilityTimers();
  bool isPeerUnreachable(const string& peerId, int timeoutSeconds);
  // Returns true if a living peer has been unreachable long enough to end the game.
  bool terminateIfPeerUnreachable(int timeoutSeconds);
  int getLivingPeerCount() {
    if (shuttingDown || rpcServer.get() == NULL) {
      return 0;
    } else {
      return rpcServer->getLivingPeerCount();
    }
  }

  int getTotalPeerCount() { return peerData.size(); }

  void host(const string& gameName);
  void join();
  void markReady();

  void start();
  void checkForEndpoints(const asio::error_code& error);
  void update(const asio::error_code& error);

  bool initialized();

  unordered_map<string, string> getStateChanges(
      const unordered_map<string, string>& data);
  void updateState(int64_t timestamp,
                   const unordered_map<string, string>& data);

  // Once netplay starts, keep the local input timeline ahead of the shared
  // clock from the network thread.  This remains active even if emulation is
  // blocked waiting for a remote timeline.
  void startInputPublisher(int64_t epochMicros, int delayMs);
  void setInputPublisherDelay(int delayMs);

  // True when every living peer's ChronoMap covers timestamp (expiration > ts).
  bool hasInputValuesAt(int64_t timestamp);

  // Wait without polling until every living peer covers timestamp.  Returns
  // false on timeout or game shutdown.
  bool waitForInputValuesAt(int64_t timestamp, int timeoutMs);

  // Non-blocking: returns empty if !hasInputValuesAt(timestamp).
  unordered_map<string, map<string, string>> getAllInputValues(
      int64_t timestamp);

  // Snapshot of our own latest published input values (for keepalive resend).
  unordered_map<string, string> getMyLatestInputValues();

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

  string getMyUserName() { return name; }

 protected:
  string userId;
  PrivateKey privateKey;
  PublicKey publicKey;
  bool shuttingDown;
  bool updateFinished;
  std::atomic<bool> gameOver{false};
  shared_ptr<NetEngine> netEngine;
  shared_ptr<HttpClientMuxer> client;
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
  std::atomic<int64_t> inputEpochMicros{0};
  std::atomic<int> inputPublisherDelayMs{0};
  std::atomic<bool> inputPublisherEnabled{false};
  set<udp::endpoint> stunEndpoints;
  int position;

  vector<string> getMyIps();
  void updateEndpointServerHttp();
  void getInitialPosition();
};
}  // namespace wga

#endif
