/**
 * @brief Common data types and events.
 *
 */
#ifndef COMMON_H
#define COMMON_H

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

struct STAConnectedEvent {
  std::array<std::uint8_t, 6> bssid;
  std::array<std::uint8_t, 32> ssid;
};

struct STAGotIPEvent {};
struct STADisconnectedEvent {};

#endif /* COMMON_H */
