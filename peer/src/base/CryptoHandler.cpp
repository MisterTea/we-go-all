#include "CryptoHandler.hpp"

namespace wga {

CryptoHandler::CryptoHandler(const PrivateKey& _myPrivateKey,
                             const PublicKey& _otherPublicKey)
    : myPrivateKey(_myPrivateKey), otherPublicKey(_otherPublicKey) {
  static bool calledInit = false;
  if (!calledInit) {
    if (-1 == sodium_init()) {
      LOGFATAL << "libsodium init failed";
    }
  }
  calledInit = true;
  myPublicKey = CryptoHandler::makePublicFromPrivate(myPrivateKey);
  incomingSessionKey.fill(0);
  outgoingSessionKey.fill(0);
  emptySessionKey.fill(0);
}

CryptoHandler::~CryptoHandler() {}

EncryptedSessionKey CryptoHandler::generateOutgoingSessionKey() {
  if (outgoingSessionKey != emptySessionKey) {
    LOGFATAL << "Tried to generate a session key when one already exists!";
  }
  randombytes_buf(outgoingSessionKey.data(), sizeof(SessionKey));

  EncryptedSessionKey retval;
  randombytes_buf(retval.data(), sizeof(Nonce));

  SODIUM_FAIL(crypto_box_easy(retval.data() + sizeof(Nonce),  // Encrypted data
                              outgoingSessionKey.data(),      // Original data
                              sizeof(SessionKey),     // Original data size
                              retval.data(),          // Nonce
                              otherPublicKey.data(),  // Public Key
                              myPrivateKey.data()     // Private Key
                              ));
  return retval;
}

bool CryptoHandler::recieveIncomingSessionKey(
    const EncryptedSessionKey& otherSessionKey) {
  if (incomingSessionKey != emptySessionKey) {
    LOGFATAL << "Tried to receive a session key when one already exists!";
  }

  Nonce nonce;
  memcpy(nonce.data(), otherSessionKey.data(), sizeof(Nonce));
  if (crypto_box_open_easy(incomingSessionKey.data(),
                           otherSessionKey.data() + sizeof(Nonce),
                           otherSessionKey.size() - sizeof(Nonce), nonce.data(),
                           otherPublicKey.data(), myPrivateKey.data()) != 0) {
    return false;
  }
  return true;
}

string CryptoHandler::encrypt(const string& buffer) {
  if (outgoingSessionKey == emptySessionKey) {
    LOGFATAL << "Tried to use a session key when one doesn't exist!";
  }
  string retval(
      crypto_secretbox_NONCEBYTES + buffer.length() + crypto_secretbox_MACBYTES,
      '\0');
  randombytes_buf(&retval[0], sizeof(Nonce));
  SODIUM_FAIL(crypto_secretbox_easy(
      (uint8_t*)&retval[sizeof(Nonce)],  // Encrypted message
      (const uint8_t*)buffer.c_str(),    // Original message
      buffer.length(),                   // Original message length
      (const uint8_t*)&retval[0],        // Nonce
      outgoingSessionKey.data()          // Session Key
      ));
  return retval;
}

optional<string> CryptoHandler::decrypt(const string& buffer) {
  if (incomingSessionKey == emptySessionKey) {
    LOGFATAL << "Tried to use a session key when one doesn't exist!";
  }
  if (buffer.length() <=
      (crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES)) {
    return nullopt;
  }
  string retval((buffer.length() - crypto_secretbox_NONCEBYTES) -
                    crypto_secretbox_MACBYTES,
                '\0');
  if (crypto_secretbox_open_easy(
          (uint8_t*)&retval[0],  // Decrypted message
          (const uint8_t*)(buffer.c_str() +
                           sizeof(Nonce)),  // Encrypted message
          buffer.length() - sizeof(Nonce),  // Encrypted message length
          (const uint8_t*)buffer.c_str(),   // Nonce
          incomingSessionKey.data()         // Session Key
          ) != 0) {
    return nullopt;
  }
  return retval;
}
}  // namespace wga
