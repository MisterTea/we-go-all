#include "LocalIpFetcher.hpp"

namespace wga {
vector<string> LocalIpFetcher::fetch(int port, bool udp) {
  set<string> localIps;
#ifdef _WIN32

#define FATAL_FAIL_WINDOWS(X)                                             \
  if (((X) != 0)) {                                                       \
    LPVOID lpMsgBuf;                                                      \
    int e;                                                                \
                                                                          \
    lpMsgBuf = (LPVOID) "Unknown error";                                  \
    e = WSAGetLastError();                                                \
    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |                    \
                          FORMAT_MESSAGE_FROM_SYSTEM |                    \
                          FORMAT_MESSAGE_IGNORE_INSERTS,                  \
                      NULL, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
                      (LPTSTR)&lpMsgBuf, 0, NULL)) {                      \
      fprintf(stderr, "Error %d: %S\n", e, lpMsgBuf);                     \
      LocalFree(lpMsgBuf);                                                \
    } else                                                                \
      fprintf(stderr, "Error %d\n", e);                                   \
  }

#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

  /* Declare and initialize variables */

  DWORD dwSize = 0;
  DWORD dwRetVal = 0;

  unsigned int i = 0;

  // Set the flags to pass to GetAdaptersAddresses
  ULONG flags =
      GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;

  // default to unspecified address family (both)
  ULONG family = AF_INET;  // TODO: Support ipv6

  LPVOID lpMsgBuf = NULL;

  PIP_ADAPTER_ADDRESSES pAddresses = NULL;
  ULONG outBufLen = 0;
  ULONG Iterations = 0;

  PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
  PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
  PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
  PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
  IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = NULL;
  IP_ADAPTER_PREFIX *pPrefix = NULL;

  // Allocate a 15 KB buffer to start with.
  outBufLen = WORKING_BUFFER_SIZE;

  do {
    pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
    if (pAddresses == NULL) {
      printf("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
      exit(1);
    }

    dwRetVal =
        GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
      free(pAddresses);
      pAddresses = NULL;
    } else {
      break;
    }

    Iterations++;

  } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

  if (dwRetVal == NO_ERROR) {
    // If successful, output some information from the data we received
    pCurrAddresses = pAddresses;
    while (pCurrAddresses) {
      pUnicast = pCurrAddresses->FirstUnicastAddress;
      if (pUnicast != NULL) {
        for (i = 0; pUnicast != NULL; i++) {
          char buf[4096];
          DWORD bufSize = 4096;
          FATAL_FAIL_WINDOWS(WSAAddressToStringA(
              pUnicast->Address.lpSockaddr, pUnicast->Address.iSockaddrLength,
              NULL, buf, &bufSize));
          // printf("UNICAST ADDRESS: %s\n",buf);
          localIps.insert(string(buf));
          pUnicast = pUnicast->Next;
        }
      }

      pCurrAddresses = pCurrAddresses->Next;
    }
  } else {
    printf("Call to GetAdaptersAddresses failed with error: %d\n", dwRetVal);
    if (dwRetVal == ERROR_NO_DATA)
      printf("\tNo addresses were found for the requested parameters\n");
    else {
      if (FormatMessage(
              FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
              NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
              // Default language
              (LPTSTR)&lpMsgBuf, 0, NULL)) {
        printf("\tError: %s", lpMsgBuf);
        LocalFree(lpMsgBuf);
        if (pAddresses) free(pAddresses);
        exit(1);
      }
    }
  }

  if (pAddresses) {
    free(pAddresses);
  }
#else
  struct ifaddrs *myaddrs, *ifa;
  void *in_addr;
  char buf[1024];

  FATAL_FAIL(getifaddrs(&myaddrs));

  for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) continue;
    if (!(ifa->ifa_flags & IFF_UP)) continue;

    if (ifa->ifa_addr->sa_family == AF_INET6) {
      continue;
    }

    switch (ifa->ifa_addr->sa_family) {
      case AF_INET: {
        struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
        in_addr = &s4->sin_addr;
        break;
      }

      case AF_INET6: {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
        in_addr = &s6->sin6_addr;
        break;
      }

      default:
        continue;
    }

    if (!inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, sizeof(buf))) {
      printf("%s: inet_ntop failed!\n", ifa->ifa_name);
    } else {
      printf("%s: %s\n", ifa->ifa_name, buf);
    }

    string bufString(buf);
    LOG(INFO) << "Got local IP address " << bufString;
    if (bufString != string("0.0.0.0")) {
      localIps.insert(bufString);
    }
  }

  freeifaddrs(myaddrs);
#endif
  if (localIps.empty()) {
    LOG(FATAL) << "No ip addresses";
  }
  // return vector<string>(localIps.begin(), localIps.end());
  return vector<string>();
}
}  // namespace wga