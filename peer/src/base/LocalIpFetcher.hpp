#pragma once

#include "Headers.hpp"

namespace wga {
class LocalIpFetcher {
 public:
  static vector<string> fetch(int port, bool udp);
};
}  // namespace wga