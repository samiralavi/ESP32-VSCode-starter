#include "blufi.h"

#include "common.h"
#include "utils.h"

#include <esp_blufi.h>
#include <esp_blufi_api.h>
#include <esp_bt.h>
#include <esp_crc.h>
#include <esp_err.h>
#include <esp_pthread.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
#include <mbedtls/aes.h>
#include <mbedtls/dhm.h>
#include <mbedtls/md5.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include <array>
#include <memory.h>
#include <stdio.h>
#include <vector>

constexpr char TAG[] = "blufi";

using namespace std;

static wifi_config_t sta_config;

/* store the station info for send back to phone */
static bool gl_sta_connected = false;
static bool gl_sta_got_ip = false;
static bool ble_is_connected = false;
static array<uint8_t, 6> gl_sta_bssid;
static array<uint8_t, 32> gl_sta_ssid;
static wifi_sta_list_t gl_sta_list;
static bool gl_sta_is_connecting = false;
static esp_blufi_extra_info_t gl_sta_conn_info;

// security
/*
   The SEC_TYPE_xxx is for self-defined packet data type in the procedure of
   "BLUFI negotiate key" If user use other negotiation procedure to exchange(or
   generate) key, should redefine the type by yourself.
 */
#define SEC_TYPE_DH_PARAM_LEN 0x00
#define SEC_TYPE_DH_PARAM_DATA 0x01
#define SEC_TYPE_DH_P 0x02
#define SEC_TYPE_DH_G 0x03
#define SEC_TYPE_DH_PUBLIC 0x04

struct blufi_security {
#define DH_SELF_PUB_KEY_LEN 128
#define DH_SELF_PUB_KEY_BIT_LEN (DH_SELF_PUB_KEY_LEN * 8)
  uint8_t self_public_key[DH_SELF_PUB_KEY_LEN];
#define SHARE_KEY_LEN 128
#define SHARE_KEY_BIT_LEN (SHARE_KEY_LEN * 8)
  uint8_t share_key[SHARE_KEY_LEN];
  size_t share_len;
#define PSK_LEN 16
  uint8_t psk[PSK_LEN];
  uint8_t *dh_param;
  int dh_param_len;
  uint8_t iv[16];
  mbedtls_dhm_context dhm;
  mbedtls_aes_context aes;
};

static std::unique_ptr<blufi_security> blufi_sec{};

static int myrand(void *rng_state, unsigned char *output, size_t len) {
  esp_fill_random(output, len);
  return (0);
}

extern "C" void btc_blufi_report_error(esp_blufi_error_state_t state);
extern "C" void ble_store_config_init(void);

