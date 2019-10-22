#include "Headers.hpp"

#include "LogHandler.hpp"

#include <cxxopts/include/cxxopts.hpp>

#undef CHECK
#define CATCH_CONFIG_RUNNER
#include "Catch2/single_include/catch2/catch.hpp"

using namespace wga;

int main(int argc, char** argv) {
  srand(1);

  // Setup easylogging configurations
  el::Configurations defaultConf = LogHandler::SetupLogHandler(&argc, &argv);
  defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "true");
  defaultConf.setGlobally(el::ConfigurationType::ToFile, "true");
  el::Loggers::setVerboseLevel(0);
  LOG(INFO) << argc;
  for (int a = 0; a < argc; a++) {
    LOG(INFO) << argv[a];
  }

#ifdef WIN32
  string logPath = "/tmp/wga_test_log";
#else
  string logDirectoryPattern = string("/tmp/wga_test_XXXXXXXX");
  string logDirectory = string(mkdtemp(&logDirectoryPattern[0]));
  string logPath = string(logDirectory) + "/log";
#endif
  cout << "Writing log to " << logPath << endl;
  LogHandler::SetupLogFile(&defaultConf, logPath);

  // Reconfigure default logger to apply settings above
  el::Loggers::reconfigureLogger("default", defaultConf);

  return Catch::Session().run(argc, argv);
}
