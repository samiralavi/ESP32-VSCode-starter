#include "app.h"

#include "http_handlers.h"

#include "common.h"
#include "utils.h"

#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_mac.h>
#include <esp_netif_sntp.h>
#include <esp_pthread.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <logger.hpp>
#include <lwip/apps/netbiosns.h>
#include <mdns.h>
#include <nvs_flash.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_softap.h>

#include <array>
#include <chrono>
#include <string>
#include <vector>

using namespace std;

constexpr char MDNS_INSTANCE[] = "esp home web server";

constexpr char TAG[] = "app";

/* Event handler for catching WiFi events */
static void event_handler(void *arg, esp_event_base_t event_base,
                          std::int32_t event_id, void *event_data) {

  auto app = reinterpret_cast<App *>(arg);
  auto &logger = app->logger;

  static int retries;

  if (event_base == WIFI_PROV_EVENT) {
    switch (event_id) {
    case WIFI_PROV_START:
      logger.info("Provisioning started");
      break;
    case WIFI_PROV_CRED_RECV: {
      auto wifi_sta_cfg = reinterpret_cast<wifi_sta_config_t *>(event_data);
      logger.info("Received Wi-Fi credentials"
                  "\n\tSSID     : %s\n\tPassword : %s",
                  (const char *)wifi_sta_cfg->ssid,
                  (const char *)wifi_sta_cfg->password);
      break;
    }
    case WIFI_PROV_CRED_FAIL: {
      auto reason = reinterpret_cast<wifi_prov_sta_fail_reason_t *>(event_data);
      logger.error("Provisioning failed!\n\tReason : %s"
                   "\n\tPlease reset to factory and retry provisioning",
                   (*reason == WIFI_PROV_STA_AUTH_ERROR)
                       ? "Wi-Fi station authentication failed"
                       : "Wi-Fi access-point not found");
      retries++;
      if (retries >= CONFIG_WIFI_PROV_MGR_MAX_RETRY_CNT) {
        logger.info("Failed to connect with provisioned AP, reseting "
                    "provisioned credentials");
        wifi_prov_mgr_reset_sm_state_on_failure();
        retries = 0;
      }
      break;
    }
    case WIFI_PROV_CRED_SUCCESS:
      logger.info("Provisioning successful");
      retries = 0;
      break;
    case WIFI_PROV_END:
      /* De-initialize manager once provisioning is finished */
      wifi_prov_mgr_deinit();
      break;
    default:
      break;
    }
  } else if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      esp_wifi_connect();
      break;
    case WIFI_EVENT_STA_CONNECTED:
      app->send(App::Event::sta_connected);
      logger.info("STA Connected.");
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      app->send(App::Event::sta_disconnected);
      esp_wifi_connect();
      break;
    case WIFI_EVENT_AP_STACONNECTED:
      logger.info("SoftAP client disconnected.");
      break;
    case WIFI_EVENT_AP_STADISCONNECTED:
      logger.info("SoftAP client disconnected.");
      break;

    default:
      break;
    }
  } else if (event_base == IP_EVENT &&
             (event_id == IP_EVENT_STA_GOT_IP ||
              event_id == IP_EVENT_AP_STAIPASSIGNED)) {
    auto event = reinterpret_cast<ip_event_got_ip_t *>(event_data);
    logger.info("Connected with IP Address:{:d}.{:d}.{:d}.{:d}",
                IP2STR(&event->ip_info.ip));
    app->send(App::Event::ip_received);

  } else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
    switch (event_id) {
    case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
      logger.info("Secured session established!");
      break;
    case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
      logger.error("Received invalid security parameters for establishing "
                   "secure session!");
      break;
    case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
      logger.error("Received incorrect username and/or PoP for establishing "
                   "secure session!");
      break;
    default:
      break;
    }
  }
}

template <> void App::onMessage(STAConnectedEvent &event) {}
template <> void App::onMessage(STADisconnectedEvent &event) {}
template <> void App::onMessage(STAGotIPEvent &event) {}

template <> void App::onMessage(Event &event) {

  switch (event) {
  case Event::sta_connected:
    logger.info("Event::softap_started event received: %d",
                static_cast<int>(event));
    publish(STAConnectedEvent{});
    break;
  case Event::ip_received:
    logger.info("Event::ip_received event received: %d",
                static_cast<int>(event));
    publish(STAGotIPEvent{});
    break;
  case Event::sta_disconnected:
    logger.info("Event::wifi_disconnected event received %d",
                static_cast<int>(event));
    publish(STADisconnectedEvent{});
    break;
  default:
    break;
  }
}

static void initialise_mdns(void) {
  mdns_init();
  mdns_hostname_set(get_device_service_name().c_str());
  mdns_instance_name_set(MDNS_INSTANCE);

  array serviceTxtData = {mdns_txt_item_t{"board", "esp32"},
                          mdns_txt_item_t{"path", "/"}};

  ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80,
                                   serviceTxtData.data(),
                                   serviceTxtData.size()));
}

void time_sync_notification_cb(struct timeval *tv) {
  fmt::print("Notification of a time synchronization event");
}

