#include "Headers.hpp"

#include "LogHandler.hpp"

#include <cxxopts/include/cxxopts.hpp>
#include "gtest/gtest.h"

using namespace wga;

int main(int argc, char **argv) {
  srand(1);
  testing::InitGoogleTest(&argc, argv);

  cxxopts::Options options("Peer", "Peer Program for WGA");
  options.add_options()  //
      ("stress", "Enable stress tests",
       cxxopts::value<bool>()->default_value("false"))  //
      ("v,verbose", "Log verbosity",
       cxxopts::value<int>()->default_value("0"))  //
      ;
  auto params = options.parse(argc, argv);

  // Setup easylogging configurations
  el::Configurations defaultConf = LogHandler::SetupLogHandler(&argc, &argv);
  defaultConf.setGlobally(el::ConfigurationType::ToFile, "false");
  el::Loggers::setVerboseLevel(params["v"].as<int>());

  // Reconfigure default logger to apply settings above
  el::Loggers::reconfigureLogger("default", defaultConf);

  if (params["stress"].as<bool>()) {
    for (int a = 0; a < 99; a++) {
      if (RUN_ALL_TESTS()) {
        LOG(FATAL) << "Tests failed";
      }
    }
  }

  return RUN_ALL_TESTS();
}
