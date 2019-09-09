#include "StunClient.hpp"

namespace wga {

class decoder {
 public:
  using const_iterator = const std::uint8_t*;

  decoder(const decoder&);

  template <typename RandomAccessSequence>
  decoder(const RandomAccessSequence&);

  template <typename RandomAccessIterator>
  decoder(RandomAccessIterator begin, RandomAccessIterator end);

  template <typename RandomAccessIterator>
  decoder(RandomAccessIterator begin, std::size_t);

  decoder();

  template <typename RandomAccessIterator>
  void reset(RandomAccessIterator begin, std::size_t);

  void skip(std::size_t size);
  bool empty() const;
  std::size_t size() const;

  template <typename T, typename... Args>
  T get(Args&&...);
  void get_raw(std::uint8_t*, std::size_t);

  const_iterator current() const;

  bool error() const { return _was_error; }
  void set_error() { _was_error = true; }

  void shrink(size_t);

 private:
  struct {
    const_iterator begin;
    const_iterator end;
  } _current;

  bool _was_error;
};

template <typename RandomAccessIterator>
decoder::decoder(RandomAccessIterator begin, RandomAccessIterator end)
    : _was_error(false) {
  _current.begin = reinterpret_cast<const_iterator>(begin);
  _current.end = reinterpret_cast<const_iterator>(end);
}

template <typename RandomAccessIterator>
decoder::decoder(RandomAccessIterator begin, std::size_t size)
    : _was_error(false) {
  _current.begin = reinterpret_cast<const_iterator>(begin);
  _current.end = _current.begin + size;
}

inline decoder::decoder() : _was_error(true) {
  _current.begin = nullptr;
  _current.end = nullptr;
}

inline decoder::decoder(const decoder& d) : _was_error(d._was_error) {
  _current.begin = d._current.begin;
  _current.end = d._current.end;
}

template <typename RandomAccessSequence>
decoder::decoder(const RandomAccessSequence& sequence) : _was_error(false) {
  _current.begin = sequence.data();
  auto value_size = sizeof(typename RandomAccessSequence::value_type);
  _current.end = sequence.data() + sequence.size() * value_size;
}

template <typename RandomAccessIterator>
void decoder::reset(RandomAccessIterator begin, std::size_t size) {
  _was_error = false;
  _current.begin = reinterpret_cast<const_iterator>(begin);
  _current.end = _current.begin + size;
}

inline void decoder::shrink(size_t new_size) {
  if (_was_error) return;

  size_t remaining = _current.end - _current.begin;

  if (new_size < remaining) {
    _current.end = _current.begin + new_size;
  }
}

inline void decoder::skip(std::size_t size) {
  if (size > this->size()) _was_error = true;
  if (_was_error) return;
  _current.begin += size;
}

inline decoder::const_iterator decoder::current() const {
  return _current.begin;
}

inline bool decoder::empty() const { return _current.begin == _current.end; }

inline std::size_t decoder::size() const {
  return _current.end - _current.begin;
}

template <>
inline std::uint8_t decoder::get<std::uint8_t>() {
  if (empty()) _was_error = true;
  if (_was_error) return 0;
  return *(_current.begin++);
}

template <>
inline std::int8_t decoder::get<std::int8_t>() {
  if (empty()) _was_error = true;
  if (_was_error) return 0;
  return *(_current.begin++);
}

template <>
inline char decoder::get<char>() {
  if (empty()) _was_error = true;
  if (_was_error) return 0;
  return *(_current.begin++);
}

template <>
inline std::uint16_t decoder::get<std::uint16_t>() {
  std::uint16_t result;
  if (size() < sizeof(result)) _was_error = true;
  if (_was_error) return 0;

  result = _current.begin[0] << 8 | _current.begin[1];

  _current.begin += sizeof(result);
  return result;
}

template <>
inline std::int16_t decoder::get<std::int16_t>() {
  std::int16_t result;
  if (size() < sizeof(result)) _was_error = true;
  if (_was_error) return 0;

  result = _current.begin[0] << 8 | _current.begin[1];

  _current.begin += sizeof(result);
  return result;
}

template <>
inline std::uint32_t decoder::get<std::uint32_t>() {
  std::uint32_t result;
  if (size() < sizeof(result)) _was_error = true;
  if (_was_error) return 0;

  result = _current.begin[0] << 24 | _current.begin[1] << 16 |
           _current.begin[2] << 8 | _current.begin[3];

  _current.begin += sizeof(result);
  return result;
}

template <>
inline std::int32_t decoder::get<std::int32_t>() {
  std::int32_t result;
  if (size() < sizeof(result)) _was_error = true;
  if (_was_error) return 0;

  result = _current.begin[0] << 24 | _current.begin[1] << 16 |
           _current.begin[2] << 8 | _current.begin[3];

  _current.begin += sizeof(result);
  return result;
}

template <>
inline std::int64_t decoder::get<std::int64_t>() {
  std::int64_t result;
  if (size() < sizeof(result)) _was_error = true;
  if (_was_error) return 0;

  uint8_t* r = reinterpret_cast<uint8_t*>(&result);
  *r++ = _current.begin[7];
  *r++ = _current.begin[6];
  *r++ = _current.begin[5];
  *r++ = _current.begin[4];
  *r++ = _current.begin[3];
  *r++ = _current.begin[2];
  *r++ = _current.begin[1];
  *r++ = _current.begin[0];

  _current.begin += sizeof(result);
  return result;
}

template <>
inline std::uint64_t decoder::get<std::uint64_t>() {
  std::uint64_t result;
  if (size() < sizeof(result)) _was_error = true;
  if (_was_error) return 0;

  uint8_t* r = reinterpret_cast<uint8_t*>(&result);
  *r++ = _current.begin[7];
  *r++ = _current.begin[6];
  *r++ = _current.begin[5];
  *r++ = _current.begin[4];
  *r++ = _current.begin[3];
  *r++ = _current.begin[2];
  *r++ = _current.begin[1];
  *r++ = _current.begin[0];

  _current.begin += sizeof(result);
  return result;
}

inline void decoder::get_raw(std::uint8_t* iter, std::size_t size) {
  if (this->size() < size) _was_error = true;
  if (_was_error) return;

  for (std::size_t i = 0; i < size; ++i) {
    *(iter++) = *(_current.begin++);
  }
}

template <class T, typename... Args>
void decode(decoder&, Args&&...);

inline void decode(decoder& d, asio::ip::address_v4& addr) {
  addr = asio::ip::address_v4(d.get<std::uint32_t>());
}

inline void decode(decoder& d, asio::ip::address_v6& addr) {
  using asio::ip::address_v6;
  address_v6::bytes_type bytes;
  d.get_raw(bytes.data(), bytes.size());
  addr = address_v6(bytes);
}

template <typename T, typename... Args>
inline T decoder::get(Args&&... args) {
  if (_was_error) return T();

  T retval;
  decode(*this, retval, args...);

  return retval;
}

static const bitset<2> SUCCESS_CLASS("10");
static const uint16_t BIND_METHOD = 0x001;
static const uint32_t COOKIE = 0x2112A442;
static const size_t HEADER_SIZE = 20;  // Bytes
static const uint16_t ATTRIB_UNKNOWN = 0x0000;
static const uint16_t ATTRIB_MAPPED_ADDRESS = 0x0001;
static const uint16_t ATTRIB_XOR_MAPPED_ADDRESS = 0x0020;

struct Attribute {
  uint16_t type;
  uint16_t length;
  const uint8_t* data;
};

// Result is in host byte order.
static uint16_t decode_method(uint16_t data) {
  // Bits #7 and 11 (from left, and #4 and 8 from right)
  // belong to the class, so need to be extracted.
  // Also last two bits are always zero.
  // Bits #0 and #1 (from left, and #14 and #15 from right)
  // are reserved for multiplexing.
  uint16_t result = (data & (1 << 0)) | (data & (1 << 1)) | (data & (1 << 2)) |
                    (data & (1 << 3))
                    //| (data & (1 << 4))
                    | ((data & (1 << 5)) >> 1) | ((data & (1 << 6)) >> 1) |
                    ((data & (1 << 7)) >> 1)
                    //| (data & (1 << 8))
                    | ((data & (1 << 9)) >> 2) | ((data & (1 << 10)) >> 2) |
                    ((data & (1 << 11)) >> 1) | ((data & (1 << 12)) >> 2) |
                    ((data & (1 << 13)) >> 2);

  return result;
}

static bitset<2> decode_class(uint16_t data) {
  bitset<2> result;
  result[0] = (data & (1 << 4)) != 0;
  result[1] = (data & (1 << 8)) != 0;
  return result;
}

static bitset<2> decode_plex(uint16_t data) {
  bitset<2> result;
  result[0] = (data & (1 << 14)) != 0;
  result[1] = (data & (1 << 15)) != 0;
  return result;
}

static Attribute decode_attribute(decoder& d) {
  Attribute a;
  a.type = d.get<uint16_t>();
  a.length = d.get<uint16_t>();
  if (d.error()) {
    a.type = ATTRIB_UNKNOWN;
    a.length = 0;
    return a;
  }
  a.data = d.current();
  return a;
}

static udp::endpoint decode_mapped_address(decoder& d) {
  namespace ip = asio::ip;

  d.skip(1);
  auto family = d.get<uint8_t>();
  auto port = d.get<uint16_t>();

  if (d.error()) return udp::endpoint();

  static const uint8_t IPV4 = 0x01;
  static const uint8_t IPV6 = 0x02;

  if (family == IPV4) {
    ip::address_v4 addr;
    decode(d, addr);
    return udp::endpoint(addr, port);
  } else if (family == IPV6) {
    ip::address_v6 addr;
    decode(d, addr);
    return udp::endpoint(addr, port);
  } else {
    d.set_error();
    return udp::endpoint();
  }
}

static udp::endpoint decode_xor_mapped_address(decoder& d) {
  d.skip(1);
  auto family = d.get<uint8_t>();
  auto port = d.get<uint16_t>();

  if (d.error()) return udp::endpoint();

  static const uint8_t IPV4 = 0x01;
  static const uint8_t IPV6 = 0x02;

  if (family == IPV4) {
    auto addr = d.get<uint32_t>();
    port = port ^ (COOKIE >> 16);
    addr = addr ^ COOKIE;
    return udp::endpoint(asio::ip::address_v4(addr), port);
  } else if (family == IPV6) {
    LOG(FATAL) << "Not implemented yet";
    d.set_error();
    return udp::endpoint();
  } else {
    d.set_error();
    return udp::endpoint();
  }
}

//------------------------------------------------------------------------------
struct StunClient::Request {
  asio::io_service& ios;
  RequestID id;
  Handler handler;
  vector<uint8_t> tx_buffer;
  Endpoint tx_endpoint;
  asio::steady_timer timer;
  size_t resend_count;

