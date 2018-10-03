#include "BiDirectionalRpc.hpp"

namespace wga {
BiDirectionalRpc::BiDirectionalRpc(shared_ptr<asio::io_service> _ioService,
                                   shared_ptr<udp::socket> _localSocket,
                                   shared_ptr<udp::socket> _remoteSocket,
                                   const udp::endpoint& _remoteEndpoint)
    : ioService(_ioService),
      localSocket(_localSocket),
      remoteSocket(_remoteSocket),
      remoteEndpoint(_remoteEndpoint),
      onBarrier(0),
      onId(0),
      flaky(false) {
  dist = std::uniform_int_distribution<uint64_t>(0, UINT64_MAX);

  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), remoteSource,
      std::bind(&BiDirectionalRpc::handleRecieve, this, std::placeholders::_1,
                std::placeholders::_2));
}

BiDirectionalRpc::~BiDirectionalRpc() {
  if (localSocket.get()) {
    LOG(FATAL) << "Tried to destroy an RPC instance without calling shutdown";
  }
  if (remoteSocket.get()) {
    LOG(FATAL) << "Tried to destroy an RPC instance without calling shutdown";
  }
}

void BiDirectionalRpc::shutdown() {
  LOG(INFO) << "CLOSING SOCKET";
  localSocket->close();
  remoteSocket->close();
  LOG(INFO) << "KILLING SOCKET";
  localSocket.reset();
  remoteSocket.reset();
}

void BiDirectionalRpc::heartbeat() {
  LOG(INFO) << "BEAT";
  if (!outgoingReplies.empty() || !outgoingRequests.empty()) {
    resendRandomOutgoingMessage();
  } else {
    LOG(INFO) << "SENDING HEARTBEAT";
    string s = "0";
    s[0] = HEARTBEAT;
    post(s);
  }
}

void BiDirectionalRpc::resendRandomOutgoingMessage() {
  if (!outgoingReplies.empty() &&
      (outgoingRequests.empty() || rand() % 2 == 0)) {
    // Re-send a random reply
    auto it = outgoingReplies.begin();
    std::advance(it, rand() % outgoingReplies.size());
    sendReply(*it);
  } else if (!outgoingRequests.empty()) {
    // Re-send a random request
    auto it = outgoingRequests.begin();
    std::advance(it, rand() % outgoingRequests.size());
    sendRequest(*it);
  } else {
  }
}

void BiDirectionalRpc::handleRecieve(const asio::error_code& error,
                                     std::size_t bytesTransferredUnsigned) {
  LOG(INFO) << "GOT PACKET FROM " << remoteSource;
  int bytesTransferred = (int)bytesTransferredUnsigned;
  reader.load(receiveBuffer, bytesTransferred);
  RpcHeader header = (RpcHeader)reader.readPrimitive<unsigned char>();
  if (flaky && rand() % 2 == 0) {
    // Pretend we never got the message
  } else {
    if (header != HEARTBEAT) {
      LOG(INFO) << "GOT PACKET WITH HEADER " << header;
    }
    switch (header) {
      case HEARTBEAT: {
        // TODO: Update keepalive time
      } break;
      case REQUEST: {
        RpcId uid = reader.readClass<RpcId>();
        LOG(INFO) << "GOT REQUEST: " << uid.str();

        bool skip = false;
        for (const IdPayload& it : incomingRequests) {
          if (it.id == uid) {
            // We already received this request.  Skip
            skip = true;
            break;
          }
        }
        if (!skip) {
          for (const IdPayload& it : outgoingReplies) {
            if (it.id == uid) {
              // We already processed this request.  Send the reply again
              skip = true;
              sendReply(it);
              break;
            }
          }
        }
        if (!skip) {
          string payload = reader.readPrimitive<string>();
          incomingRequests.push_back(IdPayload(uid, payload));
        }
      } break;
      case REPLY: {
        RpcId uid = reader.readClass<RpcId>();

        bool skip = false;
        if (incomingReplies.find(uid) != incomingReplies.end()) {
          // We already received this reply.  Send acknowledge again and skip.
          sendAcknowledge(uid);
          skip = true;
        }
        if (!skip) {
          // Stop sending the request once you get the reply
          bool deletedRequest = false;
          for (auto it = outgoingRequests.begin(); it != outgoingRequests.end();
               it++) {
            if (it->id == uid) {
              outgoingRequests.erase(it);
              deletedRequest = true;
              tryToSendBarrier();
              break;
            }
          }
          if (deletedRequest) {
            string payload = reader.readPrimitive<string>();
            incomingReplies.emplace(uid, payload);
            sendAcknowledge(uid);
          } else {
            // We must have processed both this request and reply.  Send the
            // acknowledge again.
            sendAcknowledge(uid);
          }

          // When we complete an reply, try to send a new message
          resendRandomOutgoingMessage();
        }
      } break;
      case ACKNOWLEDGE: {
        RpcId uid = reader.readClass<RpcId>();
        LOG(INFO) << "ACK UID " << uid.str();
        for (auto it = outgoingReplies.begin(); it != outgoingReplies.end();
             it++) {
          LOG(INFO) << "REPLY UID " << it->id.str();
          if (it->id == uid) {
            outgoingReplies.erase(it);

            // When we complete an RPC, try to send a new message
            resendRandomOutgoingMessage();
            break;
          }
        }
      } break;
    }
  }

  localSocket->async_receive_from(
      asio::buffer(receiveBuffer), remoteSource,
      std::bind(&BiDirectionalRpc::handleRecieve, this, std::placeholders::_1,
                std::placeholders::_2));
}

