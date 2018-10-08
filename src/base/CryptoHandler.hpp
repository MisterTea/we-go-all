#ifndef __ET_CRYPTO_HANDLER__
#define __ET_CRYPTO_HANDLER__

#include "Headers.hpp"

#include <sodium.h>

namespace wga {
typedef array<uint8_t, crypto_secretbox_NONCEBYTES> Nonce;
typedef array<uint8_t, crypto_secretbox_KEYBYTES> SessionKey;
typedef array<uint8_t, crypto_box_PUBLICKEYBYTES> PublicKey;
typedef array<uint8_t, crypto_box_SECRETKEYBYTES> PrivateKey;

typedef array<uint8_t, crypto_secretbox_NONCEBYTES + crypto_box_MACBYTES +
                           crypto_secretbox_KEYBYTES>
    EncryptedSessionKey;

class CryptoHandler {
 public:
  CryptoHandler(const PrivateKey& _myPrivateKey, const PublicKey& _myPublicKey);
  CryptoHandler();
  ~CryptoHandler();

  EncryptedSessionKey generateOutgoingSessionKey(
      const PublicKey& _otherPublicKey);
  bool recieveIncomingSessionKey(const EncryptedSessionKey& otherSessionKey);

  bool canDecrypt() { return incomingSessionKey != emptySessionKey; }
  bool canEncrypt() { return outgoingSessionKey != emptySessionKey; }

  PublicKey getMyPublicKey() { return myPublicKey; }
  void setOtherPublicKey(const PublicKey& _otherPublicKey) {
    otherPublicKey = _otherPublicKey;
  }

  string encrypt(const string& buffer);
  pair<bool, string> tryToDecrypt(const string& buffer);
  string decrypt(const string& buffer) {
    auto retval = tryToDecrypt(buffer);
    if (!retval.first) {
      LOG(FATAL) << "Decrypt failed";
    }
    return retval.second;
  }

 protected:
  void init();
  PrivateKey myPrivateKey;
  PrivateKey myPublicKey;
  PublicKey otherPublicKey;
  SessionKey outgoingSessionKey;
  SessionKey incomingSessionKey;
  SessionKey emptySessionKey;

 private:
};
}  // namespace wga

#endif  // __ET_CRYPTO_HANDLER__
