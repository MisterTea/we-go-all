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
  CryptoHandler(const PrivateKey& _myPrivateKey,
                const PublicKey& _otherPublicKey);
  ~CryptoHandler();

  static PublicKey makePublicFromPrivate(const PrivateKey& privateKey) {
    PublicKey publicKey;
    crypto_scalarmult_base(publicKey.data(), privateKey.data());
    return publicKey;
  }

  static string sign(const PrivateKey& privateKey, const string& s) {
    string retval(s.length() + crypto_sign_BYTES, '\0');
    unsigned long long signedLength;
    SODIUM_FAIL(crypto_sign((uint8_t*)&retval[0], &signedLength,
                            (const uint8_t*)&s[0], s.length(),
                            privateKey.data()));
    retval.resize(signedLength);
    return retval;
  }

  static optional<string> unsign(const PublicKey& publicKey, const string& s) {
    string retval(s.length(), '\0');
    unsigned long long unsignedLength;
    if (crypto_sign_open((uint8_t*)&retval[0], &unsignedLength,
                         (const uint8_t*)&s[0], s.length(), publicKey.data())) {
      return nullopt;
    }
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

  template <typename T>
  static string keyToString(const T& key) {
    string s;
    FATAL_IF_FALSE(
        Base64::Encode(string((const char*)key.data(), key.size()), &s));
    return s;
  }

  template <typename T>
  static T stringToKey(const string& s) {
    T key;
    FATAL_IF_FALSE(
        Base64::Decode(&s[0], s.length(), (char*)key.data(), key.size()));
    return key;
  }

  EncryptedSessionKey generateOutgoingSessionKey();
  bool recieveIncomingSessionKey(const EncryptedSessionKey& otherSessionKey);

  bool canDecrypt() { return incomingSessionKey != emptySessionKey; }
  bool canEncrypt() { return outgoingSessionKey != emptySessionKey; }

  PublicKey getMyPublicKey() { return myPublicKey; }
  PublicKey getOtherPublicKey() { return otherPublicKey; }

  string encrypt(const string& buffer);
  optional<string> decrypt(const string& buffer);

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
