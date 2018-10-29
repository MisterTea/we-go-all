#ifndef __ET_HEADERS__
#define __ET_HEADERS__

#if __FreeBSD__
#define _WITH_GETLINE
#endif

#include "asio.hpp"

#if __NetBSD__
#include <util.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <ctime>
#include <deque>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <streambuf>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "easylogging++.h"

#include "base64.hpp"
#include "catch2/catch.hpp"
#include "ctpl_stl.h"
#include "json.hpp"
#include "msgpack.hpp"
#include "optional.hpp"
#include "sole.hpp"

using namespace std;
using namespace std::chrono;
using namespace std::experimental;

namespace google {}
namespace gflags {}
using namespace google;
using namespace gflags;

using namespace base64;

using asio::ip::udp;
using nlohmann::json;
using namespace ctpl;

// The ET protocol version supported by this binary
static const int PROTOCOL_VERSION = 4;

// Nonces for CryptoHandler
static const unsigned char CLIENT_SERVER_NONCE_MSB = 0;
static const unsigned char SERVER_CLIENT_NONCE_MSB = 1;

#ifndef WGA_VERSION
#define WGA_VERSION "UNKNOWN"
#endif

#define FATAL_FAIL(X) \
  if (((X) == -1))    \
    LOG(FATAL) << "Error: (" << errno << "): " << strerror(errno);

#define FATAL_FAIL_UNLESS_EINVAL(X)   \
  if (((X) == -1) && errno != EINVAL) \
    LOG(FATAL) << "Error: (" << errno << "): " << strerror(errno);

#define FATAL_IF_FALSE(X) \
  if (((X) == false))     \
    LOG(FATAL) << "Error: (" << errno << "): " << strerror(errno);

#define DRAW_FROM_UNORDERED(ITERATOR, COLLECTION) \
  auto ITERATOR = COLLECTION.begin();             \
  std::advance(ITERATOR, rand() % COLLECTION.size());

namespace wga {
template <typename Out>
inline void split(const std::string& s, char delim, Out result) {
  std::stringstream ss;
  ss.str(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    *(result++) = item;
  }
}

inline std::vector<std::string> split(const std::string& s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, std::back_inserter(elems));
  return elems;
}

inline std::string SystemToStr(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
  if (!pipe) throw std::runtime_error("popen() failed!");
  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
      result += buffer.data();
  }
  return result;
}

inline bool replace(std::string& str, const std::string& from,
                    const std::string& to) {
  size_t start_pos = str.find(from);
  if (start_pos == std::string::npos) return false;
  str.replace(start_pos, from.length(), to);
  return true;
}

inline int replaceAll(std::string& str, const std::string& from,
                      const std::string& to) {
  if (from.empty()) return 0;
  int retval = 0;
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    retval++;
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();  // In case 'to' contains 'from', like replacing
                               // 'x' with 'yx'
  }
  return retval;
}

template <class T, size_t N, class V>
std::array<T, N> stringToArray(const V& v) {
  assert(v.size() == N);
  std::array<T, N> d;
  using std::begin;
  using std::end;
  std::copy(begin(v), end(v), begin(d));  // this is the recommended way
  return d;
}

}  // namespace wga

#endif
