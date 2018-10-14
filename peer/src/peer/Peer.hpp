#ifndef __PEER_H__
#define __PEER_H__

#include "Headers.hpp"

namespace wga {
class Peer {
 public:
  Peer(shared_ptr<CryptoHandler> _cryptoHandler, const string &_name)
      : cryptoHandler(_cryptoHandler), name(_name) {}

 protected:
  shared_ptr<CryptoHandler> cryptoHandler;
  string name;
};
}  // namespace wga

#endif
