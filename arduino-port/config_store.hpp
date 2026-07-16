#pragma once

#include "src/wheel/config.hpp"

class Configuration {
 public:
  struct LoadResult {
    wheel::ConfigRecord record{};
    bool valid_from_nvs{false};
  };

  LoadResult load();
  bool save(const wheel::ConfigRecord& record);
};
