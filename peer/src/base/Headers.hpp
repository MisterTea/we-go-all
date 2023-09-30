#ifndef __WGA_HEADERS__
#define __WGA_HEADERS__

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define ELPP_THREAD_SAFE (1)
#define DELPP_FEATURE_CRASH_LOG (1)
#define ELPP_FORCE_USE_STD_THREAD (1)

#ifdef WGA_MAMEHUB
// Use mame's asio
#include "asio.h"
#else
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include "asio.hpp"
#endif

#define USE_STANDALONE_ASIO (1)  // JJG: Add define for simplehttpserver

#ifdef __FreeBSD__
#define _WITH_GETLINE
#endif

#ifdef __NetBSD__
#include <util.h>
#endif

#ifdef _WIN32
#else
#include <ifaddrs.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <ctime>
#include <deque>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <streambuf>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Base64.hpp"
#include "UniversalStacktrace/ust/ust.hpp"
#include "easyloggingpp/src/easylogging++.h"

#define LOGFATAL LOG(ERROR) << ust::generate() << "\n", LOG(FATAL)

#include "CTPL/ctpl_stl.h"
#include "cppcodec/cppcodec/base64_default_rfc4648.hpp"
#include "msgpack.hpp"
#include "nlohmann/json.hpp"
#include "sole/sole.hpp"

#ifdef WIN32
#pragma warning(disable : 4996)  //_CRT_SECURE_NO_WARNINGS
#endif

#include "SimpleWebServer/client_http.hpp"
#include "SimpleWebServer/client_https.hpp"
#include "SimpleWebServer/server_http.hpp"

#define STATS_GO_INLINE
#define STATS_ENABLE_STDVEC_WRAPPERS
#include "stats/include/stats.hpp"

#include <optional>

#include "cpp-lru-cache/include/lrucache.hpp"
using namespace cache;

using namespace sole;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpsClient = SimpleWeb::Client<SimpleWeb::HTTPS>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

using namespace std;
using namespace std::chrono;

using base64 = cppcodec::base64_rfc4648;

using asio::ip::udp;
using nlohmann::json;
using namespace ctpl;

// Nonces for CryptoHandler
static const unsigned char CLIENT_SERVER_NONCE_MSB = 0;
static const unsigned char SERVER_CLIENT_NONCE_MSB = 1;

#ifndef WGA_VERSION
#define WGA_VERSION "UNKNOWN"
#endif

#define WGA_MAGIC std::string("WGAMAGIC")

#ifndef WIN32
#define strerror_s(BUF, SIZE, ERRNO) strerror_r(ERRNO, BUF, SIZE)
#endif

#define FATAL_FAIL(X)                                \
  if (((X) == -1)) {                                 \
    char buf[4096];                                  \
    ::strerror_s(buf, 4096, errno);                  \
    LOGFATAL << "Error: (" << errno << "): " << buf; \
  }

#define FATAL_FAIL_UNLESS_EINVAL(X)                  \
  if (((X) == -1) && errno != EINVAL) {              \
    char buf[4096];                                  \
    ::strerror_s(buf, 4096, errno);                  \
    LOGFATAL << "Error: (" << errno << "): " << buf; \
  }

#define FATAL_IF_FALSE(X)                            \
  if (((X) == false)) {                              \
    char buf[4096];                                  \
    ::strerror_s(buf, 4096, errno);                  \
    LOGFATAL << "Error: (" << errno << "): " << buf; \
  }

#define FATAL_IF_NULL(X)                             \
  if (((X) == NULL)) {                               \
    char buf[4096];                                  \
    ::strerror_s(buf, 4096, errno);                  \
    LOGFATAL << "Error: (" << errno << "): " << buf; \
  }

#define FATAL_FAIL_HTTP(RESPONSE)                                              \
  if (RESPONSE->status_code != "200 OK" &&                                     \
      RESPONSE->status_code != "308 Permanent Redirect")                       \
    LOGFATAL << "Error making http request: " << RESPONSE->status_code << "\n" \
             << RESPONSE->content.string();

#define DRAW_FROM_UNORDERED(ITERATOR, COLLECTION) \
  auto ITERATOR = COLLECTION.begin();             \
  std::advance(ITERATOR, rand() % COLLECTION.size());

namespace wga {
template <typename Out>
inline void split(const std::string &s, char delim, Out result) {
  std::stringstream ss;
  ss.str(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    *(result++) = item;
  }
}

inline std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, std::back_inserter(elems));
  return elems;
}

#ifndef _MSC_VER
inline std::string SystemToStr(const char *cmd) {
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
#endif

inline bool replace(std::string &str, const std::string &from,
                    const std::string &to) {
  size_t start_pos = str.find(from);
  if (start_pos == std::string::npos) return false;
  str.replace(start_pos, from.length(), to);
  return true;
}

inline int replaceAll(std::string &str, const std::string &from,
                      const std::string &to) {
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
std::array<T, N> stringToArray(const V &v) {
  assert(v.size() == N);
  std::array<T, N> d;
  using std::begin;
  using std::end;
  std::copy(begin(v), end(v), begin(d));  // this is the recommended way
  return d;
}

inline void microsleep(int64_t usec) {
  std::this_thread::sleep_for(std::chrono::microseconds(usec));
}
}  // namespace wga

#endif
