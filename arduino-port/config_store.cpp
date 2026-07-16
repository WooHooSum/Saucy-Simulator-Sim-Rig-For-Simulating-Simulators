#include "config_store.hpp"

#include <cstddef>

#include "nvs.h"
#include "nvs_flash.h"

namespace {
constexpr char kNamespace[] = "wheel";
constexpr char kKey[] = "runtime";
}

Configuration::LoadResult Configuration::load() {
  LoadResult result{wheel::safe_default_config(), false};
  esp_err_t init_error = nvs_flash_init();
  if (init_error == ESP_ERR_NVS_NO_FREE_PAGES || init_error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    if (nvs_flash_erase() != ESP_OK) return result;
    init_error = nvs_flash_init();
  }
  if (init_error != ESP_OK) return result;
  nvs_handle_t handle{};
  if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) return result;
  std::size_t size = sizeof(result.record);
  const esp_err_t error = nvs_get_blob(handle, kKey, &result.record, &size);
  nvs_close(handle);
  result.valid_from_nvs = error == ESP_OK && size == sizeof(result.record) &&
                          wheel::config_valid(result.record);
  if (!result.valid_from_nvs) result.record = wheel::safe_default_config();
  return result;
}

bool Configuration::save(const wheel::ConfigRecord& record) {
  if (!wheel::config_valid(record)) return false;
  nvs_handle_t handle{};
  if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
  const bool success = nvs_set_blob(handle, kKey, &record, sizeof(record)) == ESP_OK &&
                       nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return success;
}

