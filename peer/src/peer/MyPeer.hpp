#ifndef __MYPEER_H__
#define __MYPEER_H__

#include "Headers.hpp"

#include "CryptoHandler.hpp"
#include "MultiEndpointHandler.hpp"
#include "NetEngine.hpp"
#include "PlayerData.hpp"

namespace wga {
class MyPeer {
 public:
  MyPeer(shared_ptr<NetEngine> _netEngine, const PrivateKey& _privateKey,
         bool _host, int _serverPort);

  void shutdown();

  void start();
  void checkForEndpoints();
  void update();

  bool initialized() { return myData.get() != NULL; }

  void updateState(int64_t timestamp, unordered_map<string, string> data) {
    myData->playerInputData.put(myData->playerInputData.getCurrentTime(),
                                timestamp, data);
  }

 protected:
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
  map<PublicKey, shared_ptr<MultiEndpointHandler>> peerHandlers;
  map<PublicKey, shared_ptr<PlayerData>> peerData;
  shared_ptr<PlayerData> myData;
  shared_ptr<udp::socket> localSocket;
  shared_ptr<asio::steady_timer> updateTimer;
};
}  // namespace wga

#endif
