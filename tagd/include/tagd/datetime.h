#pragma once

#include "tagd/codes.h"
#include <cstdint>
#include <string>

namespace tagd {

// Unix seconds and a nonnegative fraction avoid pre-epoch truncation and float rounding.
[[nodiscard]] code format_datetime(std::string&, int64_t seconds, int32_t nanoseconds);
[[nodiscard]] code current_datetime(std::string&);

}