  Request(asio::io_service& ios, RequestID id, Endpoint tx_endpoint,
          Handler handler)
      : ios(ios),
        id(id),
        handler(handler),
        tx_buffer(512),
        tx_endpoint(tx_endpoint),
        timer(ios),
        resend_count(0) {}

  void exec(asio::error_code error, Endpoint reflected_endpoint) {
    timer.cancel();
    auto h = move(handler);
    ios.post([=]() { h(error, reflected_endpoint); });
  }
};

//------------------------------------------------------------------------------
struct StunClient::State {
  recursive_mutex mutex;
  asio::io_service& ios;
  vector<uint8_t> rx_buffer;
  Endpoint rx_endpoint;
  Requests requests;
  bool was_destroyed;

  State(asio::io_service& ios)
      : ios(ios), rx_buffer(512), was_destroyed(false) {}

  void destroy_requests(asio::error_code error) {
    for (const auto it : requests) {
      auto request = it.second;
      if (!request->handler) continue;
      request->exec(error, Endpoint());
    }
    requests.clear();
  }
};

//------------------------------------------------------------------------------
StunClient::StunClient(udp::socket& socket)
    : _socket(socket),
      _state(make_shared<State>(socket.get_io_service())),
      _request_count(0) {
  lock_guard<recursive_mutex> guard(_state->mutex);

#ifdef NDEBUG
  random_device rd;
  _rand.seed(rd());
#else
  // Using random_device makes valgrind panic.
  // https://bugs.launchpad.net/ubuntu/+source/valgrind/+bug/1501545
  _rand.seed(uint32_t(time(0)));
#endif
}

//------------------------------------------------------------------------------
StunClient::~StunClient() {
  lock_guard<recursive_mutex> guard(_state->mutex);

  _state->was_destroyed = true;

  for (auto& it : _state->requests) {
    it.second->timer.cancel();
  }

  _socket.cancel();
}

//------------------------------------------------------------------------------
void StunClient::reflect(Endpoint server_endpoint, Handler handler) {
  lock_guard<recursive_mutex> guard(_state->mutex);

  RequestID id;

  for (auto& i : id) i = rand();

  auto request = make_shared<Request>(_socket.get_io_service(), id,
                                      server_endpoint, move(handler));

  auto ri = _state->requests.find(server_endpoint);

  if (ri != _state->requests.end()) {
    return execute(ri, asio::error::operation_aborted);
  }

  _state->requests[server_endpoint] = request;

  start_sending(request);

  if (_request_count++ == 0) {
    start_receiving(_state);
  }
}

//------------------------------------------------------------------------------
void StunClient::start_sending(RequestPtr request) {
  lock_guard<recursive_mutex> guard(_state->mutex);
  static const uint16_t size = 0;

  auto& buf = request->tx_buffer;
  buf.resize(HEADER_SIZE, '\0');

  uint8_t* writer = &buf[0];
  uint16_t swappedBind = htons(BIND_METHOD);
  memcpy(writer, &swappedBind, sizeof(uint16_t));
  writer += sizeof(uint16_t);
  uint16_t swappedSize = htons(size);
  memcpy(writer, &swappedSize, sizeof(uint16_t));
  writer += sizeof(uint16_t);
  uint32_t swappedCookie = htonl(COOKIE);
  memcpy(writer, &swappedCookie, sizeof(uint32_t));
  writer += sizeof(uint32_t);
  memcpy(writer, request->id.data(), request->id.size());
  writer += request->id.size();
  if (int(writer - &buf[0]) != HEADER_SIZE) {
    LOG(FATAL) << "Invalid header size: " << int(writer - &buf[0]) << endl;
  }

  auto state = _state;
  _socket.async_send_to(asio::buffer(buf), request->tx_endpoint,
                        [this, state, request](asio::error_code error, size_t) {
                          lock_guard<recursive_mutex> guard(state->mutex);
                          on_send(error, move(request));
                        });
}

//------------------------------------------------------------------------------
void StunClient::on_send(asio::error_code ec, RequestPtr request) {
  if (!request->handler) {
    return;
  }

  if (ec == asio::error::operation_aborted) {
    return;
  }

  uint64_t timeout = 250 * (2 << request->resend_count++);
  request->timer.expires_from_now(chrono::milliseconds(timeout));

  auto state = _state;
  request->timer.async_wait([this, state, request](asio::error_code ec) {
    if (ec == asio::error::operation_aborted) {
      return;
    }
    lock_guard<recursive_mutex> guard(state->mutex);
    if (!request->handler) return;
    start_sending(move(request));
  });
}

//------------------------------------------------------------------------------
void StunClient::start_receiving(StatePtr state) {
  _socket.async_receive_from(asio::buffer(state->rx_buffer), state->rx_endpoint,
                             [this, state](asio::error_code error, size_t size) {
                               lock_guard<recursive_mutex> guard(state->mutex);
                               on_recv(error, size, move(state));
                             });
}

//------------------------------------------------------------------------------
void StunClient::on_recv(asio::error_code error, size_t size, StatePtr state) {
  if (state->was_destroyed) {
    return state->destroy_requests(asio::error::operation_aborted);
  }

  auto& rx_buf = state->rx_buffer;

  if (error) {
    return state->destroy_requests(error);
  }

  auto request_i = _state->requests.find(state->rx_endpoint);

  if (request_i == _state->requests.end()) {
    return start_receiving(move(state));
  }

  if (size < HEADER_SIZE) {
    if (--_request_count) start_receiving(move(state));
    return execute(request_i, asio::error::fault);
  }

  decoder header_d(rx_buf.data(), size);

  auto method_and_class = header_d.get<uint16_t>();
  auto plex = decode_plex(method_and_class);
  auto method = decode_method(method_and_class);
  auto class_ = decode_class(method_and_class);
  auto length = header_d.get<uint16_t>();
  auto cookie = header_d.get<uint32_t>();
  header_d.skip(12);

  if (header_d.error()) {
    if (--_request_count) start_receiving(move(state));
    // TODO: Better error mapping.
    return execute(request_i, asio::error::operation_not_supported);
  }

  if (plex != bitset<2>("00")) {
    return start_receiving(move(state));
  }

  if (cookie != COOKIE) {
    if (--_request_count) start_receiving(move(state));
    // TODO: Better error mapping.
    return execute(request_i, asio::error::fault);
  }

  if (method != BIND_METHOD) {
    if (--_request_count) start_receiving(move(state));
    return execute(request_i, asio::error::operation_not_supported);
  }

  if (class_ != SUCCESS_CLASS) {
    if (--_request_count) start_receiving(move(state));
    // TODO: Better error mapping.
    return execute(request_i, asio::error::fault);
  }

  decoder d(header_d.current(), length);

  while (!d.error()) {
    Attribute a = decode_attribute(d);

    if (d.error()) break;

    decoder dd(a.data, a.length);

    if (a.type == ATTRIB_MAPPED_ADDRESS) {
      Endpoint ep = decode_mapped_address(dd);
      if (dd.error()) break;
      execute(request_i, error, ep);
      break;
    } else if (a.type == ATTRIB_XOR_MAPPED_ADDRESS) {
      Endpoint ep = decode_xor_mapped_address(dd);
      if (dd.error()) break;
      execute(request_i, error, ep);
      break;
    }
  }

  if (state->was_destroyed) {
    return state->destroy_requests(asio::error::operation_aborted);
  }

  if (--_request_count) {
    start_receiving(move(state));
  }

  return;
}

void StunClient::execute(Requests::iterator ri, asio::error_code error,
                         Endpoint endpoint) {
  if (error && !(endpoint == Endpoint())) {
    LOG(FATAL) << "Error: " << error;
  }
  auto r = ri->second;
  _state->requests.erase(ri);
  r->exec(error, endpoint);
}

}  // namespace wga