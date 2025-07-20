#include "utils.h"

#include <format.hpp>
#include <nvs_handle.hpp>

#include <iomanip>
#include <sstream>
#include <string>

using namespace std;

constexpr char NVS_NAME_SPACE[] = "fw_storage";

template <typename T>
void read_nvs_var(const string &name, T &var, const T default_value) {

  esp_err_t err{ESP_OK};
  std::unique_ptr<nvs::NVSHandle> handle =
      nvs::open_nvs_handle(NVS_NAME_SPACE, NVS_READWRITE, &err);

  if (err != ESP_OK) {
    throw std::runtime_error(fmt::format("Exception at {} from error {}\n",
                                         __func__, esp_err_to_name(err)));
  }

  err = handle->get_item(name.c_str(), var);

  if (err == ESP_ERR_NVS_NOT_FOUND) {
    var = default_value;
    err = handle->set_item(name.c_str(), default_value);
    if (err != ESP_OK) {
      throw std::runtime_error(fmt::format("Exception at {} from error {}\n",
                                           __func__, esp_err_to_name(err)));
    }
  } else if (err != ESP_OK) {
    throw std::runtime_error(fmt::format("Exception at {} from error {}\n",
                                         __func__, esp_err_to_name(err)));
  }

  err = handle->commit();
  if (err != ESP_OK) {
    throw std::runtime_error(fmt::format("Exception at {} from error {}\n",
                                         __func__, esp_err_to_name(err)));
  }
}

template <typename T> void write_nvs_var(const string &name, const T &value) {

  esp_err_t err{ESP_OK};
  std::unique_ptr<nvs::NVSHandle> handle =
      nvs::open_nvs_handle(NVS_NAME_SPACE, NVS_READWRITE, &err);

  if (err != ESP_OK) {
    throw std::runtime_error(fmt::format("Exception at {} from error {}\n",
                                         __func__, esp_err_to_name(err)));
  }

  err = handle->set_item(name.c_str(), value);

  if (err != ESP_OK) {
    throw std::runtime_error(fmt::format("Exception at {} from error {}\n",
                                         __func__, esp_err_to_name(err)));
  }

  err = handle->commit();
  if (err != ESP_OK) {
    throw std::runtime_error(fmt::format("Exception at {} from error {}\n",
                                         __func__, esp_err_to_name(err)));
  }
}

string get_device_service_name() {
  thread_local static string device_name =
      fmt::format("{}{}", CONFIG_SSID_PREFIX, CONFIG_DEV_ID);
  return device_name;
}

std::array<uint8_t, 6> convert_mac_address(const std::string &mac_address) {
  std::array<uint8_t, 6> mac_array;
  std::stringstream mac_stream(mac_address);
  std::string token;
  int index = 5;
  while (std::getline(mac_stream, token, ':')) {
    std::stringstream converter(token);
    converter >> std::hex >> mac_array[index--];
  }
  return mac_array;
}
