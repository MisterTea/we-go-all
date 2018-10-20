#ifndef __ET_CRYPTO_HANDLER__
#define __ET_CRYPTO_HANDLER__

#include "Headers.hpp"

#include <sodium.h>

#define SODIUM_FAIL(X)                                           \
  {                                                              \
    int rc = (X);                                                \
    if ((rc) != 0) LOG(FATAL) << "Crypto Error: (" << rc << ")"; \
  }

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
  CryptoHandler(const PrivateKey& _myPrivateKey);
  ~CryptoHandler();

  static PublicKey makePublicFromPrivate(const PrivateKey& privateKey) {
    PublicKey publicKey;
    crypto_scalarmult_base(publicKey.data(), privateKey.data());
    return publicKey;
  }

  static string sign(const PrivateKey& privateKey, const string& s) {
    string retval(s.length() + crypto_sign_BYTES, '\0');
    uint64_t signedLength;
    SODIUM_FAIL(crypto_sign((uint8_t*)&retval[0], &signedLength,
                            (const uint8_t*)&s[0], s.length(),
                            privateKey.data()));
    retval.resize(signedLength);
    return retval;
  }

  static string unsign(const PublicKey& publicKey, const string& s) {
    string retval(s.length(), '\0');
    uint64_t unsignedLength;
    SODIUM_FAIL(crypto_sign_open((uint8_t*)&retval[0], &unsignedLength,
                                 (const uint8_t*)&s[0], s.length(),
                                 publicKey.data()));
    retval.resize(unsignedLength);
    return retval;
  }

  static void init() {
    if (-1 == sodium_init()) {
      LOG(FATAL) << "libsodium init failed";
    }
  }

  static pair<PublicKey, PrivateKey> generateKey() {
    PublicKey publicKey;
    PrivateKey privateKey;
    crypto_box_keypair(publicKey.data(), privateKey.data());
    return make_pair(publicKey, privateKey);
  }

  EncryptedSessionKey generateOutgoingSessionKey(
      const PublicKey& _otherPublicKey);
  bool recieveIncomingSessionKey(const EncryptedSessionKey& otherSessionKey);

  bool canDecrypt() { return incomingSessionKey != emptySessionKey; }
  bool canEncrypt() { return outgoingSessionKey != emptySessionKey; }

  PublicKey getMyPublicKey() { return myPublicKey; }
  PublicKey getOtherPublicKey() { return otherPublicKey; }
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
  PrivateKey myPublicKey;
  PrivateKey myPrivateKey;
  PublicKey otherPublicKey;
  SessionKey outgoingSessionKey;
  SessionKey incomingSessionKey;
  SessionKey emptySessionKey;

 private:
};
}  // namespace wga

#endif  // __ET_CRYPTO_HANDLER__
