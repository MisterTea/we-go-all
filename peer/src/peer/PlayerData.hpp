#ifndef __PLAYER_DATA_H__
#define __PLAYER_DATA_H__

#include "Headers.hpp"

#include "ChronoMap.hpp"

namespace wga {
class PlayerData {
 public:
  PlayerData(const PublicKey& _publicKey, const string& _name)
      : publicKey(_publicKey), name(_name) {}

  PublicKey publicKey;
  string name;
  ChronoMap<string, string> playerInputData;
  ChronoMap<string, string> metadata;
};
}  // namespace wga

#endif
