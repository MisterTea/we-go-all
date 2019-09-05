#ifndef __RPC_ID_H__
#define __RPC_ID_H__

#include "Headers.hpp"

namespace wga {
class RpcId {
 public:
  RpcId() : barrier(0), id(0) {}
  RpcId(int64_t _barrier, uint64_t _id) : barrier(_barrier), id(_id) {}
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

  int64_t barrier;
  uint64_t id;
};
}  // namespace wga

namespace std {
template <>
struct hash<wga::RpcId> : public std::unary_function<wga::RpcId, size_t> {
 public:
  // hash functor: hash uuid to size_t value by pseudorandomizing transform
  size_t operator()(const wga::RpcId& rpcId) const {
    if (sizeof(size_t) > 4) {
      return size_t(rpcId.barrier ^ rpcId.id);
    } else {
      uint64_t hash64 = rpcId.barrier ^ rpcId.id;
      return size_t(uint32_t(hash64 >> 32) ^ uint32_t(hash64));
    }
  }
};
}  // namespace std

#define SESSION_KEY_RPCID RpcId(0, 1)

#endif