#include "LocalIpFetcher.hpp"

namespace wga {
vector<string> LocalIpFetcher::fetch(int port, bool udp) {
  set<string> localIps;
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

  if (localIps.empty()) {
    LOG(FATAL) << "No ip addresses";
  }
  freeifaddrs(myaddrs);
  return vector<string>(localIps.begin(), localIps.end());
}
}  // namespace wga