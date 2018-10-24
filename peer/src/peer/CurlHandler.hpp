#ifndef __CURL_HANDLER_H__
#define __CURL_HANDLER_H__

#include "Headers.hpp"

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>

namespace wga {
class CurlHandler {
 public:
  static optional<string> get(const string &url,
                              map<string, string> queryParameters) {
    try {
      curlpp::Cleanup myCleanup;

      string urlWithParams = url + "?";
      bool first = true;
      for (auto &it : queryParameters) {
        if (!first) {
          urlWithParams += "&";
        }
        first = false;
        urlWithParams += it.first + "=" + it.second;
      }

      // Creation of the URL option.
      curlpp::Easy myRequest;
      myRequest.setOpt(new curlpp::options::Url(urlWithParams));
      myRequest.setOpt(new curlpp::options::Verbose(true));
      myRequest.setOpt(new curlpp::options::HttpGet(true));

      std::ostringstream os;
      os << myRequest;
      LOG(INFO) << os.str();
      return os.str();
    } catch (curlpp::RuntimeError &e) {
      std::cout << e.what() << std::endl;
    } catch (curlpp::LogicError &e) {
      std::cout << e.what() << std::endl;
    }

    return nullopt;
  }

  static optional<string> post(const string &url, const string &payload) {
    try {
      curlpp::Cleanup myCleanup;

      // Creation of the URL option.
      curlpp::Easy myRequest;
      myRequest.setOpt(new curlpp::options::Url(url));
      myRequest.setOpt(new curlpp::options::Verbose(true));

      std::list<std::string> header;
      header.push_back("Content-Type: application/json");
      myRequest.setOpt(new curlpp::options::HttpHeader(header));

      myRequest.setOpt(new curlpp::options::PostFields(payload));
      myRequest.setOpt(new curlpp::options::PostFieldSize(payload.length()));

      std::ostringstream os;
      os << myRequest;
      LOG(INFO) << os.str();
      return os.str();
    } catch (curlpp::RuntimeError &e) {
      std::cout << e.what() << std::endl;
    } catch (curlpp::LogicError &e) {
      std::cout << e.what() << std::endl;
    }

    return nullopt;
  }
};
}  // namespace wga

#endif
