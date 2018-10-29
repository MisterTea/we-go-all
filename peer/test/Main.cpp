#include "Headers.hpp"

#include "LogHandler.hpp"

#include "gtest/gtest.h"

using namespace wga;

DEFINE_int32(v, 0, "verbose level");
DEFINE_bool(stress, false, "Stress test");

int main(int argc, char **argv) {
  srand(1);
  testing::InitGoogleTest(&argc, argv);

  // GFLAGS parse command line arguments
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // Setup easylogging configurations
  el::Configurations defaultConf = LogHandler::SetupLogHandler(&argc, &argv);
  defaultConf.setGlobally(el::ConfigurationType::ToFile, "false");
  el::Loggers::setVerboseLevel(FLAGS_v);

  // Reconfigure default logger to apply settings above
  el::Loggers::reconfigureLogger("default", defaultConf);

  if (FLAGS_stress) {
    for (int a = 0; a < 99; a++) {
      if (RUN_ALL_TESTS()) {
        LOG(FATAL) << "Tests failed";
      }
    }
  }

  return RUN_ALL_TESTS();
}
