#pragma once

#include "Headers.hpp"

namespace wga {
class HttpClientMuxer {
 public:
  HttpClientMuxer(const string &serverPortPath) : sslWorks(false) {
    client.reset(new HttpClient(serverPortPath));
    secureClient.reset(new HttpsClient(serverPortPath, false));
  }

  json request(const std::string &method, const std::string &path = {"/"},
               string_view content = {},
               const SimpleWeb::CaseInsensitiveMultimap &header =
                   SimpleWeb::CaseInsensitiveMultimap()) {
    if (secureClient) {
      try {
        auto response = secureClient->request(method, path, content, header);
        FATAL_FAIL_HTTP(response);
        if (response->status_code == "308 Permanent Redirect") {
          auto newUrl = response->header.find("Location")->second;
          return request(method, newUrl, content, header);
        }
        json result = json::parse(response->content.string());
        sslWorks = true;
        return result;
      } catch (const std::exception &ex) {
        if (sslWorks) {
          LOG(FATAL) << "Http Request failed: " << ex.what();
        } else {
          // Maybe the server isn't ssl, let's check
          LOG(WARNING) << "SSL Failed: " << ex.what() << ", trying http";
          try {
            auto response2 = client->request(method, path, content, header);
            FATAL_FAIL_HTTP(response2);
            if (response2->status_code == "308 Permanent Redirect") {
              auto newUrl = response2->header.find("Location")->second;
              return request(method, newUrl, content, header);
            }
            json result = json::parse(response2->content.string());
            secureClient.reset();
            return result;
          } catch (const std::exception &ex) {
            LOGFATAL << "Http request error: " << ex.what();
          }
        }
      }
    } else {
      try {
        auto response = client->request(method, path, content, header);
        FATAL_FAIL_HTTP(response);
        if (response->status_code == "308 Permanent Redirect") {
          auto newUrl = response->header.find("Location")->second;
          return request(method, newUrl, content, header);
        }
        json result = json::parse(response->content.string());
        return result;
      } catch (const std::exception &ex) {
        LOGFATAL << "Http request error: " << ex.what();
      }
    }

    return json();
  }

 protected:
  shared_ptr<HttpClient> client;
  shared_ptr<HttpsClient> secureClient;
  bool sslWorks;
};
}  // namespace wga
