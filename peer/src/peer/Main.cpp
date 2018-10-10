#include "Headers.hpp"

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>

int main(int, char **) {
  try {
    curlpp::Cleanup myCleanup;

    // Creation of the URL option.
    curlpp::Easy myRequest;
    myRequest.setOpt(
        new curlpp::options::Url(std::string("https://example.com")));
    myRequest.setOpt(new curlpp::options::SslEngineDefault());

    std::ostringstream os;
    os << myRequest;
    cout << os.str() << endl;
  } catch (curlpp::RuntimeError &e) {
    std::cout << e.what() << std::endl;
  } catch (curlpp::LogicError &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}