static void blufi_dh_negotiate_data_handler(uint8_t *data, int len,
                                            uint8_t **output_data,
                                            int *output_len, bool *need_free) {
  int ret;
  uint8_t type = data[0];

  auto &blufi_actor = Blufi::get_instance();
  auto &logger = blufi_actor.logger;

  if (blufi_sec == nullptr) {
    logger.error("BLUFI Security is not initialized");
    btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
    return;
  }

  switch (type) {
  case SEC_TYPE_DH_PARAM_LEN:
    blufi_sec->dh_param_len = ((data[1] << 8) | data[2]);
    if (blufi_sec->dh_param) {
      free(blufi_sec->dh_param);
      blufi_sec->dh_param = NULL;
    }
    blufi_sec->dh_param = (uint8_t *)malloc(blufi_sec->dh_param_len);
    if (blufi_sec->dh_param == NULL) {
      btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
      logger.error("{}, malloc failed\n", __func__);
      return;
    }
    break;
  case SEC_TYPE_DH_PARAM_DATA: {
    if (blufi_sec->dh_param == NULL) {
      logger.error("{}, blufi_sec->dh_param == NULL\n", __func__);
      btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
      return;
    }
    uint8_t *param = blufi_sec->dh_param;
    memcpy(blufi_sec->dh_param, &data[1], blufi_sec->dh_param_len);
    ret = mbedtls_dhm_read_params(&blufi_sec->dhm, &param,
                                  &param[blufi_sec->dh_param_len]);
    if (ret) {
      logger.error("{} read param failed %d\n", __func__, ret);
      btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
      return;
    }
    free(blufi_sec->dh_param);
    blufi_sec->dh_param = NULL;

    const int dhm_len = mbedtls_dhm_get_len(&blufi_sec->dhm);
    ret = mbedtls_dhm_make_public(&blufi_sec->dhm, dhm_len,
                                  blufi_sec->self_public_key, dhm_len, myrand,
                                  NULL);
    if (ret) {
      logger.error("{} make public failed %d\n", __func__, ret);
      btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
      return;
    }

    ret = mbedtls_dhm_calc_secret(&blufi_sec->dhm, blufi_sec->share_key,
                                  SHARE_KEY_BIT_LEN, &blufi_sec->share_len,
                                  myrand, NULL);
    if (ret) {
      logger.error("{} mbedtls_dhm_calc_secret failed %d\n", __func__, ret);
      btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
      return;
    }

    ret =
        mbedtls_md5(blufi_sec->share_key, blufi_sec->share_len, blufi_sec->psk);

    if (ret) {
      logger.error("{} mbedtls_md5 failed %d\n", __func__, ret);
      btc_blufi_report_error(ESP_BLUFI_CALC_MD5_ERROR);
      return;
    }

    mbedtls_aes_setkey_enc(&blufi_sec->aes, blufi_sec->psk, 128);

    /* alloc output data */
    *output_data = &blufi_sec->self_public_key[0];
    *output_len = dhm_len;
    *need_free = false;

  } break;
  case SEC_TYPE_DH_P:
    break;
  case SEC_TYPE_DH_G:
    break;
  case SEC_TYPE_DH_PUBLIC:
    break;
  }
}

static int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
  int ret;
  size_t iv_offset = 0;
  uint8_t iv0[16];

  memcpy(iv0, blufi_sec->iv, sizeof(blufi_sec->iv));
  iv0[0] = iv8; /* set iv8 as the iv0[0] */

  ret =
      mbedtls_aes_crypt_cfb128(&blufi_sec->aes, MBEDTLS_AES_ENCRYPT, crypt_len,
                               &iv_offset, iv0, crypt_data, crypt_data);
  if (ret) {
    return -1;
  }

  return crypt_len;
}

static int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
  int ret;
  size_t iv_offset = 0;
  uint8_t iv0[16];

  memcpy(iv0, blufi_sec->iv, sizeof(blufi_sec->iv));
  iv0[0] = iv8; /* set iv8 as the iv0[0] */

  ret =
      mbedtls_aes_crypt_cfb128(&blufi_sec->aes, MBEDTLS_AES_DECRYPT, crypt_len,
                               &iv_offset, iv0, crypt_data, crypt_data);
  if (ret) {
    return -1;
  }

  return crypt_len;
}

static uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len) {
  /* This iv8 ignore, not used */
  return esp_crc16_be(0, data, len);
}

static esp_err_t blufi_security_init() {
  auto &blufi_actor = Blufi::get_instance();
  auto &logger = blufi_actor.logger;

  try {
    blufi_sec = std::make_unique<blufi_security>();
  } catch (const std::bad_alloc &e) {
    logger.error("Allocation failed: {} ", e.what());
    return ESP_FAIL;
  }
  mbedtls_dhm_init(&blufi_sec->dhm);
  mbedtls_aes_init(&blufi_sec->aes);

  memset(blufi_sec->iv, 0x0, 16);
  return 0;
}

static void blufi_security_deinit() {
  if (blufi_sec == NULL) {
    return;
  }
  if (blufi_sec->dh_param) {
    free(blufi_sec->dh_param);
    blufi_sec->dh_param = NULL;
  }
  mbedtls_dhm_free(&blufi_sec->dhm);
  mbedtls_aes_free(&blufi_sec->aes);

  blufi_sec.reset();
}

static void bleprph_host_task(void *param) {
  auto &blufi_actor = Blufi::get_instance();
  auto &logger = blufi_actor.logger;

  logger.info("BLE Host Task Started");
  /* This function will return only when nimble_port_stop() is executed */
  nimble_port_run();

  nimble_port_freertos_deinit();
}

