#include "BiDirectionalRpc.hpp"

namespace wga {
BiDirectionalRpc::BiDirectionalRpc() : onBarrier(0), onId(0), flaky(false) {}

BiDirectionalRpc::~BiDirectionalRpc() {}

void BiDirectionalRpc::shutdown() {}

void BiDirectionalRpc::heartbeat() {
  VLOG(1) << "BEAT";
  if (!outgoingReplies.empty() || !outgoingRequests.empty()) {
    resendRandomOutgoingMessage();
  } else {
    VLOG(1) << "SENDING HEARTBEAT";
    string s = "0";
    s[0] = HEARTBEAT;
    send(s);
  }
}

void BiDirectionalRpc::resendRandomOutgoingMessage() {
  if (!outgoingReplies.empty() &&
      (outgoingRequests.empty() || rand() % 2 == 0)) {
    // Re-send a random reply
    DRAW_FROM_UNORDERED(it, outgoingReplies);
    sendReply(*it);
  } else if (!outgoingRequests.empty()) {
    // Re-send a random request
    DRAW_FROM_UNORDERED(it, outgoingRequests);
    sendRequest(*it);
  } else {
  }
}

void BiDirectionalRpc::receive(const string& message) {
  reader.load(message);
  RpcHeader header = (RpcHeader)reader.readPrimitive<unsigned char>();
  if (flaky && rand() % 2 == 0) {
    // Pretend we never got the message
    VLOG(1) << "FLAKE";
  } else {
    if (header != HEARTBEAT) {
      VLOG(1) << "GOT PACKET WITH HEADER " << header;
    }
    switch (header) {
      case HEARTBEAT: {
        // MultiEndpointHandler deals with keepalive
      } break;
      case REQUEST: {
        RpcId uid = reader.readClass<RpcId>();
        VLOG(1) << "GOT REQUEST: " << uid.str();

        bool skip = false;
        for (auto it : incomingRequests) {
          if (it.first == uid) {
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
          addIncomingRequest(IdPayload(uid, payload));
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
            auto it = oneWayRequests.find(uid);
            if (it != oneWayRequests.end()) {
              // Remove this from the set of one way requests and don't bother
              // adding a reply.
              oneWayRequests.erase(it);
            } else {
              // Add a reply to be processed
              addIncomingReply(uid, payload);
            }
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
        VLOG(1) << "ACK UID " << uid.str();
        for (auto it = outgoingReplies.begin(); it != outgoingReplies.end();
             it++) {
          VLOG(1) << "REPLY UID " << it->id.str();
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
}

RpcId BiDirectionalRpc::request(const string& payload) {
  auto fullUuid = sole::uuid4();
  auto uuid = RpcId(onBarrier, fullUuid.cd);
  auto idPayload = IdPayload(uuid, payload);
  requestWithId(idPayload);
  return uuid;
}

void BiDirectionalRpc::requestNoReply(const string& payload) {
  auto fullUuid = sole::uuid4();
  auto uuid = RpcId(onBarrier, fullUuid.cd);
  oneWayRequests.insert(uuid);
  auto idPayload = IdPayload(uuid, payload);
  requestWithId(idPayload);
}

void BiDirectionalRpc::requestWithId(const IdPayload& idPayload) {
  if (outgoingRequests.empty() ||
      outgoingRequests.begin()->id.barrier == onBarrier) {
    // We can send the request immediately
    outgoingRequests.push_back(idPayload);
    sendRequest(outgoingRequests.back());
  } else {
    // We have to wait for existing requests from an older barrier
    delayedRequests.push_back(idPayload);
  }
}

void BiDirectionalRpc::reply(const RpcId& rpcId, const string& payload) {
  incomingRequests.erase(incomingRequests.find(rpcId));
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
               outgoingRequests.begin()->id.barrier) {
      // Part of the same barrier, keep sending
      outgoingRequests.push_back(delayedRequests.front());
      delayedRequests.pop_front();
      sendRequest(outgoingRequests.back());
    }
  }
}

void BiDirectionalRpc::sendRequest(const IdPayload& idPayload) {
  VLOG(1) << "SENDING REQUEST: " << idPayload.id.str();
  writer.start();
  writer.writePrimitive<unsigned char>(REQUEST);
  writer.writeClass<RpcId>(idPayload.id);
  writer.writePrimitive<string>(idPayload.payload);
  send(writer.finish());
}

void BiDirectionalRpc::sendReply(const IdPayload& idPayload) {
  VLOG(1) << "SENDING REPLY: " << idPayload.id.str();
  writer.start();
  writer.writePrimitive<unsigned char>(REPLY);
  writer.writeClass<RpcId>(idPayload.id);
  writer.writePrimitive<string>(idPayload.payload);
  send(writer.finish());
}

void BiDirectionalRpc::sendAcknowledge(const RpcId& uid) {
  writer.start();
  writer.writePrimitive<unsigned char>(ACKNOWLEDGE);
  writer.writeClass<RpcId>(uid);
  send(writer.finish());
}

}  // namespace wga
