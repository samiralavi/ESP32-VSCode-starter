#ifndef BLUFI_H
#define BLUFI_H

#include <esp_event.h>
#include <logger.hpp>
#include <samiralavi/activecpp.h>

#include <array>
#include <cstdint>
#include <string>

/**
 * @brief The Blufi class represents a Bluetooth and Wi-Fi manager.
 *
 * This class is responsible for managing Bluetooth and Wi-Fi functionality.
 * It inherits from the activecpp::ActorThread class, which allows it to run
 * as an active object.
 */
class Blufi : public samiralavi::ActorThread<Blufi> {
  friend samiralavi::ActorThread<Blufi>;

public:
  struct GetWiFiStatusEvent {
    esp_event_base_t base;
    int32_t id;
    void *data;
  };
  struct GetWiFiList {};
  struct BLEDeviceConnectedEvent {};
  struct BLEDeviceDisconnectedEvent {};
  struct BlufiReqConnectAP {};
  struct BlufiReqDisconnectAP {};
  struct BlufiSTARecBSSID {
    std::array<uint8_t, 6> bssid;
  };
  struct BlufiSTARecSSID {
    std::array<uint8_t, 32> ssid;
    int ssid_len;
  };
  struct BlufiSTARecPassword {
    std::array<uint8_t, 64> password;
    int password_len;
  };

  void onStart();

  /**
   * @brief Handles incoming messages.
   *
   * This function is called when a message is received by the Blufi object.
   * It takes any type of event as a parameter and processes it accordingly.
   *
   * @tparam Any The type of event.
   * @param event The event to be processed.
   */
  template <typename Any> void onMessage(Any &event);

  static Blufi &get_instance();
  espp::Logger logger;

private:
  /**
   * @brief Constructs a Blufi object.
   */
  Blufi();
};
#endif /* BLUFI_H */
