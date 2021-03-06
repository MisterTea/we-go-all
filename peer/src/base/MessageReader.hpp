#ifndef __MESSAGE_READER_H__
#define __MESSAGE_READER_H__

#include "Headers.hpp"

namespace wga {
class MessageReader {
 public:
  MessageReader() {}

  inline void load(const string& s) {
    unpackHandler.remove_nonparsed_buffer();
    unpackHandler.reserve_buffer(s.size());
    memcpy(unpackHandler.buffer(), s.c_str(), s.length());
    unpackHandler.buffer_consumed(s.length());
  }

  template <unsigned long i>
  inline void load(const std::array<char, i>& a, int size) {
    unpackHandler.remove_nonparsed_buffer();
    unpackHandler.reserve_buffer(size);
    memcpy(unpackHandler.buffer(), &a[0], size);
    unpackHandler.buffer_consumed(size);
  }

  template <typename T>
  inline T readPrimitive() {
    msgpack::object_handle oh;
    if (!unpackHandler.next(oh)) {
      throw std::runtime_error("Read failed");
    }
    T t = oh.get().convert();
    return t;
  }

  template <typename MAP>
  inline MAP readMap() {
    msgpack::object_handle oh;
    FATAL_IF_FALSE(unpackHandler.next(oh));
    MAP t = oh.get().convert();
    return t;
  }

  template <typename T>
  inline T readClass() {
    T t;
    string s = readPrimitive<string>();
    if (s.length() != sizeof(T)) {
      throw std::runtime_error("Invalid Class Size");
    }
    memcpy(&t, &s[0], sizeof(T));
    return t;
  }

  template <typename T>
  inline T readProto() {
    T t;
    string s = readPrimitive<string>();
    if (!t.ParseFromString(s)) {
      throw std::runtime_error("Invalid proto");
    }
    return t;
  }

  inline int64_t sizeRemaining() {
    // TODO: Make sure this is accurate
    return unpackHandler.nonparsed_size();
  }

 protected:
  msgpack::unpacker unpackHandler;
};
}  // namespace wga

#endif  // __MESSAGE_READER_H__