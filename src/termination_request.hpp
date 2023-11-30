#pragma once
#include <stdexcept>

namespace choose {
// exit unless this is a unit test
struct termination_request : public std::exception {};
} // namespace choose
