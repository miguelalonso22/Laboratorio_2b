#include <stdio.h>
#include "esp_netif.h"

#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static int s_retry_num = 0;
static const int EXAMPLE_ESP_MAXIMUM_RETRY = 5;

static void sta_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            // Removido ESP_LOGI, manejar el estado en tu propia lógica de aplicación.
        } else {
            // Manejo de la conexión fallida sin FreeRTOS.
            // Aquí podrías, por ejemplo, cambiar el estado de una variable o llamar a una función propia de manejo de error.
            s_retry_num = 0; // Resetea el contador de reintentos
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        // Removido ESP_LOGI, considera almacenar la IP obtenida en una estructura o variable.
        s_retry_num = 0; // Resetea el contador de reintentos al obtener una IP
        // Notificar exitosamente la conexión sin usar FreeRTOS
    }
}



static void ap_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
    }
}


void sta_init(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

// Inicializar la interfaz de red
    esp_netif_init();
// Crear el loop de eventos por defecto
    esp_event_loop_create_default();
// Crear WiFi AP por defecto
    esp_netif_create_default_wifi_sta();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &sta_event_handler,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &sta_event_handler,
                                        NULL,
                                        &instance_got_ip);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t sta_config = {
        .sta = {
            .ssid = "Miguel",
            .password = "32554803",
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
	     // .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    esp_wifi_start();
    esp_wifi_connect();
    printf("WiFi started\n");
}



void ap_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
// Inicializar la interfaz de red
    esp_netif_init();
// Crear el loop de eventos por defecto
    esp_event_loop_create_default();
// Crear WiFi AP por defecto
    esp_netif_create_default_wifi_ap();
// Registrar el manejador de eventos de WiFi 
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &ap_event_handler,
                                        NULL,
                                        NULL);
// Inicializar el stack de WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
// Configurar las opciones del AP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "MiAP",
            .ssid_len = strlen("MiAP"),
            .channel = 1,
            .password = "password123",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };


    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    printf("WiFi started\n");

}


// Manejador de solicitudes HTTP GET
// esp_err_t hello_get_handler(httpd_req_t *req) {
//     const char* resp_str = "Hola mundo";
//     httpd_resp_send(req, resp_str, strlen(resp_str));
//     return ESP_OK;
// }

// Definición de la macro MIN
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif


esp_err_t hello_get_handler(httpd_req_t *req)
{
    const char resp[] = "Hola mundo";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t hello = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = NULL
};

// Manejador de solicitudes HTTP POST
esp_err_t hello_post_handler(httpd_req_t *req) {
    char buf[100];
    int ret, remaining = req->content_len;

    // Leer el contenido del cuerpo de la solicitud
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;  // Intentar recibir de nuevo
            }
            return ESP_FAIL;
        }
        remaining -= ret;
        
        // Procesar los datos recibidos
        // Por ejemplo, simplemente imprimimos los datos recibidos (en un entorno real, procesar adecuadamente)
        printf("Datos recibidos: %.*s\n", ret, buf);
    }

    // Responder al cliente que los datos fueron recibidos
    const char* resp_str = "Datos recibidos";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
};




// Configurar el servidor web
// httpd_handle_t start_webserver(void) {
//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//     httpd_handle_t server = NULL;
//
//     if (httpd_start(&server, &config) == ESP_OK) {
//         httpd_uri_t uri_get = {
//             .uri       = "/",
//             .method    = HTTP_GET,
//             .handler   = hello_get_handler,
//             .user_ctx  = NULL
//         };
//         httpd_register_uri_handler(server, &uri_get);
//
//         httpd_uri_t uri_post = {
//             .uri       = "/post",
//             .method    = HTTP_POST,
//             .handler   = hello_post_handler,
//             .user_ctx  = NULL
//         };
//         httpd_register_uri_handler(server, &uri_post);
//     }
//     return server;
// }
//
void start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Iniciar el servidor web
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &hello);
    }
}


void app_main(void)
{
    ap_init();
    start_webserver();
// httpd_handle_t server = start_webserver();
}
