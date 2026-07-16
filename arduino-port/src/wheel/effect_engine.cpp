#include "effect_engine.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace wheel {

namespace {

constexpr std::array<std::int16_t, 65> kQuarterSine{
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962,
    8739, 9512, 10278, 11039, 11793, 12539, 13278, 14009, 14731, 15446,
    16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005,
    22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245,
    27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571, 30852,
    31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609,
    32678, 32728, 32757, 32767};

q15_t sine_q15(std::uint32_t phase) {
  phase &= 0xffffu;
  const std::uint32_t quadrant = phase >> 14u;
  std::uint32_t offset = phase & 0x3fffu;
  if (quadrant == 1u || quadrant == 3u) {
    offset = 0x4000u - offset;
  }
  const std::size_t index = std::min<std::size_t>((offset * 64u + 0x2000u) >> 14u, 64u);
  const std::int16_t value = kQuarterSine[index];
  return static_cast<q15_t>(quadrant >= 2u ? -value : value);
}

std::int32_t motion_to_q15(float value) {
  if (value >= static_cast<float>(kQ15One)) {
    return kQ15One;
  }
  if (value <= -static_cast<float>(kQ15One)) {
    return -kQ15One;
  }
  return static_cast<std::int32_t>(value);
}

std::uint32_t elapsed_since(std::uint32_t now, std::uint32_t then) { return now - then; }

std::int32_t clamp_condition_force(std::int32_t force, const Condition& condition) {
  const auto positive_saturation =
      std::min<std::int32_t>(condition.positive_saturation, kQ15One);
  const auto negative_saturation =
      std::min<std::int32_t>(condition.negative_saturation, kQ15One);
  return std::clamp(force, -negative_saturation, positive_saturation);
}

std::int32_t condition_displacement(std::int32_t value, const Condition& condition) {
  std::int32_t displacement = value - static_cast<std::int32_t>(condition.center);
  if (abs32(displacement) <= condition.dead_band) {
    return 0;
  }
  displacement += displacement > 0 ? -static_cast<std::int32_t>(condition.dead_band)
                                   : static_cast<std::int32_t>(condition.dead_band);
  return displacement;
}

}  // namespace

std::optional<std::uint8_t> EffectEngine::allocate(EffectType type) {
  if (type == EffectType::none) {
    return std::nullopt;
  }
  for (std::size_t i = 0; i < effects_.size(); ++i) {
    if (!effects_[i].allocated) {
      effects_[i] = Effect{};
      effects_[i].allocated = true;
      effects_[i].type = type;
      return static_cast<std::uint8_t>(i + 1u);
    }
  }
  return std::nullopt;
}

bool EffectEngine::allocate_at(std::uint8_t index, EffectType type) {
  if (index == 0 || index > effects_.size() || type == EffectType::none) {
    return false;
  }
  effects_[index - 1u] = Effect{};
  effects_[index - 1u].allocated = true;
  effects_[index - 1u].type = type;
  return true;
}

bool EffectEngine::update_definition(std::uint8_t index, const Effect& definition) {
  if (index == 0 || index > effects_.size() || !definition.allocated ||
      definition.type == EffectType::none) {
    return false;
  }
  auto& destination = effects_[index - 1u];
  if (!destination.allocated || destination.type != definition.type) {
    destination = definition;
    destination.playing = false;
    return true;
  }

  // USB owns effect parameters; the control task exclusively owns playback
  // state. Parameter downloads are allowed while an effect is running.
  const bool playing = destination.playing;
  const std::uint8_t loops = destination.loops_remaining;
  const std::uint32_t started_at = destination.started_at_ms;
  destination = definition;
  destination.playing = playing;
  destination.loops_remaining = loops;
  destination.started_at_ms = started_at;
  return true;
}

bool EffectEngine::free(std::uint8_t index) {
  if (index == 0 || index > effects_.size()) {
    return false;
  }
  effects_[index - 1u] = Effect{};
  return true;
}

Effect* EffectEngine::get(std::uint8_t index) {
  if (index == 0 || index > effects_.size() || !effects_[index - 1u].allocated) {
    return nullptr;
  }
  return &effects_[index - 1u];
}

const Effect* EffectEngine::get(std::uint8_t index) const {
  if (index == 0 || index > effects_.size() || !effects_[index - 1u].allocated) {
    return nullptr;
  }
  return &effects_[index - 1u];
}

