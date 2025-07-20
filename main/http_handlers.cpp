#include "http_handlers.h"

#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <wifi_provisioning/manager.h>

#include <array>
#include <cstring>

constexpr char TAG[] = "http_handler";
using namespace std;

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

void HttpHandlers::register_http_api(httpd_handle_t server_handle) {

  static Context context{
      .logger =
          espp::Logger({.tag = TAG, .level = espp::Logger::Verbosity::DEBUG})};
  auto &logger = context.logger;

  if (server_handle == nullptr) {
    logger.info("HTTP server is not yet started!");
    return;
  }

  logger.info("Registering URI handlers");

  /* URI handler for getting default page */
  httpd_uri_t uri = {.uri = "/",
                     .method = HTTP_GET,
                     .handler = index_page_handler,
                     .user_ctx = &context};
  httpd_register_uri_handler(server_handle, &uri);

  /* URI handler for fetching system info */
  uri = {.uri = "/system/info",
         .method = HTTP_GET,
         .handler = system_info_get_handler,
         .user_ctx = &context};
  httpd_register_uri_handler(server_handle, &uri);

  /* URI handler for getting index page */
  uri = {.uri = "/index.html",
         .method = HTTP_GET,
         .handler = index_page_handler,
         .user_ctx = &context};
  httpd_register_uri_handler(server_handle, &uri);

  logger.info("Starting webserver");
}

extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");
extern const uint8_t index_html_gz_end[] asm("_binary_index_html_gz_end");

esp_err_t HttpHandlers::index_page_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_send(req, (const char *)index_html_gz_start,
                  index_html_gz_end - index_html_gz_start);

  return ESP_OK;
}

esp_err_t HttpHandlers::system_info_get_handler(httpd_req_t *req) {

  auto context = reinterpret_cast<Context *>(req->user_ctx);
  auto &logger = context->logger;

  bool provisioned = false;
  esp_err_t ret = wifi_prov_mgr_is_provisioned(&provisioned);
  if (ret != ESP_OK) {
    logger.error("wifi_prov_mgr_is_provisioned failed, ret is %s",
                 esp_err_to_name(ret));
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/text");

  httpd_resp_sendstr(req, "information");

  return ESP_OK;
}
