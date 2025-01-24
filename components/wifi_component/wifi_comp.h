#ifndef _WIFI_H_

#include "esp_event.h"
#include "esp_http_server.h"

void wifi_init_softap ();
void wifi_init_station(const char* ssid, const char* password);
void start_webserver(void);

esp_err_t http_get_handler(httpd_req_t *req);
esp_err_t http_post_handler(httpd_req_t *req);

#endif