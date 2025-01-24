#include <freertos/FreeRTOS.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi_comp.h"

#define TAG "[WIFI_COMP]"

#define AP_SSID         CONFIG_AP_SSID
#define AP_CHANNEL      CONFIG_AP_CHANNEL
#define AP_STA_CONN     CONFIG_AP_STA_CONN

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_event_group;

// Event handler to monitor Wi-Fi connection events
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d, reason:%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Station started");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_softap() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .password = "",
            .max_connection = AP_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s channel:%d", AP_SSID, AP_CHANNEL);

    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));

    ESP_LOGI(TAG, "Soft AP IP Address: " IPSTR, IP2STR(&ip_info.ip));
}

void wifi_init_station(const char* ssid, const char* password)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register Wi-Fi event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
            .channel = 0,
            .bssid_set = 0,
        },
    };
    
    // Copy the SSID and password from the received input
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_LOGI(TAG, "ssid: %s | pass: %s", wifi_config.sta.ssid, wifi_config.sta.password);

    ESP_ERROR_CHECK(esp_wifi_stop());

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());


    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s | Password: %s", ssid, password);
}

void url_decode(char *str) {
    char *p = str;
    char hex[3];
    while (*str) {
        if (*str == '%') {
            hex[0] = str[1];
            hex[1] = str[2];
            hex[2] = '\0';
            *p = (char)strtol(hex, NULL, 16);
            str += 3;
        } else {
            *p = *str;
            str++;
        }
        p++;
    }
    *p = '\0';  // Null terminate the string
}

void trim_trailing(char *str) {
    int len = strlen(str);
    str[len - 1] = '\0';  // Remove trailing '?' or whitespace
    len--;
    str[len - 1] = '\0';

    // while (len > 0 && (isspace((unsigned char)str[len - 1]) || str[len - 1] == '?')) {
    //     str[len - 1] = '\0';  // Remove trailing '?' or whitespace
    //     len--;
    // }
}

esp_err_t http_get_handler(httpd_req_t *req)
{
    const char* resp_str = "<html><body>"
                           "<form action='/set_credentials' method='POST'>"
                           "SSID: <input type='text' name='ssid'><br>"
                           "Password: <input type='password' name='password'><br>"
                           "<input type='submit' value='Submit'>"
                           "</form>"
                           "</body></html>";

    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

esp_err_t http_post_handler(httpd_req_t *req)
{
    char buffer[100];
    int len = httpd_req_recv(req, buffer, sizeof(buffer));
    if (len <= 0) {
        return ESP_FAIL;
    }

    url_decode(buffer);

    ESP_LOGI(TAG, "Buffer: %s", buffer);

    // Extract SSID and password from the POST data
    char ssid[32], password[64];
    if (sscanf(buffer, "ssid=%63[^&]&password=%63s", ssid, password) == 2) {
        // trim_trailing(ssid);
        trim_trailing(password);

        printf("SSID: '%s'\n", ssid);  // Notice the quotes to show leading/trailing spaces
        printf("Password: '%s'\n", password);
    } else {
        printf("Failed to parse SSID and password.\n");
    }    
    // Store credentials in non-volatile storage (NVS) or global variables
    // You can use the NVS API to store and retrieve credentials for persistence

    ESP_LOGI(TAG, "Received credentials: SSID: %s, Password: %s", ssid, password);

    // Respond to the user
    const char* resp_str = "<html><body>Credentials received. Switching to Station mode...</body></html>";
    ESP_ERROR_CHECK(httpd_resp_send(req, resp_str, strlen(resp_str)));

    // Now switch to Station mode
    wifi_init_station(ssid, password);

    return ESP_OK;
}

void start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    httpd_uri_t get_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = http_get_handler,
        .user_ctx  = NULL
    };
    
    httpd_uri_t post_uri = {
        .uri       = "/set_credentials",
        .method    = HTTP_POST,
        .handler   = http_post_handler,
        .user_ctx  = NULL
    };
    
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &post_uri));
}