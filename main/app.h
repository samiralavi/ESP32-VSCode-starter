#ifndef APP_H
#define APP_H

#include "blufi.h"
#include "board.h"
#include "common.h"

#include <esp_http_server.h>
#include <samiralavi/activecpp.h>

#include <memory>

class App : public samiralavi::ActorThread<App> {
  friend samiralavi::ActorThread<App>;

public:
  enum class Event {
    ip_received, /* IP received event */
    sta_connected,
    sta_disconnected /* WiFi disconnected event */
  };

  static App &get_instance();

  /**
   * @brief Function called when the application starts.
   */
  void onStart();

  /**
   * @brief Function called when a message is received by the App.
   * @param msg The GlobalEvent message received.
   */
  template <typename Any> void onMessage(Any &event);

  espp::Logger logger;

private:
  Blufi *blufi_{nullptr}; ///< Pointer to the Blufi object.
  Board board_;

  App();

  void register_http_api(httpd_handle_t server_handle);
};

#endif /* APP_H */