void start_provisioning() {
  fmt::print("Starting provisioning");

  string service_name = get_device_service_name();
  wifi_prov_security_t security = WIFI_PROV_SECURITY_0;
  string service_key{CONFIG_WIFI_PASSWORD};

  ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
      security, nullptr, service_name.c_str(), service_key.c_str()));
}

void wifi_init_softap(void) {

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  string device_name = get_device_service_name();
  string password = CONFIG_WIFI_PASSWORD;

  wifi_config_t wifi_ap_config{};
  wifi_ap_config.ap.ssid_len = device_name.size();
  memcpy(wifi_ap_config.ap.ssid, device_name.c_str(), device_name.size());
  wifi_ap_config.ap.channel = 1;
  memcpy(wifi_ap_config.ap.password, password.c_str(), password.size());
  wifi_ap_config.ap.max_connection = 4;
  wifi_ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_ap_config.ap.pmf_cfg = {
      .required = false,
  };
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

  ESP_ERROR_CHECK(esp_wifi_start());

  fmt::print("wifi_init_softap finished.");
}

App::App() : logger({.tag = TAG, .level = espp::Logger::Verbosity::DEBUG}) {
  /* Configure SDK thread configuration */
  auto cfg = esp_pthread_get_default_config();
  cfg.thread_name = "app";
  cfg.pin_to_core = 1;
  cfg.stack_size = 5 * 1024;
  esp_pthread_set_cfg(&cfg);

  // TODO: Enable board configuration once LED HW is fixed.
  // board_.configure_board();
}
App &App::get_instance() {

  static auto instance = App::create();
  return *instance.get();
}

void App::onStart() {

  /* Initialize NVS partition */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    /* NVS partition was truncated
     * and needs to be erased */
    ESP_ERROR_CHECK(nvs_flash_erase());

    /* Retry nvs_flash_init */
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  ESP_ERROR_CHECK(esp_iface_mac_addr_set(
      convert_mac_address(CONFIG_MAC_ADDRESS).data(), ESP_MAC_BASE));

  /* Initialize TCP/IP */
  ESP_ERROR_CHECK(esp_netif_init());

  /* Initialize the event loop */
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  initialise_mdns();
  netbiosns_init();
  netbiosns_set_name(get_device_service_name().c_str());

  /* Register our event handler for Wi-Fi, IP and Provisioning related events */
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                             event_handler, this));
  ESP_ERROR_CHECK(esp_event_handler_register(
      PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, event_handler, this));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             event_handler, this));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                             event_handler, this));

  /* Initialize Wi-Fi including netif with default config */
  esp_netif_create_default_wifi_sta();

  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  /* Configuration for the provisioning manager */
  wifi_prov_mgr_config_t prov_config = {

      .scheme = wifi_prov_scheme_softap,

      .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,

      .app_event_handler = WIFI_PROV_EVENT_HANDLER_NONE

  };

  /* HTTP server config*/
  static httpd_handle_t _server_handle =
      nullptr; /* The handle for the HTTP server. */
  static httpd_config_t _http_server_config = HTTPD_DEFAULT_CONFIG();
  _http_server_config.uri_match_fn = httpd_uri_match_wildcard;
  _http_server_config.stack_size = 10 * 1024;
  _http_server_config.max_uri_handlers = 20;
  _http_server_config.max_open_sockets = 1;
  _http_server_config.lru_purge_enable =
      true; /*Purge "Least Recently Used" connection */
  logger.info("Starting server on port: '%d'", _http_server_config.server_port);
  ESP_ERROR_CHECK(httpd_start(&_server_handle, &_http_server_config));
  HttpHandlers::register_http_api(_server_handle);
  /* End HTTP server config */

  wifi_prov_scheme_softap_set_httpd_handle(&_server_handle);

  /* Initialize provisioning manager with the
   * configuration parameters set above */
  ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_config));

  bool provisioned = false;

  /* Let's find out if the device is provisioned */
  ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

  /* NTP Service */
  logger.info("Initializing SNTP");
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_NTP_SERVER);
  config.start = false; // start SNTP service explicitly (after connecting)
  config.server_from_dhcp = true; // accept NTP offers from DHCP server, if any
                                  // (need to enable *before* connecting)
  config.renew_servers_after_new_IP =
      true; // let esp-netif update configured SNTP server(s) after receiving
            // DHCP lease
  config.index_of_first_server =
      1; // updates from server num 1, leaving server 0 (from DHCP) intact
  // configure the event on which we renew servers

  config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;

  config.sync_cb =
      time_sync_notification_cb; // only if we need the notification function
  esp_netif_sntp_init(&config);

  logger.info("Starting SNTP");
  esp_netif_sntp_start();
  /* End NTP Service */

  /* If device is not yet provisioned start provisioning service */
  if (!provisioned) {
    start_provisioning();
  } else {
    logger.info("Already provisioned, starting Wi-Fi STA");

    /* We don't need the manager as device is already provisioned,
     * so let's release it's resources */
    wifi_prov_mgr_deinit();

    wifi_init_softap();
  }

  blufi_ = &Blufi::get_instance();

  blufi_->connect(getChannel<STAConnectedEvent>());
  blufi_->connect(getChannel<STADisconnectedEvent>());
  blufi_->connect(getChannel<STAGotIPEvent>());
}