static void blufi_on_reset(int reason) {
  MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void blufi_on_sync() { esp_blufi_profile_init(); }

esp_err_t esp_blufi_gap_register_callback(void) { return ESP_OK; }

static int softap_get_current_connection_number(void) {
  esp_err_t ret;
  ret = esp_wifi_ap_get_sta_list(&gl_sta_list);
  if (ret == ESP_OK) {
    return gl_sta_list.num;
  }

  return 0;
}

static void blufi_event_handler(esp_blufi_cb_event_t event,
                                esp_blufi_cb_param_t *param) {
  /* This handler runs in the blufi thread, avoid lengthy blocks. */

  auto &blufi_actor = Blufi::get_instance();
  auto &logger = blufi_actor.logger;

  switch (event) {
  case ESP_BLUFI_EVENT_INIT_FINISH:
    if (param->init_finish.state != ESP_BLUFI_INIT_OK) {
      logger.error("BLUFI stack init failed.");
    } else {
      logger.info("BLUFI stack initialization finished successfully.");
    }
    esp_blufi_adv_start();
    break;
  case ESP_BLUFI_EVENT_DEINIT_FINISH:
    if (param->init_finish.state != ESP_BLUFI_INIT_OK) {
      logger.error("BLUFI stack de-init failed.");
    } else {
      logger.info("BLUFI stack de-initialization finished successfully.");
    }
    break;
  case ESP_BLUFI_EVENT_BLE_CONNECT:
    blufi_actor.send(Blufi::BLEDeviceConnectedEvent{});
    break;
  case ESP_BLUFI_EVENT_BLE_DISCONNECT:
    blufi_actor.send(Blufi::BLEDeviceDisconnectedEvent{});
    break;
  case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
    blufi_actor.send(Blufi::BlufiReqConnectAP{});
    break;
  case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
    blufi_actor.send(Blufi::BlufiReqDisconnectAP{});
    break;
  case ESP_BLUFI_EVENT_REPORT_ERROR:
    logger.error("Report error, error code {}\n",
                 static_cast<int>(param->report_error.state));
    esp_blufi_send_error_info(param->report_error.state);
    break;
  case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
    blufi_actor.send(Blufi::GetWiFiStatusEvent{});
    break;
  }
  case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
    logger.info("BLE device requested disconnection.");
    esp_blufi_disconnect();
    break;
  case ESP_BLUFI_EVENT_RECV_STA_BSSID: {
    auto actor_event = Blufi::BlufiSTARecBSSID{};
    std::copy_n(param->sta_bssid.bssid, actor_event.bssid.size(),
                actor_event.bssid.begin());
    blufi_actor.send(actor_event);
  } break;
  case ESP_BLUFI_EVENT_RECV_STA_SSID: {
    auto actor_event =
        Blufi::BlufiSTARecSSID{.ssid_len = param->sta_ssid.ssid_len};
    std::copy_n(param->sta_ssid.ssid, param->sta_ssid.ssid_len,
                actor_event.ssid.begin());
    blufi_actor.send(actor_event);
  } break;
  case ESP_BLUFI_EVENT_RECV_STA_PASSWD: {
    auto actor_event = Blufi::BlufiSTARecPassword{
        .password_len = param->sta_passwd.passwd_len};
    std::copy_n(param->sta_passwd.passwd, param->sta_passwd.passwd_len,
                actor_event.password.begin());
    blufi_actor.send(actor_event);
  } break;
  case ESP_BLUFI_EVENT_GET_WIFI_LIST:
    blufi_actor.send(Blufi::GetWiFiList{});
    break;
  default:
    break;
  }
}

