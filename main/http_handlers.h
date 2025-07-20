#ifndef HTTP_HANDLERS_H
#define HTTP_HANDLERS_H

#include <logger.hpp>

#include <esp_err.h>
#include <esp_http_server.h>

class HttpHandlers {
public:
  struct Context {
    espp::Logger logger;
  };
  static void register_http_api(httpd_handle_t server_handle);

private:
  static esp_err_t index_page_handler(httpd_req_t *req);
  static esp_err_t system_info_get_handler(httpd_req_t *req);
};
#endif /* HTTP_HANDLERS_H */
