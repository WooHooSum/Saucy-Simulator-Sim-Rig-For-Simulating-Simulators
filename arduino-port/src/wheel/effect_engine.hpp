#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "fixed.hpp"

namespace wheel {

constexpr std::size_t kEffectSlotCount = 40;
constexpr std::uint16_t kInfiniteDuration = 0xffff;

enum class EffectType : std::uint8_t {
  none = 0,
  constant,
  ramp,
  square,
  sine,
  triangle,
  sawtooth_up,
  sawtooth_down,
  spring,
  damper,
  friction,
  inertia,
};

struct Envelope {
  std::uint16_t attack_level{32767};
  std::uint16_t fade_level{32767};
  std::uint16_t attack_ms{0};
  std::uint16_t fade_ms{0};
};

struct Condition {
  q15_t center{0};
  q15_t positive_coefficient{0};
  q15_t negative_coefficient{0};
  std::uint16_t positive_saturation{32767};
  std::uint16_t negative_saturation{32767};
  std::uint16_t dead_band{0};
};

struct Effect {
  EffectType type{EffectType::none};
  bool allocated{false};
  bool playing{false};
  std::uint16_t duration_ms{kInfiniteDuration};
  std::uint16_t start_delay_ms{0};
  std::uint16_t gain{32767};
  std::uint16_t sample_period_ms{1};
  std::uint16_t trigger_repeat_ms{0};
  std::uint8_t trigger_button{0xff};
  std::uint8_t loops_remaining{1};
  std::uint32_t started_at_ms{0};
  Envelope envelope{};
  Condition condition{};
  q15_t magnitude{0};
  q15_t direction{32767};
  q15_t offset{0};
  q15_t ramp_start{0};
  q15_t ramp_end{0};
  std::uint16_t phase_hundredth_deg{0};
  std::uint16_t period_ms{1000};
};

struct AxisState {
  std::int32_t position{0};
  float velocity{0.0F};
  float acceleration{0.0F};
};

using MotionState = AxisState;

class EffectEngine {
 public:
  std::optional<std::uint8_t> allocate(EffectType type);
  bool allocate_at(std::uint8_t index, EffectType type);
  bool update_definition(std::uint8_t index, const Effect& definition);
  bool free(std::uint8_t index);
  Effect* get(std::uint8_t index);
  const Effect* get(std::uint8_t index) const;

  bool start(std::uint8_t index, std::uint8_t loop_count, bool solo, std::uint32_t now_ms);
  bool stop(std::uint8_t index);
  void stop_all();
  void reset();
  void set_paused(bool paused);
  void set_actuators_enabled(bool enabled);
  void set_device_gain(std::uint16_t gain);

  std::int16_t tick(const AxisState& motion, std::uint32_t now_ms);
  std::size_t allocated_count() const;
  bool any_playing() const;
  bool paused() const { return paused_; }
  bool actuators_enabled() const { return actuators_enabled_; }

 private:
  q15_t evaluate(Effect& effect, const AxisState& motion, std::uint32_t now_ms);
  static q15_t apply_envelope(const Effect& effect, q15_t force, std::uint32_t elapsed_ms);
  static q15_t waveform(EffectType type, std::uint32_t phase);

  std::array<Effect, kEffectSlotCount> effects_{};
  std::uint16_t device_gain_{32767};
  bool paused_{false};
  bool resume_pending_{false};
  bool time_reference_valid_{false};
  bool actuators_enabled_{false};
  std::uint32_t pause_started_ms_{0};
  std::uint32_t last_tick_ms_{0};
};

}  // namespace wheel