RpcId BiDirectionalRpc::request(const string& payload) {
  auto fullUuid = sole::uuid4();
  auto uuid = RpcId(onBarrier, fullUuid.cd);
  auto idPayload = IdPayload(uuid, payload);
  requestWithId(idPayload);
  return uuid;
}

void BiDirectionalRpc::requestWithId(const IdPayload& idPayload) {
  if (outgoingRequests.empty() ||
      outgoingRequests.front().id.barrier == onBarrier) {
    // We can send the request immediately
    outgoingRequests.push_back(idPayload);
    sendRequest(outgoingRequests.back());
  } else {
    // We have to wait for existing requests from an older barrier
    delayedRequests.push_back(idPayload);
  }
}

void BiDirectionalRpc::reply(const RpcId& rpcId, const string& payload) {
  outgoingReplies.push_back(IdPayload(rpcId, payload));
  sendReply(outgoingReplies.back());
}

void BiDirectionalRpc::tryToSendBarrier() {
  if (delayedRequests.empty()) {
    // Nothing to send
    return;
  }
  if (outgoingRequests.empty()) {
    // There are no outgoing requests, we can send the next barrier
    outgoingRequests.push_back(delayedRequests.front());
    delayedRequests.pop_front();
    sendRequest(outgoingRequests.back());

    while (!delayedRequests.empty() &&
           delayedRequests.front().id.barrier ==
               outgoingRequests.front().id.barrier) {
      // Part of the same barrier, keep sending
      outgoingRequests.push_back(delayedRequests.front());
      delayedRequests.pop_front();
      sendRequest(outgoingRequests.back());
    }
  }
}

void BiDirectionalRpc::sendRequest(const IdPayload& idPayload) {
  LOG(INFO) << "SENDING REQUEST: " << idPayload.id.str() << " TO "
            << remoteEndpoint;
  writer.start();
  writer.writePrimitive<unsigned char>(REQUEST);
  writer.writeClass<RpcId>(idPayload.id);
  writer.writePrimitive<string>(idPayload.payload);
  post(writer.finish());
}

void BiDirectionalRpc::sendReply(const IdPayload& idPayload) {
  LOG(INFO) << "SENDING REPLY: " << idPayload.id.str() << " TO "
            << remoteEndpoint;
  writer.start();
  writer.writePrimitive<unsigned char>(REPLY);
  writer.writeClass<RpcId>(idPayload.id);
  writer.writePrimitive<string>(idPayload.payload);
  post(writer.finish());
}

void BiDirectionalRpc::sendAcknowledge(const RpcId& uid) {
  writer.start();
  writer.writePrimitive<unsigned char>(ACKNOWLEDGE);
  writer.writeClass<RpcId>(uid);
  post(writer.finish());
}

void BiDirectionalRpc::post(const string& s) {
  ioService->post([this, s]() {
    int bytesSent = remoteSocket->send_to(asio::buffer(s), remoteEndpoint);
    LOG(INFO) << bytesSent << " bytes sent";
  });
}
}  // namespace wga
