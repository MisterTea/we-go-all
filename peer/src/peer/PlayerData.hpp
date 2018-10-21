#ifndef __PLAYER_DATA_H__
#define __PLAYER_DATA_H__

#include "CryptoHandler.hpp"
#include "Headers.hpp"

namespace wga {
class PlayerData {
 public:
  PlayerData(const PublicKey& _publicKey, const string& _name)
      : publicKey(_publicKey), name(_name) {}

 protected:
  PublicKey publicKey;
  string name;
};
}  // namespace wga

#endif
