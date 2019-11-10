#include "BiDirectionalRpc.hpp"

#include "TimeHandler.hpp"

namespace wga {
bool ALL_RPC_FLAKY = false;

BiDirectionalRpc::BiDirectionalRpc()
    : processedReplies(128 * 1024),
      onBarrier(0),
      onId(0),
      flaky(ALL_RPC_FLAKY),
      shuttingDown(false),
      clockSynchronizer(GlobalClock::timeHandler) {}

BiDirectionalRpc::~BiDirectionalRpc() {}

void BiDirectionalRpc::sendShutdown() {
  VLOG(1) << "SHUTTING DOWN RPC";
  string s(1, '\0');
  s[0] = SHUTDOWN;
  send(s);
}

void BiDirectionalRpc::shutdown() {
  lock_guard<recursive_mutex> guard(mutex);
  shuttingDown = true;
}

void BiDirectionalRpc::initTimeShift() {
  // Send a bunch of heartbeats to stabilize any timeshift calculations
  // This has to be done in a separate thread from heartbeat()
  for (int a = 0; a < 20; a++) {
    LOG(INFO) << "ON INITIAL PING: " << a;
    RpcId reqId;
    {
      lock_guard<recursive_mutex> guard(mutex);
      reqId = request("PING");
    }
    while (true) {
      {
        lock_guard<recursive_mutex> guard(mutex);
        if (hasIncomingReplyWithId(reqId) || hasProcessedReplyWithId(reqId)) {
          break;
        }
      }
      VLOG(1) << "WAITING ON REPLY: " << reqId.id;
      microsleep(1000);
    }
  }
}

void BiDirectionalRpc::heartbeat() {
  lock_guard<recursive_mutex> guard(mutex);
  // TODO: If the outgoingReplies/requests is high, and we have recently
  // received data, flush a lot of data out
  LOG(INFO) << "BEAT: " << int64_t(this);
  if (!outgoingReplies.empty() || !outgoingRequests.empty()) {
    LOG(INFO) << "RESENDING MESSAGES: " << outgoingRequests.size() << " "
            << outgoingReplies.size();
    resendRandomOutgoingMessage();
  } else if (readyToSend()) {
    LOG(INFO) << "SENDING HEARTBEAT";
    requestOneWay("PING");
  }
}

void BiDirectionalRpc::resendRandomOutgoingMessage() {
  if (!outgoingReplies.empty()) {
    // Re-send a random reply
    DRAW_FROM_UNORDERED(it, outgoingReplies);
    sendReply(it->first, it->second, true);
  } else if (!outgoingRequests.empty()) {
    // Re-send a random request
    DRAW_FROM_UNORDERED(it, outgoingRequests);
    sendRequest(it->first, it->second, true);
  } else {
  }
}

bool BiDirectionalRpc::receive(const string& message) {
  lock_guard<recursive_mutex> guard(mutex);
  VLOG(1) << "Receiving message with length " << message.length();
  MessageReader reader;
  reader.load(message);
  RpcHeader header = (RpcHeader)reader.readPrimitive<unsigned char>();
  if (flaky && (rand() % 100 == 0)) {
    // Pretend we never got the message
    LOG(INFO) << "FLAKE";
  } else {
    VLOG(1) << "GOT PACKET WITH HEADER " << header;
    switch (header) {
      case REQUEST: {
        while (reader.sizeRemaining()) {
          RpcId rpcId = reader.readClass<RpcId>();
          string payload = reader.readPrimitive<string>();
          if (!validatePacket(rpcId, payload)) {
            return false;
          }
          handleRequest(rpcId, payload);
        }
      } break;
      case REPLY: {
        while (reader.sizeRemaining()) {
          RpcId uid = reader.readClass<RpcId>();
          int64_t requestReceiveTime = reader.readPrimitive<int64_t>();
          int64_t replySendTime = reader.readPrimitive<int64_t>();
          string payload = reader.readPrimitive<string>();
          if (!validatePacket(uid, payload)) {
            return false;
          }
          handleReply(uid, payload, requestReceiveTime, replySendTime);
        }
      } break;
      case ACKNOWLEDGE: {
        RpcId uid = reader.readClass<RpcId>();
        string payload = reader.readPrimitive<string>();
        if (!validatePacket(uid, payload)) {
          return false;
        }
        VLOG(1) << "ACK UID " << uid.str();
        for (auto it = outgoingReplies.begin(); it != outgoingReplies.end();
             it++) {
          VLOG(1) << "REPLY UID " << it->first.str();
          if (it->first == uid) {
            clockSynchronizer.eraseRequestRecieveTime(it->first);
            outgoingReplies.erase(it);
            break;
          }
        }
      } break;
      case SHUTDOWN: {
        LOG(INFO) << "GOT SHUT DOWN";
        {
          string s(1, '\0');
          s[0] = SHUTDOWN_REPLY;
          send(s);
        }
        shutdown();
      } break;
      case SHUTDOWN_REPLY: {
        LOG(INFO) << "GOT SHUT DOWN";
        shutdown();
      } break;
      default: {
        LOGFATAL << "Got invalid header: " << header << " in message "
                 << message;
      }
    }
  }
  return true;
}

void BiDirectionalRpc::handleRequest(const RpcId& rpcId,
                                     const string& payload) {
  VLOG(1) << "GOT REQUEST: " << rpcId.str();

  bool skip = (incomingRequests.find(rpcId) != incomingRequests.end());
  if (!skip) {
    for (const auto& it : outgoingReplies) {
      if (it.first == rpcId) {
        // We already processed this request.  Send the reply again
        skip = true;
        sendReply(it.first, it.second, false);
        break;
      }
    }
  }

  if (!skip) {
    addIncomingRequest(IdPayload(rpcId, payload));
    auto it = incomingRequests.find(rpcId);
    if (it != incomingRequests.end() && it->second == "PING") {
      // heartbeat, send reply right away
      reply(rpcId, "PONG");
      return;
    }
  }
}

void BiDirectionalRpc::handleReply(const RpcId& rpcId, const string& payload,
                                   int64_t requestReceiveTime,
                                   int64_t replySendTime) {
  VLOG(1) << "GOT REPLY: " << rpcId.id;
  bool skip = false;
  if (incomingReplies.find(rpcId) != incomingReplies.end() ||
      processedReplies.exists(rpcId)) {
    // We already received this reply.  Send acknowledge again and skip.
    sendAcknowledge(rpcId);
    skip = true;
  }
  if (!skip) {
    // Stop sending the request once you get the reply
    bool deletedRequest = false;
    for (auto it = outgoingRequests.begin(); it != outgoingRequests.end();
         it++) {
      if (it->first == rpcId) {
        outgoingRequests.erase(it);
        deletedRequest = true;
        tryToSendBarrier();
        break;
      }
    }
    if (deletedRequest) {
      auto it = oneWayRequests.find(rpcId);
      if (it != oneWayRequests.end()) {
        // Remove this from the set of one way requests and don't bother
        // adding a reply.
        oneWayRequests.erase(it);
      } else {
        // Add a reply to be processed
        addIncomingReply(rpcId, payload);
      }
      sendAcknowledge(rpcId);
      clockSynchronizer.handleReply(rpcId, requestReceiveTime, replySendTime);
    } else {
      // We must have processed both this request and reply.  Send the
      // acknowledge again.
      sendAcknowledge(rpcId);
    }
  }
}

RpcId BiDirectionalRpc::request(const string& payload) {
  lock_guard<recursive_mutex> guard(mutex);
  auto fullUuid = sole::uuid4();
  auto uuid = RpcId(onBarrier, fullUuid.cd);
  auto idPayload = IdPayload(uuid, payload);
  requestWithId(idPayload);
  return uuid;
}

void BiDirectionalRpc::requestOneWay(const string& payload) {
  lock_guard<recursive_mutex> guard(mutex);
  auto fullUuid = sole::uuid4();
  auto uuid = RpcId(onBarrier, fullUuid.cd);
  oneWayRequests.insert(uuid);
  auto idPayload = IdPayload(uuid, payload);
  requestWithId(idPayload);
}

void BiDirectionalRpc::requestWithId(const IdPayload& idPayload) {
  lock_guard<recursive_mutex> guard(mutex);
  if (outgoingRequests.empty() ||
      outgoingRequests.begin()->first.barrier == onBarrier) {
    // We can send the request immediately
    outgoingRequests[idPayload.id] = idPayload.payload;
    clockSynchronizer.createRequest(idPayload.id);
    sendRequest(idPayload.id, idPayload.payload, false);
  } else {
    // We have to wait for existing requests from an older barrier
    delayedRequests[idPayload.id] = idPayload.payload;
  }
}

void BiDirectionalRpc::reply(const RpcId& rpcId, const string& payload) {
  lock_guard<recursive_mutex> guard(mutex);
  auto it = incomingRequests.find(rpcId);
  if (it == incomingRequests.end()) {
    LOGFATAL << "Tried to reply but had no request: " << rpcId.id;
  }
  incomingRequests.erase(it);
  outgoingReplies[rpcId] = payload;
  sendReply(rpcId, payload, false);
}

void BiDirectionalRpc::tryToSendBarrier() {
  if (delayedRequests.empty()) {
    // Nothing to send
    return;
  }
  if (outgoingRequests.empty()) {
    // There are no outgoing requests, we can send the next barrier
    int64_t lowestBarrier = delayedRequests.begin()->first.barrier;
    for (const auto& it : delayedRequests) {
      lowestBarrier = min(lowestBarrier, it.first.barrier);
    }

    for (auto it = delayedRequests.begin(); it != delayedRequests.end();) {
      if (it->first.barrier == lowestBarrier) {
        outgoingRequests[it->first] = it->second;
        clockSynchronizer.createRequest(it->first);
        sendRequest(it->first, it->second, false);
        it = delayedRequests.erase(it);
      } else {
        it++;
      }
    }
  }
}

void BiDirectionalRpc::sendRequest(const RpcId& id, const string& payload, bool batch) {
  VLOG(1) << "SENDING REQUEST: " << id.str();
  MessageWriter writer;
  writer.start();
  set<RpcId> rpcsSent;

  rpcsSent.insert(id);
  writer.writePrimitive<unsigned char>(REQUEST);
  writer.writeClass<RpcId>(id);
  writer.writePrimitive<string>(payload);
  if (batch) {
    // Try to attach more requests to this packet
    int i = 0;
    while (!outgoingRequests.empty() &&
      rpcsSent.size() < outgoingRequests.size()) {
      DRAW_FROM_UNORDERED(it, outgoingRequests);
      if (rpcsSent.find(it->first) != rpcsSent.end()) {
        // Drew an rpc that's already in the packet.  Just bail for now, maybe in
        // the future do something more clever.
        break;
      }
      int size = int(sizeof(RpcId) + it->second.length());
      if (size + writer.size() > 400) {
        // Too big
        break;
      }
      i++;
      rpcsSent.insert(it->first);
      writer.writeClass<RpcId>(it->first);
      writer.writePrimitive<string>(it->second);
    }
    VLOG(1) << "Attached " << i << " extra packets";
  }
  send(writer.finish());
}

void BiDirectionalRpc::sendReply(const RpcId& id, const string& payload, bool batch) {
  lock_guard<recursive_mutex> guard(mutex);
  VLOG(1) << "SENDING REPLY: " << id.str();
  set<RpcId> rpcsSent;

  rpcsSent.insert(id);
  MessageWriter writer;
  writer.start();
  writer.writePrimitive<unsigned char>(REPLY);
  writer.writeClass<RpcId>(id);
  auto replyDuration = clockSynchronizer.getReplyDuration(id);
  writer.writePrimitive<int64_t>(replyDuration.first);
  writer.writePrimitive<int64_t>(replyDuration.second);
  writer.writePrimitive<string>(payload);
  if (batch) {
    // Try to attach more replies to this packet
    int i = 0;
    while (!outgoingReplies.empty() && rpcsSent.size() < outgoingReplies.size()) {
      DRAW_FROM_UNORDERED(it, outgoingReplies);
      if (rpcsSent.find(it->first) != rpcsSent.end()) {
        // Drew an rpc that's already in the packet.  Just bail for now, maybe in
        // the future do something more clever.
        break;
      }
      int size = int(sizeof(RpcId) + it->second.length());
      if (size + writer.size() > 400) {
        // Too big
        break;
      }
      i++;
      rpcsSent.insert(it->first);
      writer.writeClass<RpcId>(it->first);
      auto replyDuration = clockSynchronizer.getReplyDuration(it->first);
      writer.writePrimitive<int64_t>(replyDuration.first);
      writer.writePrimitive<int64_t>(replyDuration.second);
      writer.writePrimitive<string>(it->second);
    }
    VLOG(1) << "Attached " << i << " extra packets";
  }
  send(writer.finish());
}

void BiDirectionalRpc::sendAcknowledge(const RpcId& uid) {
  MessageWriter writer;
  writer.start();
  writer.writePrimitive<unsigned char>(ACKNOWLEDGE);
  writer.writeClass<RpcId>(uid);
  writer.writePrimitive<string>("ACK_OK");
  send(writer.finish());
}

void BiDirectionalRpc::addIncomingRequest(const IdPayload& idPayload) {
  lock_guard<recursive_mutex> guard(mutex);
  clockSynchronizer.receiveRequest(idPayload.id);
  incomingRequests.insert(make_pair(idPayload.id, idPayload.payload));
}

}  // namespace wga
