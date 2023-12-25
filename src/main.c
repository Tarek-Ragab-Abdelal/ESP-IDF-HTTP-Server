/**
 * @file main.c
 * @author Tarek Ragab (https://www.linkedin.com/in/tarek-ragab/)
 * @brief Task by Roman (Upwork Client)
 * @version 0.1
 * @date 2023-12-24
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"
#include "esp_http_server.h"
#include "cJSON.h"

#define ESP_WIFI_SSID "ESP32-Trial"
#define ESP_WIFI_PASS "12345678"
#define ESP_WIFI_CHANNEL 6
#define MAX_STA_CONN 4

static const char *TAG = "MAIN";

typedef struct
{
    char first_name[50];
    char last_name[50];
} user_data_t;

/**
 * @brief HTTP POST request handler function.
 *
 * This function handles the HTTP POST requests and extracts data from the JSON payload.
 *
 * @param req HTTP request structure.
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t post_handler(httpd_req_t *req)
{
    char buf[150];
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        ret = httpd_req_recv(req, buf, sizeof(buf));
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }

        remaining -= ret;
    }

    // Parse the received JSON data
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON data");
        return ESP_FAIL;
    }

    // Get the values of "first_name" and "last_name"
    cJSON *first_name_json = cJSON_GetObjectItem(root, "first_name");
    cJSON *last_name_json = cJSON_GetObjectItem(root, "last_name");

    if (first_name_json == NULL || last_name_json == NULL)
    {
        ESP_LOGE(TAG, "Missing JSON data fields");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Extract the strings
    const char *first_name = first_name_json->valuestring;
    const char *last_name = last_name_json->valuestring;

    // Use the extracted values as needed
    ESP_LOGI(TAG, "Received data: First Name: %s, Last Name: %s", first_name, last_name);

    // Respond with a success message
    httpd_resp_send(req, "Data received successfully", HTTPD_RESP_USE_STRLEN);

    // Clean up cJSON objects
    cJSON_Delete(root);

    return ESP_OK;
}

/**
 * @brief HTTP GET request handler function for the home page.
 *
 * This function handles the HTTP GET requests for the home page and sends an HTML form.
 *
 * @param req HTTP request structure.
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t home_handler(httpd_req_t *req)
{
    const char *html_template =
        "<html>"
        "<head>"
        "<title>ESP32 Homepage</title>"
        "<style>"
        "   body {"
        "       display: flex;"
        "       flex-direction: column;"
        "       align-items: center;"
        "       justify-content: center;"
        "       height: 100vh;"
        "       margin: 0;"
        "   }"
        "   form {"
        "       display: flex;"
        "       flex-direction: column;"
        "       align-items: center;"
        "       border: 1px solid #ccc;"
        "       padding: 20px;"
        "       border-radius: 10px;"
        "       box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);"
        "       background-color: #f9f9f9;"
        "   }"
        "   label {"
        "       margin-bottom: 10px;"
        "   }"
        "   input {"
        "       padding: 10px;"
        "       margin-bottom: 15px;"
        "       width: 200px;"
        "   }"
        "   input[type=\"submit\"] {"
        "       background-color: #4caf50;"
        "       color: white;"
        "       border: none;"
        "       padding: 10px 20px;"
        "       text-align: center;"
        "       text-decoration: none;"
        "       display: inline-block;"
        "       font-size: 16px;"
        "       cursor: pointer;"
        "       border-radius: 5px;"
        "   }"
        "</style>"
        "<script>"
        "function sendData() {"
        "  var firstName = document.getElementById('first_name').value;"
        "  var lastName = document.getElementById('last_name').value;"
        "  var data = { 'first_name': firstName, 'last_name': lastName };"
        "  fetch('/post-endpoint', {"
        "    method: 'POST',"
        "    headers: { 'Content-Type': 'application/json' },"
        "    body: JSON.stringify(data)"
        "  }).then(response => response.text())"
        "    .then(data => console.log(data))"
        "    .catch(error => console.error('Error:', error));"
        "}"
        "</script>"
        "</head>"
        "<body>"
        "<h1>ESP32 Homepage</h1>"
        "<form onsubmit='sendData(); return false;'>"
        "   <label for=\"first_name\">First Name:</label>"
        "   <input type=\"text\" id=\"first_name\" name=\"first_name\"><br>"
        "   <label for=\"last_name\">Last Name:</label>"
        "   <input type=\"text\" id=\"last_name\" name=\"last_name\"><br>"
        "   <input type=\"submit\" value=\"Save/Send\">"
        "</form>"
        "</body>"
        "</html>";

    httpd_resp_send(req, html_template, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t post_uri = {
    .uri = "/post-endpoint",
    .method = HTTP_POST,
    .handler = post_handler,
    .user_ctx = NULL};

httpd_uri_t home_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = home_handler,
    .user_ctx = NULL};

/**
 * @brief Start the HTTP server.
 *
 * This function starts the HTTP server and registers URI handlers.
 */
void start_http_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &post_uri);
        httpd_register_uri_handler(server, &home_uri);
    }
}

/**
 * @brief WiFi event handler function.
 *
 * This function handles WiFi events such as station connected and disconnected.
 *
 * @param arg User data pointer (not used).
 * @param event_base Event base.
 * @param event_id Event ID.
 * @param event_data Event data.
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        uint8_t *a = event->mac;
        ESP_LOGI("WiFi_Event", "station %02x:%02x:%02x:%02x:%02x:%02x join, AID=%d", a[0], a[1], a[2], a[3], a[4], a[5], event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        uint8_t *a = event->mac;
        ESP_LOGI("WiFi_Event", "station %02x:%02x:%02x:%02x:%02x:%02x leave, AID=%d", a[0], a[1], a[2], a[3], a[4], a[5], event->aid);
    }
}

/**
 * @brief STA event handler function
 *
 * This function handles connection and disconnection events of Station to the ESP32
 *
 * @param arg User data pointer (not used).
 * @param event_base Event base.
 * @param event_id Event ID.
 * @param event_data Event data.
 */
static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    ESP_LOGI("CONNECT", "Entered Connect Handler");
    start_http_server();
}

/**
 * @brief Initialize WiFi in soft AP mode.
 *
 * This function initializes WiFi in soft AP mode with the specified IP settings.
 */
void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_AP_STAIPASSIGNED,
                                               &connect_handler,
                                               NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));

    esp_netif_ip_info_t ip_info;

    IP4_ADDR(&ip_info.ip, 192, 168, 0, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 0, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    esp_netif_dhcps_start(ap_netif);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
}

/**
 * @brief Application entry point.
 *
 * This is the main function that initializes components and runs the main loop.
 */
void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