static esp_err_t esp_blufi_host_init() {

  auto &blufi_actor = Blufi::get_instance();
  auto &logger = blufi_actor.logger;

  esp_err_t err;
  err = esp_nimble_init();
  if (err) {
    logger.error("{} failed: {}\n", __func__, esp_err_to_name(err));
    return ESP_FAIL;
  }

  /* Initialize the NimBLE host configuration. */
  ble_hs_cfg.reset_cb = blufi_on_reset;
  ble_hs_cfg.sync_cb = blufi_on_sync;
  ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  ble_hs_cfg.sm_io_cap = 4;
#ifdef CONFIG_EXAMPLE_BONDING
  ble_hs_cfg.sm_bonding = 1;
#endif
#ifdef CONFIG_EXAMPLE_MITM
  ble_hs_cfg.sm_mitm = 1;
#endif
#ifdef CONFIG_EXAMPLE_USE_SC
  ble_hs_cfg.sm_sc = 1;
#else
  ble_hs_cfg.sm_sc = 0;
#ifdef CONFIG_EXAMPLE_BONDING
  ble_hs_cfg.sm_our_key_dist = 1;
  ble_hs_cfg.sm_their_key_dist = 1;
#endif
#endif

  int rc;
  rc = esp_blufi_gatt_svr_init();
  assert(rc == 0);

  /* Set the default device name. */
  rc = ble_svc_gap_device_name_set(get_device_service_name().c_str());
  assert(rc == 0);

  /* XXX Need to have template for store */
  ble_store_config_init();

  esp_blufi_btc_init();

  nimble_port_freertos_init(bleprph_host_task);

  return ESP_OK;
}

static esp_err_t ble_controller_init() {

  auto &blufi_actor = Blufi::get_instance();
  auto &logger = blufi_actor.logger;

  esp_err_t ret = ESP_OK;

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret) {
    logger.error("{} initialize bt controller failed: {}\n", __func__,
                 esp_err_to_name(ret));
    return ret;
  }

  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret) {
    logger.error("{} enable bt controller failed: {}\n", __func__,
                 esp_err_to_name(ret));
    return ret;
  }
  return ret;
}

static esp_err_t esp_blufi_host_and_cb_init() {

  auto &blufi_actor = Blufi::get_instance();
  auto &logger = blufi_actor.logger;

  esp_err_t ret = ESP_OK;

  static esp_blufi_callbacks_t blufi_callbacks = {
      .event_cb = blufi_event_handler,
      .negotiate_data_handler = blufi_dh_negotiate_data_handler,
      .encrypt_func = blufi_aes_encrypt,
      .decrypt_func = blufi_aes_decrypt,
      .checksum_func = blufi_crc_checksum,
  };

  ret = esp_blufi_register_callbacks(&blufi_callbacks);
  if (ret) {
    logger.error("{:x} blufi register failed, error code = {:x}\n", __func__,
                 ret);
    return ret;
  }

  ret = esp_blufi_gap_register_callback();
  if (ret) {
    logger.error("{:x} gap register failed, error code = {:x}\n", __func__,
                 ret);
    return ret;
  }

  ret = esp_blufi_host_init();
  if (ret) {
    logger.error("{} initialise host failed: {}\n", __func__,
                 esp_err_to_name(ret));
    return ret;
  }

  return ret;
}

Blufi::Blufi() : logger({.tag = TAG, .level = espp::Logger::Verbosity::DEBUG}) {
  /* Configure SDK thread configuration */
  auto blui_thread_cfg = esp_pthread_get_default_config();
  blui_thread_cfg.stack_size = 3072 * 10;
  blui_thread_cfg.pin_to_core = 1;
  esp_pthread_set_cfg(&blui_thread_cfg);
}

void Blufi::onStart() {
  int ret = ble_controller_init();
  if (ret) {
    logger.error("{} BLE controller init failed: {}\n", __func__,
                 esp_err_to_name(ret));
    return;
  }

  ret = esp_blufi_host_and_cb_init();
  if (ret) {
    logger.error("{} initialise failed: {}\n", __func__, esp_err_to_name(ret));
    return;
  }

  logger.info("BLUFI VERSION {:x}\n", esp_blufi_get_version());
}

Blufi &Blufi::get_instance() {

  static auto instance = Blufi::create();
  return *instance.get();
}

template <> void Blufi::onMessage(STAConnectedEvent &event) {
  gl_sta_connected = true;
  gl_sta_is_connecting = false;
  gl_sta_bssid = event.bssid;
  gl_sta_ssid = event.ssid;
}

template <> void Blufi::onMessage(STADisconnectedEvent &event) {
  gl_sta_connected = false;
  gl_sta_is_connecting = false;
}

