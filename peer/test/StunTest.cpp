#include "StunClient.hpp"

#undef CHECK
#include "Catch2/single_include/catch2/catch.hpp"

namespace wga {
struct Stun {
  std::string url;
  std::string port;
};

static const vector<Stun> local_stuns({{
                                           "localhost",
                                           "3478",
                                       },
                                       {
                                           "localhost",
                                           "3480",
                                       },
                                       {
                                           "localhost",
                                           "3482",
                                       },
                                       {
                                           "localhost",
                                           "3484",
                                       },
                                       {
                                           "localhost",
                                           "3486",
                                       },
                                       {
                                           "localhost",
                                           "3488",
                                       },
                                       {
                                           "localhost",
                                           "3490",
                                       }});

static const vector<Stun> public_stuns({{
                                            "stun.l.google.com",
                                            "19302",
                                        },
                                        {
                                            "stun1.l.google.com",
                                            "19302",
                                        },
                                        {
                                            "stun2.l.google.com",
                                            "19302",
                                        },
                                        {
                                            "stun3.l.google.com",
                                            "19302",
                                        },
                                        {
                                            "stun4.l.google.com",
                                            "19302",
                                        },
                                        {
                                            "stun.ekiga.net",
                                            "3478",
                                        },
                                        {
                                            "stun.ideasip.com",
                                            "3478",
                                        },
                                        {
                                            "stun.iptel.org",
                                            "3478",
                                        },
                                        {
                                            "stun.rixtelecom.se",
                                            "3478",
                                        },
                                        {
                                            "stun.schlund.de",
                                            "3478",
                                        },
                                        {
                                            "stunserver.org",
                                            "3478",
                                        },
                                        {
                                            "stun.softjoys.com",
                                            "3478",
                                        },
                                        {
                                            "stun.voiparound.com",
                                            "3478",
                                        },
                                        {
                                            "stun.voipbuster.com",
                                            "3478",
                                        },
                                        {
                                            "stun.voipstunt.com",
                                            "3478",
                                        },
                                        {
                                            "stun.voxgratia.org",
                                            "3478",
                                        },
                                        {
                                            "stun.xten.com",
                                            "3478",
                                        },
                                        {
                                            "s1.taraba.net",
                                            "3478",
                                        },
                                        {
                                            "s2.taraba.net",
                                            "3478",
                                        },
                                        {
                                            "s1.voipstation.jp",
                                            "3478",
                                        },
                                        {
                                            "s2.voipstation.jp",
                                            "3478",
                                        }});

//------------------------------------------------------------------------------
TEST_CASE("StunTestSimple", "[StunTest]") {
  using namespace std::chrono_literals;

  using Iterator = udp::resolver::iterator;

  asio::io_service ios;
  udp::resolver resolver(ios);
  asio::steady_timer timer(ios);

  vector<Stun> stuns = {{
      "stun.l.google.com",
      "19302",
  },
      {
          "stun1.l.google.com",
          "19302",
      },
  };

  udp::socket socket_to_reflect(ios, udp::endpoint(udp::v4(), 0));
  auto stun_client =
      std::unique_ptr<StunClient>(new StunClient(socket_to_reflect));

  constexpr int N = 2;
  int wait_for = N;

  for (const auto& stun : stuns) {
    udp::resolver::query q(stun.url, stun.port);
    resolver.async_resolve(q, [&](error_code e, Iterator iter) {
      if (e || iter == Iterator()) {
        if (e != asio::error::operation_aborted) {
          cout << "Can't resolve " << stun.url << " " << e.message() << endl;
        }
        return;
      }

      if (!stun_client) return;

      stun_client->reflect(*iter,
                           [&](error_code e, udp::endpoint reflective_ep) {
                             if (e.value()) {
                              cout << "ERROR: " << stun.url << ": "
                                  << e.message() << " "
                                  << reflective_ep << endl;
                             } else {
                              cout << "FINISHED: " << stun.url << ": "
                                  << reflective_ep << endl;
                             }

                             if (!e && --wait_for == 0) {
                               timer.cancel();
                               stun_client.reset();
                               resolver.cancel();
                             }
                           });
    });
  }

  timer.expires_from_now(5s);
  timer.async_wait([&](error_code ec) {
    stun_client.reset();
    resolver.cancel();
  });

  ios.run();

  if (wait_for != 0) {
    std::cerr << "stun_client test failed: make sure at least " << N
              << " stun servers are running on the following addresses."
              << std::endl;
    for (const auto& stun : stuns) {
      std::cerr << "    " << stun.url << ":" << stun.port << std::endl;
    }
  }

  REQUIRE(wait_for == 0);
}
}  // namespace wga

