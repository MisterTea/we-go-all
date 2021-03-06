#ifndef __WGA_LOG_HANDLER__
#define __WGA_LOG_HANDLER__

#include "Headers.hpp"

namespace wga {
class LogHandler {
 public:
  static el::Configurations SetupLogHandler(int *argc, char ***argv);
  static void SetupLogFile(el::Configurations *defaultConf, string filename,
                           string maxlogsize = "20971520");
  static void rolloutHandler(const char *filename, std::size_t size);
};
}  // namespace wga
#endif  // __WGA_LOG_HANDLER__