template <> void Blufi::onMessage(GetWiFiStatusEvent &event) {
  logger.info("BLUFI get wifi status from AP.");

  wifi_mode_t mode;
  esp_blufi_extra_info_t info;

  esp_wifi_get_mode(&mode);

  if (gl_sta_connected) {
    memset(&info, 0, sizeof(esp_blufi_extra_info_t));
    memcpy(info.sta_bssid, gl_sta_bssid.data(), gl_sta_bssid.size());
    info.sta_bssid_set = true;
    info.sta_ssid = gl_sta_ssid.data();
    info.sta_ssid_len = gl_sta_ssid.size();
    esp_blufi_send_wifi_conn_report(
        mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP,
        softap_get_current_connection_number(), &info);
  } else if (gl_sta_is_connecting) {
    esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING,
                                    softap_get_current_connection_number(),
                                    &gl_sta_conn_info);
  } else {
    esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL,
                                    softap_get_current_connection_number(),
                                    &gl_sta_conn_info);
  }
}
template <> void Blufi::onMessage(BLEDeviceConnectedEvent &event) {
  logger.info("BLE device connected.");
  ble_is_connected = true;
  esp_blufi_adv_stop();
  blufi_security_init();
}

template <> void Blufi::onMessage(BLEDeviceDisconnectedEvent &event) {
  logger.info("BLE device disconnected.");
  ble_is_connected = false;
  blufi_security_deinit();
  esp_blufi_adv_start();
}

template <> void Blufi::onMessage(BlufiReqConnectAP &event) {
  logger.info("BLE device request WiFi connect to AP.");
  /* there is no wifi callback when the device has already connected to this
  wifi so disconnect wifi before connection.
  */
  // wifi_config_t new_config;
  // esp_wifi_get_config(WIFI_IF_STA, &new_config);
  // memcpy(new_config.sta.ssid, sta_config.sta.ssid,
  // sizeof(sta_config.sta.ssid)); new_config.sta. = sta_config.sta.ssid;
  esp_wifi_set_config(WIFI_IF_STA, &sta_config);
  esp_restart();
}

template <> void Blufi::onMessage(BlufiReqDisconnectAP &event) {
  logger.info("BLE device request disconnect from AP.");
  esp_wifi_disconnect();
}

template <> void Blufi::onMessage(BlufiSTARecBSSID &event) {
  std::copy_n(event.bssid.begin(), event.bssid.size(), sta_config.sta.bssid);
  sta_config.sta.bssid_set = 1;
  logger.info("Recv STA BSSID {}\n", sta_config.sta.bssid);
}

template <> void Blufi::onMessage(BlufiSTARecSSID &event) {
  strncpy((char *)sta_config.sta.ssid, (char *)event.ssid.data(),
          event.ssid_len);
  sta_config.sta.ssid[event.ssid_len] = '\0';
  logger.info("Recv STA SSID {}\n", sta_config.sta.ssid);
}

template <> void Blufi::onMessage(BlufiSTARecPassword &event) {
  strncpy((char *)sta_config.sta.password, (char *)event.password.data(),
          event.password_len);
  sta_config.sta.password[event.password_len] = '\0';
  logger.info("Recv STA PASSWORD {}\n", sta_config.sta.password);
}

template <> void Blufi::onMessage(GetWiFiList &event) {
  wifi_scan_config_t scanConf = {
      .ssid = nullptr, .bssid = nullptr, .channel = 0, .show_hidden = false};
  esp_err_t ret = esp_wifi_scan_start(&scanConf, true);
  if (ret != ESP_OK) {
    esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
  }
  logger.info("WiFi scan is finished.\n");
  uint16_t ap_count = 0;
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

  vector<wifi_ap_record_t> ap_list(ap_count);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list.data()));

  vector<esp_blufi_ap_record_t> blufi_ap_list;
  for (auto &item : ap_list) {
    esp_blufi_ap_record_t record;
    record.rssi = item.rssi;
    memcpy(record.ssid, item.ssid, sizeof(record.ssid));
    blufi_ap_list.push_back(record);
  }
  ESP_ERROR_CHECK(esp_blufi_send_wifi_list(ap_count, blufi_ap_list.data()));
}