bool EffectEngine::start(std::uint8_t index, std::uint8_t loop_count, bool solo,
                         std::uint32_t now_ms) {
  auto* effect = get(index);
  if (effect == nullptr) {
    return false;
  }
  if (solo) {
    stop_all();
  }
  effect->playing = true;
  effect->loops_remaining = loop_count == 0 ? 1 : loop_count;
  if (!time_reference_valid_) {
    last_tick_ms_ = now_ms;
    if (paused_) {
      pause_started_ms_ = now_ms;
    }
    time_reference_valid_ = true;
  }
  effect->started_at_ms = paused_ || resume_pending_ ? pause_started_ms_ : now_ms;
  return true;
}

bool EffectEngine::stop(std::uint8_t index) {
  auto* effect = get(index);
  if (effect == nullptr) {
    return false;
  }
  effect->playing = false;
  return true;
}

void EffectEngine::stop_all() {
  for (auto& effect : effects_) {
    effect.playing = false;
  }
}

void EffectEngine::reset() {
  effects_.fill(Effect{});
  device_gain_ = 32767;
  paused_ = false;
  resume_pending_ = false;
  time_reference_valid_ = false;
  actuators_enabled_ = false;
  pause_started_ms_ = 0;
  last_tick_ms_ = 0;
}

void EffectEngine::set_paused(bool paused) {
  if (paused == paused_) {
    return;
  }
  if (paused) {
    paused_ = true;
    if (!resume_pending_ && time_reference_valid_) {
      pause_started_ms_ = last_tick_ms_;
    }
    resume_pending_ = false;
    return;
  }
  paused_ = false;
  resume_pending_ = time_reference_valid_;
}

void EffectEngine::set_actuators_enabled(bool enabled) {
  actuators_enabled_ = enabled;
  if (!enabled) {
    stop_all();
  }
}

void EffectEngine::set_device_gain(std::uint16_t gain) {
  device_gain_ = std::min<std::uint16_t>(gain, 32767);
}

std::int16_t EffectEngine::tick(const AxisState& motion, std::uint32_t now_ms) {
  if (!time_reference_valid_) {
    last_tick_ms_ = now_ms;
    if (paused_) {
      pause_started_ms_ = now_ms;
    }
    time_reference_valid_ = true;
  }
  if (paused_) {
    return 0;
  }
  if (resume_pending_) {
    const std::uint32_t paused_duration = elapsed_since(now_ms, pause_started_ms_);
    for (auto& effect : effects_) {
      if (effect.allocated && effect.playing) {
        effect.started_at_ms += paused_duration;
      }
    }
    resume_pending_ = false;
  }
  last_tick_ms_ = now_ms;
  if (!actuators_enabled_) {
    return 0;
  }
  std::int64_t mixed = 0;
  for (auto& effect : effects_) {
    if (effect.allocated && effect.playing) {
      mixed += evaluate(effect, motion, now_ms);
    }
  }
  const std::int64_t gained =
      (mixed * static_cast<std::int64_t>(device_gain_)) / kQ15One;
  return clamp_q15(static_cast<std::int32_t>(
      std::clamp<std::int64_t>(gained, -kQ15One, kQ15One)));
}

std::size_t EffectEngine::allocated_count() const {
  return static_cast<std::size_t>(std::count_if(effects_.begin(), effects_.end(),
                                                [](const Effect& effect) { return effect.allocated; }));
}

bool EffectEngine::any_playing() const {
  return std::any_of(effects_.begin(), effects_.end(),
                     [](const Effect& effect) { return effect.playing; });
}

