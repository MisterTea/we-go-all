#include "TimeHandler.hpp"

namespace wga {
shared_ptr<SystemClockTimeHandler> GlobalClock::timeHandler(
    new SystemClockTimeHandler());
}  // namespace wga
