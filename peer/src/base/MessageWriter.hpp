#ifndef __MESSAGE_WRITER_H__
#define __MESSAGE_WRITER_H__

#include "Headers.hpp"

namespace wga {
class MessageWriter {
 public:
  MessageWriter() : packHandler(buffer) {}

  inline void start() { buffer.clear(); }

  template <typename T>
  inline void writePrimitive(const T& t) {
    packHandler.pack(t);
  }

  template <typename T>
  inline void writeClass(const T& t) {
    string s(sizeof(T), '\0');
    memcpy(&s[0], &t, sizeof(T));
    writePrimitive<string>(s);
  }

  template <typename T>
  inline void writeProto(const T& t) {
    string s;
    t.SerializeToString(&s);
    writePrimitive<string>(s);
  }

  inline string finish() {
    string s(buffer.data(), buffer.size());
    start();
    return s;
  }

  inline int64_t size() { return buffer.size(); }

 protected:
  msgpack::sbuffer buffer;
  msgpack::packer<msgpack::sbuffer> packHandler;
};
}  // namespace wga

#endif  // __MESSAGE_WRITER_H__