q15_t EffectEngine::evaluate(Effect& effect, const AxisState& motion, std::uint32_t now_ms) {
  std::uint32_t elapsed = elapsed_since(now_ms, effect.started_at_ms);
  if (elapsed < effect.start_delay_ms) {
    return 0;
  }
  elapsed -= effect.start_delay_ms;

  if (effect.duration_ms != kInfiniteDuration && effect.duration_ms > 0 &&
      elapsed >= effect.duration_ms) {
    const std::uint32_t completed = elapsed / effect.duration_ms;
    if (completed >= effect.loops_remaining) {
      effect.playing = false;
      return 0;
    }
    elapsed %= effect.duration_ms;
  }

  std::int32_t force = 0;
  switch (effect.type) {
    case EffectType::constant:
      force = effect.magnitude;
      break;
    case EffectType::ramp: {
      const auto duration = effect.duration_ms == 0 || effect.duration_ms == kInfiniteDuration
                                ? 1u
                                : effect.duration_ms;
      const auto ramp_elapsed =
          static_cast<std::int64_t>(std::min<std::uint32_t>(elapsed, duration));
      const auto delta = static_cast<std::int64_t>(effect.ramp_end) - effect.ramp_start;
      force = static_cast<std::int32_t>(static_cast<std::int64_t>(effect.ramp_start) +
                                        (delta * ramp_elapsed) / duration);
      break;
    }
    case EffectType::sine:
    case EffectType::square:
    case EffectType::triangle:
    case EffectType::sawtooth_up:
    case EffectType::sawtooth_down: {
      const std::uint32_t period = std::max<std::uint16_t>(effect.period_ms, 1);
      const std::uint32_t phase_offset =
          (static_cast<std::uint32_t>(effect.phase_hundredth_deg) * 65536u) / 36000u;
      const std::uint32_t phase = ((elapsed % period) * 65536u) / period + phase_offset;
      force = effect.offset + mul_q15(waveform(effect.type, phase), effect.magnitude);
      break;
    }
    case EffectType::spring: {
      const std::int32_t displacement = condition_displacement(motion.position, effect.condition);
      const std::int32_t coefficient = displacement > 0 ? effect.condition.positive_coefficient
                                                        : effect.condition.negative_coefficient;
      force = displacement == 0 ? 0 : mul_q15(displacement, coefficient);
      force = clamp_condition_force(force, effect.condition);
      break;
    }
    case EffectType::damper: {
      const std::int32_t displacement =
          condition_displacement(motion_to_q15(motion.velocity), effect.condition);
      const std::int32_t coefficient = displacement > 0 ? effect.condition.positive_coefficient
                                                        : effect.condition.negative_coefficient;
      force = displacement == 0 ? 0 : mul_q15(displacement, coefficient);
      force = clamp_condition_force(force, effect.condition);
      break;
    }
    case EffectType::friction: {
      const std::int32_t displacement =
          condition_displacement(motion_to_q15(motion.velocity), effect.condition);
      if (displacement == 0) {
        force = 0;
        break;
      }
      const std::int32_t coefficient = displacement > 0 ? effect.condition.positive_coefficient
                                                        : effect.condition.negative_coefficient;
      force = displacement > 0 ? coefficient : -coefficient;
      force = clamp_condition_force(force, effect.condition);
      break;
    }
    case EffectType::inertia: {
      const std::int32_t displacement =
          condition_displacement(motion_to_q15(motion.acceleration), effect.condition);
      const std::int32_t coefficient = displacement > 0 ? effect.condition.positive_coefficient
                                                        : effect.condition.negative_coefficient;
      force = displacement == 0 ? 0 : mul_q15(displacement, coefficient);
      force = clamp_condition_force(force, effect.condition);
      break;
    }
    case EffectType::none:
      return 0;
  }

  force = mul_q15(force, effect.direction);
  force = apply_envelope(effect, clamp_q15(force), elapsed);
  return mul_q15(force, effect.gain);
}

q15_t EffectEngine::apply_envelope(const Effect& effect, q15_t force, std::uint32_t elapsed_ms) {
  std::int64_t scale = 32767;
  if (effect.envelope.attack_ms > 0 && elapsed_ms < effect.envelope.attack_ms) {
    scale = static_cast<std::int64_t>(effect.envelope.attack_level) +
            ((32767 - static_cast<std::int64_t>(effect.envelope.attack_level)) * elapsed_ms) /
                effect.envelope.attack_ms;
  }

  if (effect.duration_ms != kInfiniteDuration && effect.envelope.fade_ms > 0 &&
      elapsed_ms < effect.duration_ms) {
    const std::uint32_t fade_start =
        effect.duration_ms > effect.envelope.fade_ms ? effect.duration_ms - effect.envelope.fade_ms : 0;
    if (elapsed_ms >= fade_start) {
      const std::uint32_t fade_elapsed = elapsed_ms - fade_start;
      const std::int64_t fade_scale =
          32767 - ((32767 - static_cast<std::int64_t>(effect.envelope.fade_level)) * fade_elapsed) /
                      effect.envelope.fade_ms;
      scale = std::min(scale, fade_scale);
    }
  }
  return mul_q15(force, static_cast<std::int32_t>(
                            std::clamp<std::int64_t>(scale, -kQ15One, kQ15One)));
}

q15_t EffectEngine::waveform(EffectType type, std::uint32_t phase) {
  const std::uint32_t p = phase & 0xffffu;
  switch (type) {
    case EffectType::sine:
      return sine_q15(p);
    case EffectType::square:
      return p < 0x8000u ? 32767 : -32767;
    case EffectType::triangle: {
      const std::int32_t value = p < 0x8000u ? static_cast<std::int32_t>(p) * 2 - 32767
                                             : 98303 - static_cast<std::int32_t>(p) * 2;
      return clamp_q15(value);
    }
    case EffectType::sawtooth_up:
      return clamp_q15(static_cast<std::int32_t>(p) - 32767);
    case EffectType::sawtooth_down:
      return clamp_q15(32767 - static_cast<std::int32_t>(p));
    default:
      return 0;
  }
}

}  // namespace wheel
