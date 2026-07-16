#pragma once

#include <algorithm>
#include <cstdint>

namespace wheel {

using q15_t = std::int16_t;
constexpr std::int32_t kQ15One = 32767;

constexpr q15_t clamp_q15(std::int64_t value) {
  return static_cast<q15_t>(
      std::clamp(value, -static_cast<std::int64_t>(kQ15One),
                 static_cast<std::int64_t>(kQ15One)));
}

constexpr q15_t mul_q15(std::int32_t lhs, std::int32_t rhs) {
  return clamp_q15((static_cast<std::int64_t>(lhs) * static_cast<std::int64_t>(rhs)) /
                   kQ15One);
}

constexpr std::int32_t abs32(std::int32_t value) {
  return value < 0 ? -value : value;
}

}  // namespace wheel

