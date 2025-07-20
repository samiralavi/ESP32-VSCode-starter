#ifndef UTILS_H
#define UTILS_H

#include <array>
#include <cstdint>
#include <string>

std::string get_device_service_name();

std::array<uint8_t, 6> convert_mac_address(const std::string &mac_address);

uint32_t random_get();
void random_sleep_thread(uint32_t ms); /* delays for up to the given arg ms */
void read_nvs_var(const char *var, char *buf, int len, const char *def);

#endif /* UTILS_H */
