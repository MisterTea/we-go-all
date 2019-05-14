#include "TimeHandler.hpp"

namespace wga {
std::chrono::time_point<std::chrono::high_resolution_clock>
    TimeHandler::initialTime = std::chrono::high_resolution_clock::now();
std::chrono::microseconds TimeHandler::timeShift = std::chrono::microseconds(0);
}  // namespace wga
