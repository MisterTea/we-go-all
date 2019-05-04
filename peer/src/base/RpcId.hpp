#ifndef __RPC_ID_H__
#define __RPC_ID_H__

#include "Headers.hpp"

namespace wga {
class RpcId {
 public:
  RpcId() : barrier(0), id(0) {}
  RpcId(uint64_t _barrier, uint64_t _id) : barrier(_barrier), id(_id) {}
  bool operator==(const RpcId& other) const {
    return barrier == other.barrier && id == other.id;
  }
  bool operator!=(const RpcId& other) const { return !(*this == other); }
  bool operator<(const RpcId& other) const {
    return barrier < other.barrier ||
           (barrier == other.barrier && id < other.id);
  }
  string str() const { return to_string(barrier) + "/" + to_string(id); }
  bool empty() { return barrier == 0 && id == 0; }

  uint64_t barrier;
  uint64_t id;
};

#define SESSION_KEY_RPCID RpcId(0, 1)

}  // namespace wga

#endif