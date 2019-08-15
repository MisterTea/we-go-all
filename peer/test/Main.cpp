#include "Headers.hpp"

#include "LogHandler.hpp"

#include <cxxopts/include/cxxopts.hpp>

#undef CHECK
#define CATCH_CONFIG_RUNNER
#include "Catch2/single_include/catch2/catch.hpp"

using namespace wga;

int main(int argc, char **argv) {
  srand(1);

  cxxopts::Options options("Peer", "Peer Program for WGA");
  options.allow_unrecognised_options();
  options.add_options()  //
      ("stress", "Enable stress tests",
       cxxopts::value<bool>()->default_value("false"))  //
      ("v,verbose", "Log verbosity",
       cxxopts::value<int>()->default_value("0"))  //
      ;
  auto params = options.parse(argc, argv);

  // Setup easylogging configurations
  el::Configurations defaultConf = LogHandler::SetupLogHandler(&argc, &argv);
  defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "false");
  defaultConf.setGlobally(el::ConfigurationType::ToFile, "true");
  el::Loggers::setVerboseLevel(params["verbose"].as<int>());

  string logDirectoryPattern = string("/tmp/wga_test_XXXXXXXX");
  string logDirectory = string(mkdtemp(&logDirectoryPattern[0]));
  string logPath = string(logDirectory) + "/log";
  cout << "Writing log to " << logPath << endl;
  LogHandler::SetupLogFile(&defaultConf, logPath);

  // Reconfigure default logger to apply settings above
  el::Loggers::reconfigureLogger("default", defaultConf);

  return Catch::Session().run(argc, argv);
}
