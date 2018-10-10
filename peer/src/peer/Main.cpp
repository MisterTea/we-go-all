#include "BiDirectionalRpc.hpp"
#include "Headers.hpp"
#include "LogHandler.hpp"

using namespace wga;

#include <cpr/cpr.h>

int main(int argc, char** argv) {
  auto r = cpr::Get(
      cpr::Url{"https://api.github.com/repos/whoshuu/cpr/contributors"},
      cpr::Authentication{"user", "pass"},
      cpr::Parameters{{"anon", "true"}, {"key", "value"}});
  cout << r.status_code << endl;  // 200
  r.header["content-type"];       // application/json; charset=utf-8
  r.text;                         // JSON text string
}