#ifndef __WGA_STUN_CLIENT_HPP__
#define __WGA_STUN_CLIENT_HPP__

#include "Headers.hpp"

// Implementation of the STUN client according to RFC5389
// https://tools.ietf.org/html/rfc5389


namespace wga {

class StunClient {
  struct State;
  struct Request;
  using udp = asio::ip::udp;
  using Endpoint = udp::endpoint;
  using Handler = function<void(asio::error_code, Endpoint)>;
  using StatePtr = shared_ptr<State>;
  using RequestPtr = shared_ptr<Request>;
  using RequestID = array<char, 12>;
  using Bytes = vector<uint8_t>;
  using Requests = map<Endpoint, RequestPtr>;

 public:
  StunClient(udp::socket&);
  void reflect(Endpoint server_endpoint, Handler);
  ~StunClient();

 private:
  void start_sending(RequestPtr);
  void start_receiving(StatePtr);
  void on_send(asio::error_code, RequestPtr);
  void on_recv(asio::error_code, size_t, StatePtr);
  void execute(Requests::iterator, asio::error_code, Endpoint e = Endpoint());

 private:
  mt19937 _rand;
  udp::socket& _socket;
  StatePtr _state;
  size_t _request_count;
};

}  // namespace club

#endif  // ifndef __WGA_STUN_CLIENT_HPP__